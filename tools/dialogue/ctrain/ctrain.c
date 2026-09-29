/* ctrain: train the native 5M dialogue-choice model in ISO C11.
 *
 * It reproduces train_meaning.py without Python or PyTorch: the same
 * architecture (RMSNorm, fused QKV, interleaved RoPE, SwiGLU, tied embedding),
 * the same last-position loss over the legal candidate IDs, AdamW with cosine
 * decay and global-norm clipping, and it writes the same CCOREV2 container
 * that src/story/cc_core_model.c loads.
 *
 *   ctrain train --reference REF.ccv2 --train T --dev D --test E --out DIR
 *   ctrain eval  --model M.ccv2 --data FILE [--picks]
 *
 * Data files are written by export_c_dataset.py: one row per line,
 *   <intent> <target> <count> <token>...
 * The prefix must hold the choice marker 1580, the candidate IDs 1024+k and
 * the closing token 1281, exactly as CcCoreModelBeginPolicy expects.
 *
 * Only the tensors the policy path reads are trained: the token embedding, the
 * six always-on situation and company rows (constant across positions when meta
 * is zero), the blocks and the final norm. Every other tensor of the container
 * is written as zeros, which the native loader accepts.
 */
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef CT_REAL_DOUBLE
typedef double real;
#define rexp exp
#define rsqrt sqrt
#define rlog log
#define rcos cos
#define rsin sin
#define rpow pow
#else
typedef float real;
#define rexp expf
#define rsqrt sqrtf
#define rlog logf
#define rcos cosf
#define rsin sinf
#define rpow powf
#endif

#if defined(__APPLE__) && !defined(CT_NO_BLAS) && !defined(CT_REAL_DOUBLE)
#include <Accelerate/Accelerate.h>
#define CT_BLAS 1
#endif

#define MARKER 1580
#define CLOSE 1281
#define CAND0 1024
#define MAXCAND 16
#define NCONST 6
#define RMS_EPS 1.1920929e-07

typedef struct { int V, D, L, H, FF, CTX; } Config;

/* ---------- parameter layout ---------- */
typedef struct { size_t n1, n2, qkv, out, up, down; } LayerOff;
typedef struct {
    Config c; size_t count;
    size_t emb, cst, norm; LayerOff *layer;
    real *w, *g, *m, *v;
} Model;

static size_t layout(Model *md)
{
    const Config *c = &md->c; size_t at = 0;
    md->emb = at; at += (size_t)c->V * c->D;
    md->cst = at; at += (size_t)NCONST * c->D;
    md->layer = calloc((size_t)c->L, sizeof(LayerOff));
    for (int l = 0; l < c->L; ++l) {
        LayerOff *o = &md->layer[l];
        o->n1 = at; at += c->D; o->n2 = at; at += c->D;
        o->qkv = at; at += (size_t)3 * c->D * c->D;
        o->out = at; at += (size_t)c->D * c->D;
        o->up = at; at += (size_t)2 * c->FF * c->D;
        o->down = at; at += (size_t)c->D * c->FF;
    }
    md->norm = at; at += c->D;
    md->count = at; return at;
}

/* ---------- random ---------- */
typedef struct { uint64_t s; } Rng;
static uint64_t rnext(Rng *r)
{
    uint64_t z = (r->s += UINT64_C(0x9e3779b97f4a7c15));
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    return z ^ (z >> 31);
}
static double runi(Rng *r) { return ((double)(rnext(r) >> 11) + 0.5) / 9007199254740992.0; }
static double rnorm(Rng *r)
{
    return sqrt(-2.0 * log(runi(r))) * cos(6.283185307179586 * runi(r));
}

static void model_alloc(Model *md, const Config *c, int grads)
{
    md->c = *c; layout(md);
    md->w = calloc(md->count, sizeof(real));
    md->g = grads ? calloc(md->count, sizeof(real)) : NULL;
    md->m = grads ? calloc(md->count, sizeof(real)) : NULL;
    md->v = grads ? calloc(md->count, sizeof(real)) : NULL;
    if (!md->w || (grads && (!md->g || !md->m || !md->v))) { fputs("out of memory\n", stderr); exit(2); }
}
static void model_free(Model *md) { free(md->w); free(md->g); free(md->m); free(md->v); free(md->layer); }

static void model_init(Model *md, uint64_t seed)
{
    Rng r = {seed}; const Config *c = &md->c;
    for (size_t i = 0; i < md->count; ++i) md->w[i] = (real)(0.02 * rnorm(&r));
    for (int l = 0; l < c->L; ++l)
        for (int i = 0; i < c->D; ++i) md->w[md->layer[l].n1 + i] = md->w[md->layer[l].n2 + i] = 1;
    for (int i = 0; i < c->D; ++i) md->w[md->norm + i] = 1;
}

/* ---------- matrix products (row-major) ----------
 * C[M,N] = op(A)[M,K] * op(B)[K,N] + beta * C */
static void gemm(int ta, int tb, int M, int N, int K, const real *A, int lda,
                 const real *B, int ldb, real *C, int ldc, real beta)
{
#ifdef CT_BLAS
    cblas_sgemm(CblasRowMajor, ta ? CblasTrans : CblasNoTrans, tb ? CblasTrans : CblasNoTrans,
                M, N, K, 1.0f, A, lda, B, ldb, beta, C, ldc);
#else
    if (beta == 0) for (int i = 0; i < M; ++i) memset(C + (size_t)i * ldc, 0, (size_t)N * sizeof(real));
    else if (beta != 1) for (int i = 0; i < M; ++i) for (int j = 0; j < N; ++j) C[(size_t)i * ldc + j] *= beta;
    if (!ta && tb) {
        for (int i = 0; i < M; ++i) for (int j = 0; j < N; ++j) {
            real s = 0; const real *a = A + (size_t)i * lda, *b = B + (size_t)j * ldb;
            for (int k = 0; k < K; ++k) s += a[k] * b[k];
            C[(size_t)i * ldc + j] += s;
        }
    } else if (!ta && !tb) {
        for (int i = 0; i < M; ++i) for (int k = 0; k < K; ++k) {
            real a = A[(size_t)i * lda + k]; const real *b = B + (size_t)k * ldb; real *c = C + (size_t)i * ldc;
            for (int j = 0; j < N; ++j) c[j] += a * b[j];
        }
    } else if (ta && !tb) {
        for (int k = 0; k < K; ++k) for (int i = 0; i < M; ++i) {
            real a = A[(size_t)k * lda + i]; const real *b = B + (size_t)k * ldb; real *c = C + (size_t)i * ldc;
            for (int j = 0; j < N; ++j) c[j] += a * b[j];
        }
    } else {
        for (int i = 0; i < M; ++i) for (int j = 0; j < N; ++j) {
            real s = 0;
            for (int k = 0; k < K; ++k) s += A[(size_t)k * lda + i] * B[(size_t)j * ldb + k];
            C[(size_t)i * ldc + j] += s;
        }
    }
#endif
}

/* ---------- per-sequence work ---------- */
typedef struct {
    real *x, *n1, *inv1, *qkv, *prob, *att, *xm, *n2, *inv2, *up, *feed;
} Cache;
typedef struct {
    const Model *md; int T;          /* capacity */
    real *cs, *sn;                    /* rope tables [CTX][HD/2] */
    Cache *cache; real *xf, *hlast, invf;
    real *dx, *dxm, *dn, *dup, *dfeed, *datt, *dqkv, *dp, *ds, *tmp;
    real *g;                          /* private gradient buffer */
} Work;

static void rope_tables(const Config *c, real *cs, real *sn)
{
    int hd = c->D / c->H;
    for (int t = 0; t < c->CTX; ++t) for (int i = 0; i < hd / 2; ++i) {
        real angle = (real)t * rpow((real)10000, -(real)(2 * i) / (real)hd);
        cs[t * (hd / 2) + i] = rcos(angle); sn[t * (hd / 2) + i] = rsin(angle);
    }
}

static Work *work_new(const Model *md, int T, int grads)
{
    const Config *c = &md->c; Work *w = calloc(1, sizeof(*w));
    size_t D = (size_t)c->D, FF = (size_t)c->FF, H = (size_t)c->H;
    w->md = md; w->T = T;
    w->cs = malloc((size_t)c->CTX * (D / H / 2) * sizeof(real)); w->sn = malloc((size_t)c->CTX * (D / H / 2) * sizeof(real));
    rope_tables(c, w->cs, w->sn);
    w->cache = calloc((size_t)c->L, sizeof(Cache));
    for (int l = 0; l < c->L; ++l) {
        Cache *k = &w->cache[l];
        k->x = malloc(T * D * sizeof(real)); k->n1 = malloc(T * D * sizeof(real)); k->inv1 = malloc(T * sizeof(real));
        k->qkv = malloc(T * 3 * D * sizeof(real)); k->prob = malloc(H * (size_t)T * T * sizeof(real));
        k->att = malloc(T * D * sizeof(real)); k->xm = malloc(T * D * sizeof(real));
        k->n2 = malloc(T * D * sizeof(real)); k->inv2 = malloc(T * sizeof(real));
        k->up = malloc(T * 2 * FF * sizeof(real)); k->feed = malloc(T * FF * sizeof(real));
    }
    w->xf = malloc(T * D * sizeof(real)); w->hlast = malloc(D * sizeof(real));
    w->dx = malloc(T * D * sizeof(real)); w->dxm = malloc(T * D * sizeof(real)); w->dn = malloc(T * D * sizeof(real));
    w->dup = malloc(T * 2 * FF * sizeof(real)); w->dfeed = malloc(T * FF * sizeof(real));
    w->datt = malloc(T * D * sizeof(real)); w->dqkv = malloc(T * 3 * D * sizeof(real));
    w->dp = malloc((size_t)T * T * sizeof(real)); w->ds = malloc((size_t)T * T * sizeof(real));
    w->tmp = malloc(T * D * sizeof(real));
    w->g = grads ? calloc(md->count, sizeof(real)) : NULL;
    return w;
}
static void work_free(Work *w)
{
    for (int l = 0; l < w->md->c.L; ++l) {
        Cache *k = &w->cache[l];
        free(k->x); free(k->n1); free(k->inv1); free(k->qkv); free(k->prob); free(k->att);
        free(k->xm); free(k->n2); free(k->inv2); free(k->up); free(k->feed);
    }
    free(w->cache); free(w->cs); free(w->sn); free(w->xf); free(w->hlast); free(w->dx); free(w->dxm);
    free(w->dn); free(w->dup); free(w->dfeed); free(w->datt); free(w->dqkv); free(w->dp); free(w->ds);
    free(w->tmp); free(w->g); free(w);
}

static void rms_fwd(const real *x, const real *wt, real *y, real *inv, int rows, int D)
{
    for (int t = 0; t < rows; ++t) {
        real s = 0; const real *r = x + (size_t)t * D;
        for (int i = 0; i < D; ++i) s += r[i] * r[i];
        real iv = (real)1 / rsqrt(s / (real)D + (real)RMS_EPS);
        if (inv) inv[t] = iv;
        for (int i = 0; i < D; ++i) y[(size_t)t * D + i] = r[i] * iv * wt[i];
    }
}
/* dx += ..., dw += ... */
static void rms_bwd(const real *x, const real *wt, const real *inv, const real *dy, real *dx, real *dw, int rows, int D)
{
    for (int t = 0; t < rows; ++t) {
        const real *r = x + (size_t)t * D, *d = dy + (size_t)t * D; real iv = inv[t], dot = 0;
        for (int i = 0; i < D; ++i) { dw[i] += d[i] * r[i] * iv; dot += d[i] * wt[i] * r[i]; }
        real k = iv * iv * iv * dot / (real)D;
        for (int i = 0; i < D; ++i) dx[(size_t)t * D + i] += d[i] * wt[i] * iv - r[i] * k;
    }
}

static void rope(const Work *w, real *qkv, int T, int sign)
{
    const Config *c = &w->md->c; int hd = c->D / c->H, D = c->D;
    for (int t = 0; t < T; ++t) for (int part = 0; part < 2; ++part) for (int h = 0; h < c->H; ++h)
        for (int i = 0; i < hd / 2; ++i) {
            real *p = qkv + (size_t)t * 3 * D + (size_t)part * D + h * hd + 2 * i;
            real co = w->cs[t * (hd / 2) + i], si = sign * w->sn[t * (hd / 2) + i], a = p[0], b = p[1];
            p[0] = a * co - b * si; p[1] = a * si + b * co;
        }
}

static void const_bias(const Model *md, real *b)
{
    for (int j = 0; j < md->c.D; ++j) {
        real s = 0; for (int k = 0; k < NCONST; ++k) s += md->w[md->cst + (size_t)k * md->c.D + j];
        b[j] = s;
    }
}

/* Forward over the whole prefix; returns the last hidden state in w->hlast. */
static void forward(Work *w, const int *tok, int T)
{
    const Model *md = w->md; const Config *c = &md->c; int D = c->D, FF = c->FF, H = c->H, hd = D / H;
    real bias[512]; const_bias(md, bias);
    real *x = w->cache[0].x;
    for (int t = 0; t < T; ++t) for (int j = 0; j < D; ++j) x[(size_t)t * D + j] = md->w[md->emb + (size_t)tok[t] * D + j] + bias[j];
    real scale = (real)1 / rsqrt((real)hd);
    for (int l = 0; l < c->L; ++l) {
        const LayerOff *o = &md->layer[l]; Cache *k = &w->cache[l];
        rms_fwd(k->x, md->w + o->n1, k->n1, k->inv1, T, D);
        gemm(0, 1, T, 3 * D, D, k->n1, D, md->w + o->qkv, D, k->qkv, 3 * D, 0);
        rope(w, k->qkv, T, 1);
        for (int h = 0; h < H; ++h) {
            real *P = k->prob + (size_t)h * T * T;
            for (int t = 0; t < T; ++t) {
                const real *q = k->qkv + (size_t)t * 3 * D + h * hd; real mx = -1e30f, sum = 0;
                for (int s = 0; s <= t; ++s) {
                    const real *kk = k->qkv + (size_t)s * 3 * D + D + h * hd; real d = 0;
                    for (int i = 0; i < hd; ++i) d += q[i] * kk[i];
                    P[(size_t)t * T + s] = d * scale; if (d * scale > mx) mx = d * scale;
                }
                for (int s = 0; s <= t; ++s) { P[(size_t)t * T + s] = rexp(P[(size_t)t * T + s] - mx); sum += P[(size_t)t * T + s]; }
                real *out = k->att + (size_t)t * D + h * hd;
                for (int i = 0; i < hd; ++i) out[i] = 0;
                for (int s = 0; s <= t; ++s) {
                    real p = P[(size_t)t * T + s] / sum; P[(size_t)t * T + s] = p;
                    const real *v = k->qkv + (size_t)s * 3 * D + 2 * D + h * hd;
                    for (int i = 0; i < hd; ++i) out[i] += p * v[i];
                }
                for (int s = t + 1; s < T; ++s) P[(size_t)t * T + s] = 0;
            }
        }
        gemm(0, 1, T, D, D, k->att, D, md->w + o->out, D, k->xm, D, 0);
        for (size_t i = 0; i < (size_t)T * D; ++i) k->xm[i] += k->x[i];
        rms_fwd(k->xm, md->w + o->n2, k->n2, k->inv2, T, D);
        gemm(0, 1, T, 2 * FF, D, k->n2, D, md->w + o->up, D, k->up, 2 * FF, 0);
        for (int t = 0; t < T; ++t) for (int j = 0; j < FF; ++j) {
            real g = k->up[(size_t)t * 2 * FF + j], v = k->up[(size_t)t * 2 * FF + FF + j];
            k->feed[(size_t)t * FF + j] = g / ((real)1 + rexp(-g)) * v;
        }
        real *next = l + 1 < c->L ? w->cache[l + 1].x : w->xf;
        gemm(0, 1, T, D, FF, k->feed, FF, md->w + o->down, FF, next, D, 0);
        for (size_t i = 0; i < (size_t)T * D; ++i) next[i] += k->xm[i];
    }
    real inv;
    rms_fwd(w->xf + (size_t)(T - 1) * D, md->w + md->norm, w->hlast, &inv, 1, D);
    w->invf = inv;
}

typedef struct { int n; int id[MAXCAND]; } Legal;
static int legal_of(const int *tok, int T, Legal *lg)
{
    int at = -1; lg->n = 0;
    for (int i = 0; i < T; ++i) if (tok[i] == MARKER) { at = i; break; }
    if (at < 0 || tok[T - 1] != CLOSE) return 0;
    for (int i = at + 1; i < T - 1; ++i) {
        int k = tok[i] - CAND0;
        if (k < 0 || k >= MAXCAND || lg->n >= MAXCAND) return 0;
        lg->id[lg->n++] = k;
    }
    return lg->n > 0;
}

static void logits_of(const Work *w, const Legal *lg, real *z)
{
    const Model *md = w->md; int D = md->c.D;
    for (int a = 0; a < lg->n; ++a) {
        const real *e = md->w + md->emb + (size_t)(CAND0 + lg->id[a]) * D; real s = 0;
        for (int j = 0; j < D; ++j) s += w->hlast[j] * e[j];
        z[a] = s;
    }
}

/* Forward and backward for one row; accumulates into w->g. Returns loss. */
static double step_row(Work *w, const int *tok, int T, int target)
{
    const Model *md = w->md; const Config *c = &md->c; int D = c->D, FF = c->FF, H = c->H, hd = D / H;
    Legal lg; real z[MAXCAND], dz[MAXCAND];
    if (!legal_of(tok, T, &lg) || target < 0 || target >= lg.n) { fputs("bad row\n", stderr); exit(2); }
    forward(w, tok, T);
    logits_of(w, &lg, z);
    real mx = z[0]; for (int a = 1; a < lg.n; ++a) if (z[a] > mx) mx = z[a];
    real sum = 0; for (int a = 0; a < lg.n; ++a) { dz[a] = rexp(z[a] - mx); sum += dz[a]; }
    double loss = -(double)(z[target] - mx - rlog(sum));
    for (int a = 0; a < lg.n; ++a) dz[a] = dz[a] / sum - (a == target);
    real *g = w->g; real dh[512];
    for (int j = 0; j < D; ++j) dh[j] = 0;
    for (int a = 0; a < lg.n; ++a) {
        size_t row = md->emb + (size_t)(CAND0 + lg.id[a]) * D;
        for (int j = 0; j < D; ++j) { dh[j] += dz[a] * md->w[row + j]; g[row + j] += dz[a] * w->hlast[j]; }
    }
    memset(w->dx, 0, (size_t)T * D * sizeof(real));
    rms_bwd(w->xf + (size_t)(T - 1) * D, md->w + md->norm, &w->invf, dh, w->dx + (size_t)(T - 1) * D, g + md->norm, 1, D);
    real scale = (real)1 / rsqrt((real)hd);
    for (int l = c->L - 1; l >= 0; --l) {
        const LayerOff *o = &md->layer[l]; Cache *k = &w->cache[l];
        /* dx is the gradient of the block output. */
        memcpy(w->dxm, w->dx, (size_t)T * D * sizeof(real));
        gemm(0, 0, T, FF, D, w->dx, D, md->w + o->down, FF, w->dfeed, FF, 0);
        gemm(1, 0, D, FF, T, w->dx, D, k->feed, FF, g + o->down, FF, 1);
        for (int t = 0; t < T; ++t) for (int j = 0; j < FF; ++j) {
            real gt = k->up[(size_t)t * 2 * FF + j], v = k->up[(size_t)t * 2 * FF + FF + j];
            real sg = (real)1 / ((real)1 + rexp(-gt)), df = w->dfeed[(size_t)t * FF + j];
            w->dup[(size_t)t * 2 * FF + j] = df * v * (sg + gt * sg * ((real)1 - sg));
            w->dup[(size_t)t * 2 * FF + FF + j] = df * gt * sg;
        }
        gemm(0, 0, T, D, 2 * FF, w->dup, 2 * FF, md->w + o->up, D, w->dn, D, 0);
        gemm(1, 0, 2 * FF, D, T, w->dup, 2 * FF, k->n2, D, g + o->up, D, 1);
        rms_bwd(k->xm, md->w + o->n2, k->inv2, w->dn, w->dxm, g + o->n2, T, D);
        /* attention output projection */
        gemm(0, 0, T, D, D, w->dxm, D, md->w + o->out, D, w->datt, D, 0);
        gemm(1, 0, D, D, T, w->dxm, D, k->att, D, g + o->out, D, 1);
        memset(w->dqkv, 0, (size_t)T * 3 * D * sizeof(real));
        for (int h = 0; h < H; ++h) {
            const real *P = k->prob + (size_t)h * T * T;
            for (int t = 0; t < T; ++t) {
                const real *da = w->datt + (size_t)t * D + h * hd; real dot = 0;
                for (int s = 0; s <= t; ++s) {
                    const real *v = k->qkv + (size_t)s * 3 * D + 2 * D + h * hd; real d = 0;
                    for (int i = 0; i < hd; ++i) d += da[i] * v[i];
                    w->dp[(size_t)t * T + s] = d; dot += P[(size_t)t * T + s] * d;
                    real *dv = w->dqkv + (size_t)s * 3 * D + 2 * D + h * hd;
                    for (int i = 0; i < hd; ++i) dv[i] += P[(size_t)t * T + s] * da[i];
                }
                for (int s = 0; s <= t; ++s) {
                    real ds = P[(size_t)t * T + s] * (w->dp[(size_t)t * T + s] - dot) * scale;
                    const real *q = k->qkv + (size_t)t * 3 * D + h * hd, *kk = k->qkv + (size_t)s * 3 * D + D + h * hd;
                    real *dq = w->dqkv + (size_t)t * 3 * D + h * hd, *dk = w->dqkv + (size_t)s * 3 * D + D + h * hd;
                    for (int i = 0; i < hd; ++i) { dq[i] += ds * kk[i]; dk[i] += ds * q[i]; }
                }
            }
        }
        rope(w, w->dqkv, T, -1);
        gemm(0, 0, T, D, 3 * D, w->dqkv, 3 * D, md->w + o->qkv, D, w->dn, D, 0);
        gemm(1, 0, 3 * D, D, T, w->dqkv, 3 * D, k->n1, D, g + o->qkv, D, 1);
        memcpy(w->dx, w->dxm, (size_t)T * D * sizeof(real));
        rms_bwd(k->x, md->w + o->n1, k->inv1, w->dn, w->dx, g + o->n1, T, D);
    }
    for (int t = 0; t < T; ++t) {
        real *e = g + md->emb + (size_t)tok[t] * D;
        for (int j = 0; j < D; ++j) {
            e[j] += w->dx[(size_t)t * D + j];
            for (int q = 0; q < NCONST; ++q) g[md->cst + (size_t)q * D + j] += w->dx[(size_t)t * D + j];
        }
    }
    return loss;
}

/* ---------- SHA-256 ---------- */
typedef struct { uint32_t h[8]; uint64_t len; unsigned char buf[64]; size_t fill; } Sha;
static const uint32_t K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha_block(Sha *s, const unsigned char *p)
{
    uint32_t w[64], a, b, c, d, e, f, g, h;
    for (int i = 0; i < 16; ++i) w[i] = (uint32_t)p[4*i] << 24 | (uint32_t)p[4*i+1] << 16 | (uint32_t)p[4*i+2] << 8 | p[4*i+3];
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = ROR(w[i-15], 7) ^ ROR(w[i-15], 18) ^ (w[i-15] >> 3), s1 = ROR(w[i-2], 17) ^ ROR(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
        uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}
static void sha_init(Sha *s)
{
    static const uint32_t iv[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    memcpy(s->h, iv, sizeof(iv)); s->len = 0; s->fill = 0;
}
static void sha_add(Sha *s, const void *data, size_t n)
{
    const unsigned char *p = data; s->len += n;
    while (n) {
        size_t take = 64 - s->fill; if (take > n) take = n;
        memcpy(s->buf + s->fill, p, take); s->fill += take; p += take; n -= take;
        if (s->fill == 64) { sha_block(s, s->buf); s->fill = 0; }
    }
}
static void sha_hex(Sha *s, char out[65])
{
    uint64_t bits = s->len * 8; unsigned char pad = 0x80, zero = 0, len[8];
    sha_add(s, &pad, 1); while (s->fill != 56) sha_add(s, &zero, 1);
    for (int i = 0; i < 8; ++i) len[i] = (unsigned char)(bits >> (56 - 8 * i));
    sha_add(s, len, 8);
    for (int i = 0; i < 8; ++i) snprintf(out + 8 * i, 9, "%08x", s->h[i]);
}

/* ---------- the CCOREV2 container ---------- */
typedef struct { char name[64]; int rows, cols; int trained; size_t src; } Slot;  /* cols==0: 1-D */

/* Canonical state_dict order of Crownless(...) for this config. */
static int slots_of(const Model *md, Slot *s)
{
    const Config *c = &md->c; int n = 0, D = c->D;
#define ADD(nm, r, cc, tr, off) do { snprintf(s[n].name, 64, "%s", nm); s[n].rows = r; s[n].cols = cc; s[n].trained = tr; s[n].src = off; ++n; } while (0)
    ADD("embedding.weight", c->V, D, 1, md->emb);
    ADD("roles.weight", 16, D, 0, 0); ADD("knowledge.weight", 4, D, 0, 0); ADD("provenance.weight", 4, D, 0, 0);
    ADD("events.weight", 4, D, 0, 0); ADD("kinds.weight", 62, D, 0, 0); ADD("voices.weight", 16, D, 0, 0);
    ADD("goals.weight", 8, D, 0, 0); ADD("stresses.weight", 4, D, 0, 0); ADD("courages.weight", 4, D, 0, 0);
    static const char *cn[NCONST] = {"hungry", "sheltered", "in_transit", "owes", "trusts", "far"};
    static const int rows[NCONST] = {2, 2, 2, 2, 2, 4};
    (void)rows;
    /* Order in the container: hungry, sheltered, in_transit, owes, trusts, faction, far. */
    for (int q = 0; q < 5; ++q) { char nm[64]; snprintf(nm, 64, "%s.weight", cn[q]); ADD(nm, 2, D, 2 + q, md->cst + (size_t)q * D); }
    ADD("faction.weight", 4, D, 0, 0);
    ADD("far.weight", 2, D, 2 + 5, md->cst + (size_t)5 * D);
    for (int l = 0; l < c->L; ++l) {
        const LayerOff *o = &md->layer[l]; char nm[64];
        snprintf(nm, 64, "blocks.%d.n1.weight", l); ADD(nm, D, 0, 1, o->n1);
        snprintf(nm, 64, "blocks.%d.n2.weight", l); ADD(nm, D, 0, 1, o->n2);
        snprintf(nm, 64, "blocks.%d.qkv.weight", l); ADD(nm, 3 * D, D, 1, o->qkv);
        snprintf(nm, 64, "blocks.%d.out.weight", l); ADD(nm, D, D, 1, o->out);
        snprintf(nm, 64, "blocks.%d.up.weight", l); ADD(nm, 2 * c->FF, D, 1, o->up);
        snprintf(nm, 64, "blocks.%d.down.weight", l); ADD(nm, D, c->FF, 1, o->down);
    }
    ADD("norm.weight", D, 0, 1, md->norm);
    ADD("copy_start.weight", D, D, 0, 0); ADD("copy_end.weight", D, D, 0, 0);
    ADD("copy_gate.weight", 1, D, 0, 0); ADD("copy_gate.bias", 1, 0, 0, 0);
#undef ADD
    return n;
}
/* trained: 1 = plain copy of a flat range, >=2 = constant row 0 of a two-row table (row 1 stays zero) */

static size_t slot_bytes(const Slot *s, size_t *scales)
{
    if (s->cols) { size_t data = (size_t)s->rows * s->cols; if (scales) *scales = data; return data + (size_t)s->rows * 4; }
    if (scales) *scales = 0; return (size_t)s->rows * 4;
}

static int read_all(const char *path, unsigned char **out, size_t *n)
{
    FILE *f = fopen(path, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    *out = malloc((size_t)size + 1); *n = fread(*out, 1, (size_t)size, f); fclose(f);
    return *n == (size_t)size;
}

/* Find "key": in a compact JSON object text; returns pointer to its value. */
static const char *json_key(const char *text, size_t n, const char *key)
{
    char pat[80]; snprintf(pat, sizeof(pat), "\"%s\":", key);
    size_t k = strlen(pat);
    for (size_t i = 0; i + k <= n; ++i) if (memcmp(text + i, pat, k) == 0) return text + i + k;
    return NULL;
}
static int json_int(const char *text, size_t n, const char *key)
{
    const char *p = json_key(text, n, key); return p ? atoi(p) : -1;
}

typedef struct { unsigned char *raw; size_t size, header_at, header_len, payload_at; } Container;

static int container_open(const char *path, Container *ct, Config *c)
{
    memset(ct, 0, sizeof(*ct));
    if (!read_all(path, &ct->raw, &ct->size) || ct->size < 12 || memcmp(ct->raw, "CCOREV2\0", 8)) return 0;
    uint32_t len = (uint32_t)ct->raw[8] | (uint32_t)ct->raw[9] << 8 | (uint32_t)ct->raw[10] << 16 | (uint32_t)ct->raw[11] << 24;
    if (len > 1000000u || len > ct->size - 12) return 0;
    ct->header_at = 12; ct->header_len = len; ct->payload_at = 12 + len;
    const char *h = (const char *)ct->raw + 12;
    const char *cfg = json_key(h, len, "config"); if (!cfg) return 0;
    size_t cl = 0; while (cfg[cl] && cfg[cl] != '}') ++cl;
    c->D = json_int(cfg, cl, "dim"); c->L = json_int(cfg, cl, "layers"); c->H = json_int(cfg, cl, "heads");
    c->FF = json_int(cfg, cl, "ff"); c->V = json_int(cfg, cl, "vocab"); c->CTX = json_int(cfg, cl, "context");
    return c->D > 0 && c->L > 0 && c->H > 0 && c->FF > 0 && c->V > CAND0 + MAXCAND && c->CTX > 0 && c->D % c->H == 0 && c->D <= 512;
}

/* Verify the container's tensor table equals the layout this trainer writes. */
static int container_layout_ok(const Container *ct, const Model *md)
{
    Slot slots[128]; int n = slots_of(md, slots);
    const char *h = (const char *)ct->raw + ct->header_at; size_t hl = ct->header_len;
    const char *tp = json_key(h, hl, "tensors"); if (!tp) return 0;
    size_t off = 0; char want[512];
    for (int i = 0; i < n; ++i) {
        size_t sc, bytes = slot_bytes(&slots[i], &sc);
        if (slots[i].cols) snprintf(want, sizeof(want), "{\"dtype\":\"i8\",\"name\":\"%s\",\"offset\":%zu,\"scales\":%zu,\"shape\":[%d,%d]}",
                                    slots[i].name, off, off + sc, slots[i].rows, slots[i].cols);
        else snprintf(want, sizeof(want), "{\"dtype\":\"f32\",\"name\":\"%s\",\"offset\":%zu,\"shape\":[%d]}", slots[i].name, off, slots[i].rows);
        size_t k = strlen(want);
        tp += (i ? 1 : 1);      /* skip '[' or ',' */
        if (tp + k > h + hl || memcmp(tp, want, k)) return 0;
        tp += k; off += bytes;
    }
    return *tp == ']' && ct->size - ct->payload_at == off;
}

static float f32le(const unsigned char *p)
{
    uint32_t b = (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; float f; memcpy(&f, &b, 4); return f;
}

/* Copy container weights into the model (constant rows take row 0). */
static void container_load(const Container *ct, Model *md)
{
    Slot slots[128]; int n = slots_of(md, slots); size_t off = 0;
    for (int i = 0; i < n; ++i) {
        size_t sc, bytes = slot_bytes(&slots[i], &sc); const unsigned char *p = ct->raw + ct->payload_at + off;
        if (slots[i].trained && slots[i].cols) {
            size_t count = (size_t)slots[i].rows * slots[i].cols; if (slots[i].trained >= 2) count = slots[i].cols;
            for (size_t j = 0; j < count; ++j) {
                float sv = f32le(p + sc + 4 * (j / slots[i].cols));
                md->w[slots[i].src + j] = (real)((int)(signed char)p[j] * sv);
            }
        } else if (slots[i].trained && !slots[i].cols)
            for (int j = 0; j < slots[i].rows; ++j) md->w[slots[i].src + j] = (real)f32le(p + 4 * j);
        off += bytes;
    }
}

static void put_f32(unsigned char *p, float v) { uint32_t b; memcpy(&b, &v, 4); p[0] = (unsigned char)b; p[1] = (unsigned char)(b >> 8); p[2] = (unsigned char)(b >> 16); p[3] = (unsigned char)(b >> 24); }

/* Write a container: the reference header with a new payload hash. */
static int container_save(const Container *ref, const Model *md, const char *path)
{
    Slot slots[128]; int n = slots_of(md, slots); size_t total = 0;
    for (int i = 0; i < n; ++i) total += slot_bytes(&slots[i], NULL);
    unsigned char *pay = calloc(total, 1); size_t off = 0;
    for (int i = 0; i < n; ++i) {
        size_t sc, bytes = slot_bytes(&slots[i], &sc); unsigned char *p = pay + off;
        if (!slots[i].trained) { /* zeros with a valid positive scale */
            for (int r = 0; slots[i].cols && r < slots[i].rows; ++r) put_f32(p + sc + 4 * r, 1e-12f);
        } else if (slots[i].cols) {
            for (int r = 0; r < slots[i].rows; ++r) {
                float mx = 0; int live = slots[i].trained == 1 || r == 0;
                const real *row = md->w + slots[i].src + (size_t)r * slots[i].cols;
                if (live) for (int j = 0; j < slots[i].cols; ++j) { float a = fabsf((float)row[j]); if (a > mx) mx = a; }
                float scale = mx / 127.0f; if (scale < 1e-12f) scale = 1e-12f;
                for (int j = 0; j < slots[i].cols; ++j) {
                    float q = live ? nearbyintf((float)row[j] / scale) : 0; if (q > 127) q = 127; if (q < -127) q = -127;
                    p[(size_t)r * slots[i].cols + j] = (unsigned char)(signed char)q;
                }
                put_f32(p + sc + 4 * r, scale);
            }
        } else for (int j = 0; j < slots[i].rows; ++j) put_f32(p + 4 * j, (float)md->w[slots[i].src + j]);
        off += bytes;
    }
    Sha s; char hex[65]; sha_init(&s); sha_add(&s, pay, total); sha_hex(&s, hex);
    unsigned char *head = malloc(ref->header_len); memcpy(head, ref->raw + ref->header_at, ref->header_len);
    const char *pp = json_key((const char *)head, ref->header_len, "payload_sha256");
    if (!pp || pp[0] != '"') { free(pay); free(head); return 0; }
    memcpy((char *)pp + 1, hex, 64);
    FILE *f = fopen(path, "wb"); if (!f) { free(pay); free(head); return 0; }
    fwrite(ref->raw, 1, 12, f); fwrite(head, 1, ref->header_len, f); fwrite(pay, 1, total, f);
    int ok = fclose(f) == 0; free(pay); free(head); return ok;
}

/* ---------- data ---------- */
typedef struct { int n, intents; int *intent, *target, *count, **tok; int maxlen; } Data;

static int data_load(const char *path, Data *d)
{
    FILE *f = fopen(path, "r"); if (!f) return 0;
    int cap = 1024; d->n = 0; d->maxlen = 0; d->intents = 0;
    d->intent = malloc(cap * sizeof(int)); d->target = malloc(cap * sizeof(int));
    d->count = malloc(cap * sizeof(int)); d->tok = malloc(cap * sizeof(int *));
    for (;;) {
        int a, b, c; if (fscanf(f, "%d %d %d", &a, &b, &c) != 3) break;
        if (c < 3 || c > 512) { fclose(f); return 0; }
        if (d->n == cap) {
            cap *= 2; d->intent = realloc(d->intent, cap * sizeof(int)); d->target = realloc(d->target, cap * sizeof(int));
            d->count = realloc(d->count, cap * sizeof(int)); d->tok = realloc(d->tok, cap * sizeof(int *));
        }
        d->tok[d->n] = malloc(c * sizeof(int));
        for (int i = 0; i < c; ++i) if (fscanf(f, "%d", &d->tok[d->n][i]) != 1) { fclose(f); return 0; }
        d->intent[d->n] = a; d->target[d->n] = b; d->count[d->n] = c;
        if (c > d->maxlen) d->maxlen = c; if (a + 1 > d->intents) d->intents = a + 1; ++d->n;
    }
    fclose(f); return d->n > 0;
}

static int data_fits(const Data *d, int vocab)
{
    for (int i = 0; i < d->n; ++i) for (int j = 0; j < d->count[i]; ++j)
        if (d->tok[i][j] < 0 || d->tok[i][j] >= vocab) return 0;
    return 1;
}

static void data_free(Data *d) { for (int i = 0; i < d->n; ++i) free(d->tok[i]); free(d->tok); free(d->intent); free(d->target); free(d->count); }

/* Greedy argmax over the legal candidates. Returns the chosen index. */
static int pick(Work *w, const int *tok, int T)
{
    Legal lg; real z[MAXCAND];
    if (!legal_of(tok, T, &lg)) return -1;
    forward(w, tok, T); logits_of(w, &lg, z);
    int best = 0; for (int a = 1; a < lg.n; ++a) if (z[a] > z[best]) best = a;
    return best;
}

static void evaluate(const Model *md, const Data *d, const char *name, int picks, int *correct_out)
{
    Work *w = work_new(md, d->maxlen, 0); int ok = 0;
    for (int i = 0; i < d->n; ++i) {
        int p = pick(w, d->tok[i], d->count[i]); ok += p == d->target[i];
        if (picks) printf("%d\n", p);
    }
    fprintf(stderr, "%s: %d/%d correct (%.2f%%)\n", name, ok, d->n, 100.0 * ok / d->n);
    if (correct_out) *correct_out = ok;
    work_free(w);
}

#ifndef CTRAIN_NO_MAIN
/* ---------- training ---------- */
typedef struct {
    Work *w; const Data *d; const int *rows; int from, stride, count; double loss;
} Job;
static void *run_job(void *arg)
{
    Job *j = arg; j->loss = 0;
    for (int i = j->from; i < j->count; i += j->stride)
        j->loss += step_row(j->w, j->d->tok[j->rows[i]], j->d->count[j->rows[i]], j->d->target[j->rows[i]]);
    return NULL;
}

static const char *arg_of(int argc, char **argv, const char *name, const char *fallback)
{
    for (int i = 2; i + 1 < argc; ++i) if (!strcmp(argv[i], name)) return argv[i + 1];
    return fallback;
}
static int has_flag(int argc, char **argv, const char *name)
{
    for (int i = 2; i < argc; ++i) if (!strcmp(argv[i], name)) return 1;
    return 0;
}

static int cmd_eval(int argc, char **argv)
{
    const char *mp = arg_of(argc, argv, "--model", NULL), *dp = arg_of(argc, argv, "--data", NULL);
    Container ct; Config c; Data d; if (!mp || !dp) return 2;
    if (!container_open(mp, &ct, &c)) { fputs("bad model container\n", stderr); return 3; }
    Model md; model_alloc(&md, &c, 0);
    if (!container_layout_ok(&ct, &md)) { fputs("container layout differs\n", stderr); return 3; }
    container_load(&ct, &md);
    if (!data_load(dp, &d) || !data_fits(&d, c.V)) { fputs("bad data\n", stderr); return 3; }
    evaluate(&md, &d, "eval", has_flag(argc, argv, "--picks"), NULL);
    return 0;
}

static int cmd_train(int argc, char **argv)
{
    const char *ref = arg_of(argc, argv, "--reference", NULL), *trp = arg_of(argc, argv, "--train", NULL),
               *dvp = arg_of(argc, argv, "--dev", NULL), *tsp = arg_of(argc, argv, "--test", NULL),
               *out = arg_of(argc, argv, "--out", NULL);
    int steps = atoi(arg_of(argc, argv, "--steps", "4000")), batch = atoi(arg_of(argc, argv, "--batch-size", "32")),
        threads = atoi(arg_of(argc, argv, "--threads", "4")), seed = atoi(arg_of(argc, argv, "--seed", "19"));
    double lr = atof(arg_of(argc, argv, "--lr", "3e-4")), decay = atof(arg_of(argc, argv, "--weight-decay", "0.01"));
    if (!ref || !trp || !out || steps < 1 || batch < 1 || threads < 1 || threads > 64) return 2;
    Container ct; Config c; Data tr, dv = {0}, ts = {0};
    if (!container_open(ref, &ct, &c)) { fputs("bad reference container\n", stderr); return 3; }
    if (!data_load(trp, &tr) || (dvp && !data_load(dvp, &dv)) || (tsp && !data_load(tsp, &ts)) ||
        !data_fits(&tr, c.V) || (dv.n && !data_fits(&dv, c.V)) || (ts.n && !data_fits(&ts, c.V))) { fputs("bad data\n", stderr); return 3; }
    Model md; model_alloc(&md, &c, 1);
    if (!container_layout_ok(&ct, &md)) { fputs("reference layout differs from this trainer\n", stderr); return 3; }
    if (mkdir(out, 0777) != 0) { fprintf(stderr, "output %s must be fresh\n", out); return 3; }
    model_init(&md, (uint64_t)seed);
    if (has_flag(argc, argv, "--warm-start")) container_load(&ct, &md);
    fprintf(stderr, "parameters trained: %zu; rows: %d; threads: %d\n", md.count, tr.n, threads);
    /* pools by intent, as train_meaning.py samples them */
    int **pool = calloc((size_t)tr.intents, sizeof(int *)); int *psize = calloc((size_t)tr.intents, sizeof(int));
    for (int i = 0; i < tr.n; ++i) psize[tr.intent[i]]++;
    for (int k = 0; k < tr.intents; ++k) { pool[k] = malloc((size_t)(psize[k] + 1) * sizeof(int)); psize[k] = 0; }
    for (int i = 0; i < tr.n; ++i) pool[tr.intent[i]][psize[tr.intent[i]]++] = i;
    int live[256], nlive = 0;
    for (int k = 0; k < tr.intents && nlive < 256; ++k) if (psize[k]) live[nlive++] = k;
    Work **work = malloc((size_t)threads * sizeof(Work *));
    for (int t = 0; t < threads; ++t) work[t] = work_new(&md, tr.maxlen, 1);
    Rng rng = {(uint64_t)seed * 7919u + 1}; int *rows = malloc((size_t)batch * sizeof(int));
    char hpath[512]; snprintf(hpath, sizeof(hpath), "%s/history.jsonl", out);
    FILE *hist = fopen(hpath, "w"); time_t start = time(NULL); double b1 = 0.9, b2 = 0.999, pw1 = 1, pw2 = 1;
    for (int step = 1; step <= steps; ++step) {
        for (int i = 0; i < batch; ++i) {
            int k = live[rnext(&rng) % (uint64_t)nlive]; rows[i] = pool[k][rnext(&rng) % (uint64_t)psize[k]];
        }
        Job jobs[64]; pthread_t th[64];
        for (int t = 0; t < threads; ++t) {
            memset(work[t]->g, 0, md.count * sizeof(real));
            jobs[t] = (Job){work[t], &tr, rows, t, threads, batch, 0};
            if (threads == 1) run_job(&jobs[0]); else pthread_create(&th[t], NULL, run_job, &jobs[t]);
        }
        double loss = 0;
        for (int t = 0; t < threads; ++t) { if (threads > 1) pthread_join(th[t], NULL); loss += jobs[t].loss; }
        double sq = 0;
        for (size_t i = 0; i < md.count; ++i) {
            real g = 0; for (int t = 0; t < threads; ++t) g += work[t]->g[i];
            g /= (real)batch; md.g[i] = g; sq += (double)g * g;
        }
        loss /= batch;
        if (!isfinite(loss) || !isfinite(sq)) { fputs("non-finite training loss\n", stderr); return 4; }
        double norm = sqrt(sq), clip = norm > 1.0 ? 1.0 / (norm + 1e-6) : 1.0;
        double rate = 3e-5 + 0.5 * (lr - 3e-5) * (1 + cos(3.14159265358979323846 * (step - 1) / steps));
        pw1 *= b1; pw2 *= b2;
        for (size_t i = 0; i < md.count; ++i) {
            double g = md.g[i] * clip;
            md.m[i] = (real)(b1 * md.m[i] + (1 - b1) * g); md.v[i] = (real)(b2 * md.v[i] + (1 - b2) * g * g);
            double p = md.w[i] * (1 - rate * decay);
            p -= rate / (1 - pw1) * md.m[i] / (sqrt(md.v[i]) / sqrt(1 - pw2) + 1e-8);
            md.w[i] = (real)p;
        }
        fprintf(hist, "{\"step\":%d,\"loss\":%.6f,\"grad_norm\":%.4f,\"seconds\":%ld}\n", step, loss, norm, (long)(time(NULL) - start));
        if (step % 100 == 0 || step == steps) { fflush(hist); fprintf(stderr, "step %d loss %.4f norm %.3f %lds\n", step, loss, norm, (long)(time(NULL) - start)); }
    }
    fclose(hist);
    char mpath[512]; snprintf(mpath, sizeof(mpath), "%s/model.ccv2", out);
    if (!container_save(&ct, &md, mpath)) { fputs("could not write model\n", stderr); return 5; }
    /* Evaluate the float weights and the exported int8 weights separately. */
    if (dv.n) evaluate(&md, &dv, "development (float)", 0, NULL);
    if (ts.n) evaluate(&md, &ts, "test (float)", 0, NULL);
    Container back; Config bc; Model q; model_alloc(&q, &c, 0);
    if (!container_open(mpath, &back, &bc) || !container_layout_ok(&back, &q)) { fputs("exported model does not reload\n", stderr); return 5; }
    container_load(&back, &q);
    if (dv.n) evaluate(&q, &dv, "development (exported int8)", 0, NULL);
    if (ts.n) evaluate(&q, &ts, "test (exported int8)", 0, NULL);
    fprintf(stderr, "wrote %s\n", mpath);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[1], "train")) return cmd_train(argc, argv);
    if (argc >= 2 && !strcmp(argv[1], "eval")) return cmd_eval(argc, argv);
    fputs("usage: ctrain train|eval ...\n", stderr); return 2;
}
#endif

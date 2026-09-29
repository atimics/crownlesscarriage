/* Finite-difference check of the C trainer's hand-written backward pass,
 * run in double precision on a tiny model with the portable matrix code. */
#define CTRAIN_NO_MAIN 1
#define CT_REAL_DOUBLE 1
#define CT_NO_BLAS 1
#include "../tools/dialogue/ctrain/ctrain.c"

static double row_loss(Work *w, const int *tok, int T, int target)
{
    Legal lg; real z[MAXCAND];
    if (!legal_of(tok, T, &lg)) return -1;
    forward(w, tok, T); logits_of(w, &lg, z);
    real mx = z[0]; for (int a = 1; a < lg.n; ++a) if (z[a] > mx) mx = z[a];
    real sum = 0; for (int a = 0; a < lg.n; ++a) sum += exp(z[a] - mx);
    return -(double)(z[target] - mx - log(sum));
}

int main(void)
{
    Config c = {1100, 8, 2, 2, 6, 16}; Model md; model_alloc(&md, &c, 1); model_init(&md, 5);
    /* Larger weights make every path matter; norms away from one test their gradient. */
    Rng r = {77};
    for (size_t i = 0; i < md.count; ++i) md.w[i] += 0.3 * rnorm(&r);
    const int rows[3][12] = {
        {1280, 30, 31, 32, 33, 34, 1580, 1024, 1025, 1026, 1281, 0},
        {1280, 30, 31, 1580, 1024, 1025, 1281, 0, 0, 0, 0, 0},
        {1280, 40, 41, 42, 43, 44, 45, 1580, 1024, 1025, 1026, 1281}};
    const int lens[3] = {11, 7, 12}, targets[3] = {2, 0, 1};
    Work *w = work_new(&md, 12, 1); int checked = 0, failed = 0; double worst = 0;
    for (int n = 0; n < 3; ++n) {
        memset(w->g, 0, md.count * sizeof(real));
        double loss = step_row(w, rows[n], lens[n], targets[n]);
        if (fabs(loss - row_loss(w, rows[n], lens[n], targets[n])) > 1e-12) { puts("loss differs between paths"); return 1; }
        for (size_t i = 0; i < md.count; ++i) {
            double keep = md.w[i], e = 1e-6;
            md.w[i] = keep + e; double up = row_loss(w, rows[n], lens[n], targets[n]);
            md.w[i] = keep - e; double dn = row_loss(w, rows[n], lens[n], targets[n]);
            md.w[i] = keep;
            double numeric = (up - dn) / (2 * e), analytic = w->g[i];
            double err = fabs(numeric - analytic) / (fabs(numeric) + fabs(analytic) + 1e-7);
            if (fabs(numeric) + fabs(analytic) > 1e-9) { ++checked; if (err > worst) worst = err; }
            if (err > 1e-5 && fabs(numeric - analytic) > 1e-8) {
                if (failed++ < 8) printf("gradient %zu row %d: analytic %.10g numeric %.10g\n", i, n, analytic, numeric);
            }
        }
    }
    printf("checked %d nonzero gradients, worst relative error %.3g\n", checked, worst);
    if (failed || checked < 500) { puts("FAIL"); return 1; }
    puts("ok"); return 0;
}

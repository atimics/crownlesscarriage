#include "story/cc_core_model.h"
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { D = 192, FF = 624, LAYERS = 8, CONTEXT = 512, VOCAB = 4096,
       HEADS = 6, HD = 32, MAX_ACTIONS = 160, TENSORS = 59 };
typedef struct CoreTensorLayout { int rows, cols; size_t offset, scales; } CoreTensorLayout;
typedef struct CoreMeaning { const char *name; int id; } CoreMeaning;
typedef struct CoreToken { const char *bytes; int length; } CoreToken;
typedef struct CoreMerge { int a, b, result; } CoreMerge;
typedef struct CoreClass { uint32_t first, last; int flags; } CoreClass;
#include "story/cc_core_model_tables.inc"
_Static_assert(sizeof(CORE_LAYOUT) / sizeof(CORE_LAYOUT[0]) == TENSORS, "Review the native tensor layout");

struct CcCoreModel {
    float *weights[TENSORS];
    float keys[LAYERS][CONTEXT][D], values[LAYERS][CONTEXT][D];
    float cosine[CONTEXT][HD / 2], sine[CONTEXT][HD / 2];
    float sources[CC_CORE_FIELDS][D], hidden[D];
    int tokens[CONTEXT], meta[CONTEXT][5], positions[CC_CORE_FIELDS];
    int feedback[CC_CORE_FIELDS], prefix, used, candidates, actions, status;
    char literals[CC_CORE_FIELDS][CC_EVENT_TEXT_CAPACITY];
    char text[CC_CORE_UTTERANCE];
    size_t length;
};

static bool Utf8(const char *s, size_t n, uint32_t *code, size_t *width)
{
    if (n == 0U) return false;
    unsigned char a = (unsigned char)s[0];
    size_t w = a < 128U ? 1U : a >= 194U && a <= 223U ? 2U :
        a >= 224U && a <= 239U ? 3U : a >= 240U && a <= 244U ? 4U : 0U;
    if (w == 0U || w > n) return false;
    uint32_t value = a & (w == 1U ? 127U : w == 2U ? 31U : w == 3U ? 15U : 7U);
    for (size_t i = 1U; i < w; ++i) {
        unsigned char b = (unsigned char)s[i];
        if ((b & 192U) != 128U) return false;
        value = (value << 6U) | (b & 63U);
    }
    if ((w == 2U && value < 128U) || (w == 3U && value < 2048U) ||
        (w == 4U && value < 65536U) || value > 0x10ffffU ||
        (value >= 0xd800U && value <= 0xdfffU)) return false;
    *code = value; *width = w;
    return true;
}

static int Class(uint32_t code)
{
    size_t low = 0U, high = sizeof(CORE_CLASSES) / sizeof(CORE_CLASSES[0]);
    while (low < high) {
        size_t middle = low + (high - low) / 2U;
        if (code < CORE_CLASSES[middle].first) high = middle;
        else if (code > CORE_CLASSES[middle].last) low = middle + 1U;
        else return CORE_CLASSES[middle].flags;
    }
    return 0;
}

static int Category(const char *s, size_t n, size_t *width)
{
    uint32_t code = 0U;
    if (!Utf8(s, n, &code, width)) return -1;
    return Class(code);
}

static int Bpe(const char *text, size_t size, int *tokens, int capacity)
{
    int work[4096];
    if (size > sizeof(work) / sizeof(work[0])) return -1;
    int count = (int)size;
    for (int i = 0; i < count; ++i) work[i] = CORE_BYTE_IDS[(unsigned char)text[i]];
    while (count > 1) {
        int best = -1, rank = INT32_MAX, result = 0;
        for (int i = 0; i + 1 < count; ++i) {
            for (size_t j = 0U; j < sizeof(CORE_MERGES) / sizeof(CORE_MERGES[0]); ++j) {
                if ((int)j >= rank) break;
                if (CORE_MERGES[j].a == work[i] && CORE_MERGES[j].b == work[i + 1]) {
                    best = i; rank = (int)j; result = CORE_MERGES[j].result; break;
                }
            }
        }
        if (best < 0) break;
        work[best] = result;
        memmove(work + best + 1, work + best + 2, (size_t)(count - best - 2) * sizeof(int));
        --count;
    }
    if (count > capacity) return -1;
    memcpy(tokens, work, (size_t)count * sizeof(int));
    return count;
}

int CcCoreModelEncode(const char *text, int *tokens, int capacity)
{
    if (text == NULL || tokens == NULL || capacity < 0 || strlen(text) > 4096U) return -1;
    size_t full_size = strlen(text), at = 0U;
    int count = 0;
    while (at < full_size) {
        size_t total = full_size, end = at, width = 0U;
        int special = -1;
        if (total - at >= 5U && memcmp(text + at, "[EOS]", 5U) == 0) { special = 0; end += 5U; }
        if (total - at >= 4U && text[at] == '[' && text[at + 1U] == 'F' &&
            text[at + 2U] >= '0' && text[at + 2U] <= '7' && text[at + 3U] == ']') {
            special = 1 + text[at + 2U] - '0'; end = at + 4U;
        }
        if (special >= 0) {
            if (count == capacity) return -1;
            tokens[count++] = special; at = end; continue;
        }
        for (size_t i = at + 1U; i < total; ++i) {
            if (text[i] == '[' && ((total - i >= 5U && memcmp(text + i, "[EOS]", 5U) == 0) ||
                (total - i >= 4U && text[i + 1U] == 'F' && text[i + 2U] >= '0' && text[i + 2U] <= '7' && text[i + 3U] == ']'))) {
                total = i; break;
            }
        }
        if (text[at] == '\'') {
            static const char *const endings[] = {"s", "t", "re", "ve", "m", "ll", "d"};
            for (size_t i = 0U; i < sizeof(endings) / sizeof(endings[0]); ++i) {
                size_t len = strlen(endings[i]);
                if (total - at >= len + 1U && memcmp(text + at + 1U, endings[i], len) == 0) { end += len + 1U; break; }
            }
        }
        if (end == at) {
            size_t start = at;
            if (text[start] == ' ' && start + 1U < total) {
                int next = Category(text + start + 1U, total - start - 1U, &width);
                if (next < 0) return -1;
                if ((next & 4) == 0) ++start;
            }
            int flags = Category(text + start, total - start, &width);
            if (flags < 0) return -1;
            int group = (flags & 1) ? 1 : (flags & 2) ? 2 : (flags & 4) ? 4 : 0;
            end = start + width;
            size_t last = start;
            while (end < total) {
                /* Special tokens split the ordinary regex input first. */
                if (text[end] == '[' && ((total - end >= 5U && memcmp(text + end, "[EOS]", 5U) == 0) ||
                    (total - end >= 4U && text[end + 1U] == 'F' && text[end + 2U] >= '0' && text[end + 2U] <= '7' && text[end + 3U] == ']'))) break;
                flags = Category(text + end, total - end, &width);
                if (flags < 0) return -1;
                int next = (flags & 1) ? 1 : (flags & 2) ? 2 : (flags & 4) ? 4 : 0;
                if (next != group) break;
                last = end; end += width;
            }
            /* The whitespace lookahead leaves the final space for the next word. */
            if (group == 4 && end < total && last > at) end = last;
        }
        int added = Bpe(text + at, end - at, tokens + count, capacity - count);
        if (added < 0) return -1;
        count += added; at = end;
    }
    return count;
}

static float ReadFloat(const unsigned char *bytes)
{
    uint32_t bits = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
        ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
    float value; memcpy(&value, &bits, sizeof(value)); return value;
}

CcCoreModel *CcCoreModelLoad(const char *path)
{
    if (path == NULL || sizeof(float) != 4U) return NULL;
    FILE *file = fopen(path, "rb");
    if (file == NULL) return NULL;
    unsigned char *raw = malloc(CORE_FILE_SIZE);
    bool valid = raw != NULL && fread(raw, 1U, CORE_FILE_SIZE, file) == CORE_FILE_SIZE && fgetc(file) == EOF;
    (void)fclose(file);
    uint64_t hash = UINT64_C(14695981039346656037);
    if (valid) for (size_t i = 0U; i < CORE_FILE_SIZE; ++i) hash = (hash ^ raw[i]) * UINT64_C(1099511628211);
    if (!valid || hash != CORE_FILE_HASH || strcmp(CORE_GRAMMAR, CcCoreAccountGrammar()) != 0) { free(raw); return NULL; }
    CcCoreModel *model = calloc(1U, sizeof(*model));
    if (model == NULL) { free(raw); return NULL; }
    for (int i = 0; i < TENSORS; ++i) {
        CoreTensorLayout layout = CORE_LAYOUT[i];
        size_t count = (size_t)layout.rows * (size_t)(layout.cols > 0 ? layout.cols : 1);
        model->weights[i] = malloc(count * sizeof(float));
        if (model->weights[i] == NULL) { free(raw); CcCoreModelFree(model); return NULL; }
        for (size_t j = 0U; j < count; ++j) {
            float value = layout.cols == 0 ? ReadFloat(raw + layout.offset + 4U * j) :
                (float)(raw[layout.offset + j] < 128U ? (int)raw[layout.offset + j] : (int)raw[layout.offset + j] - 256) *
                ReadFloat(raw + layout.scales + 4U * (j / (size_t)layout.cols));
            if (!isfinite(value)) { free(raw); CcCoreModelFree(model); return NULL; }
            model->weights[i][j] = value;
        }
    }
    free(raw);
    for (int t = 0; t < CONTEXT; ++t) for (int i = 0; i < HD / 2; ++i) {
        float angle = (float)t * powf(10000.0f, -(float)(2 * i) / (float)HD);
        model->cosine[t][i] = cosf(angle); model->sine[t][i] = sinf(angle);
    }
    return model;
}

void CcCoreModelFree(CcCoreModel *model)
{
    if (model == NULL) return;
    for (int i = 0; i < TENSORS; ++i) free(model->weights[i]);
    free(model);
}

static float Dot(const float *a, const float *b, int count)
{
    float sum = 0.0f;
    for (int i = 0; i < count; ++i) sum += a[i] * b[i];
    return sum;
}
static void Matvec(const float *matrix, const float *input, float *output, int rows, int cols)
{
    for (int i = 0; i < rows; ++i) output[i] = Dot(matrix + i * cols, input, cols);
}
static void Norm(const float *input, const float *weight, float *output)
{
    float scale = 1.0f / sqrtf(Dot(input, input, D) / (float)D + FLT_EPSILON);
    for (int i = 0; i < D; ++i) output[i] = input[i] * scale * weight[i];
}

static void Hidden(CcCoreModel *m, int token, const int *meta)
{
    int position = m->used;
    float x[D], z[D], qkv[3 * D], attn[D], update[D], up[2 * FF], feed[FF];
    memcpy(x, m->weights[0] + token * D, sizeof(x));
    if (meta != NULL) for (int j = 0; j < D; ++j) {
        if (meta[0] != 0) x[j] += m->weights[1][meta[0] * D + j] + m->weights[2][meta[1] * D + j] +
            m->weights[3][meta[2] * D + j] + m->weights[4][meta[3] * D + j];
        if (meta[4] != 0) x[j] += m->weights[5][meta[4] * D + j];
    }
    for (int layer = 0; layer < LAYERS; ++layer) {
        int base = 6 + layer * 6;
        Norm(x, m->weights[base], z);
        Matvec(m->weights[base + 2], z, qkv, 3 * D, D);
        for (int h = 0; h < HEADS; ++h) for (int i = 0; i < HD / 2; ++i) {
            int at = h * HD + i * 2;
            float c = m->cosine[position][i], s = m->sine[position][i];
            float a = qkv[at], b = qkv[at + 1];
            qkv[at] = a * c - b * s; qkv[at + 1] = a * s + b * c;
            a = qkv[D + at]; b = qkv[D + at + 1];
            m->keys[layer][position][at] = a * c - b * s;
            m->keys[layer][position][at + 1] = a * s + b * c;
        }
        memcpy(m->values[layer][position], qkv + 2 * D, D * sizeof(float));
        for (int h = 0; h < HEADS; ++h) {
            float scores[CONTEXT], maximum = -FLT_MAX, sum = 0.0f;
            for (int t = 0; t <= position; ++t) {
                scores[t] = Dot(qkv + h * HD, m->keys[layer][t] + h * HD, HD) / sqrtf((float)HD);
                if (scores[t] > maximum) maximum = scores[t];
            }
            for (int t = 0; t <= position; ++t) { scores[t] = expf(scores[t] - maximum); sum += scores[t]; }
            for (int j = 0; j < HD; ++j) {
                float value = 0.0f;
                for (int t = 0; t <= position; ++t) value += scores[t] / sum * m->values[layer][t][h * HD + j];
                attn[h * HD + j] = value;
            }
        }
        Matvec(m->weights[base + 3], attn, update, D, D);
        for (int j = 0; j < D; ++j) x[j] += update[j];
        Norm(x, m->weights[base + 1], z);
        Matvec(m->weights[base + 4], z, up, 2 * FF, D);
        for (int j = 0; j < FF; ++j) feed[j] = up[j] / (1.0f + expf(-up[j])) * up[FF + j];
        Matvec(m->weights[base + 5], feed, update, D, FF);
        for (int j = 0; j < D; ++j) x[j] += update[j];
    }
    Norm(x, m->weights[54], m->hidden);
    ++m->used;
}

static bool WordAt(const char *text, size_t size)
{
    uint32_t code = 0U; size_t width = 0U;
    return size > 0U && Utf8(text, size, &code, &width) && (Class(code) & 8) != 0;
}
static bool WordBefore(const char *text, size_t at)
{
    if (at == 0U) return false;
    size_t start = at - 1U;
    while (start > 0U && ((unsigned char)text[start] & 192U) == 128U) --start;
    return WordAt(text + start, at - start);
}

static bool HistoryText(const CcCoreAccount *account, const CcCoreSpoken *message,
                         CcId speaker, char *output, size_t capacity)
{
    size_t size = strlen(message->text), at = 0U;
    const char *label = message->speaker == speaker ? "self: " : "other: ";
    size_t written = strlen(label);
    if (written >= capacity) return false;
    memcpy(output, label, written);
    while (at < size) {
        size_t best = 0U; int field = -1;
        for (size_t i = 0U; i < account->field_count; ++i) {
            CcCoreField f = account->fields[i];
            if (f.spoken && f.knowledge != CC_CORE_UNKNOWN && f.length > 0U && f.length >= best && f.length <= size - at &&
                memcmp(message->text + at, account->text + f.start, f.length) == 0 &&
                !WordBefore(message->text, at) && !WordAt(message->text + at + f.length, size - at - f.length)) {
                best = f.length; field = (int)i;
            }
        }
        if (field >= 0) {
            if (written + 4U >= capacity) return false;
            output[written++] = '['; output[written++] = 'F'; output[written++] = (char)('0' + field); output[written++] = ']';
            at += best;
        } else {
            if (written + 1U >= capacity) return false;
            output[written++] = message->text[at++];
        }
    }
    if (written + 2U > capacity) return false;
    output[written++] = '\n'; output[written] = '\0'; return true;
}

bool CcCoreModelBegin(CcCoreModel *m, const CcCoreAccount *account,
                      CcId speaker, const CcCoreSpoken *history, size_t count)
{
    if (m == NULL) return false;
    m->status = -1; m->text[0] = '\0'; m->length = 0U;
    if (account == NULL || count > CC_CORE_HISTORY || (count > 0U && history == NULL) ||
        account->field_count > CC_CORE_FIELDS || memchr(account->text, 0, sizeof(account->text)) == NULL) return false;
    int meaning = 0;
    const char *rule = CcCoreAccountRule(account);
    if (rule == NULL) return false;
    for (size_t i = 0U; i < sizeof(CORE_MEANINGS) / sizeof(CORE_MEANINGS[0]); ++i)
        if (strcmp(rule, CORE_MEANINGS[i].name) == 0) meaning = CORE_MEANINGS[i].id;
    if (meaning == 0) return false;
    size_t source_size = strlen(account->text);
    for (size_t i = 0U; i < account->field_count; ++i) {
        CcCoreField f = account->fields[i];
        if (f.start > source_size || f.length > source_size - f.start || f.role < CC_CORE_NONE ||
            f.role > CC_CORE_QUANTITY || f.knowledge < CC_CORE_KNOWN || f.knowledge > CC_CORE_UNKNOWN) return false;
    }
    int messages[CC_CORE_HISTORY][256], lengths[CC_CORE_HISTORY], total = 0;
    for (size_t i = 0U; i < count; ++i) {
        if (memchr(history[i].text, 0, sizeof(history[i].text)) == NULL) return false;
        char text[CC_CORE_UTTERANCE + 16];
        if (!HistoryText(account, &history[i], speaker, text, sizeof(text))) return false;
        lengths[i] = CcCoreModelEncode(text, messages[i], 256);
        if (lengths[i] < 0) return false;
        total += lengths[i];
    }
    size_t first = 0U;
    while (total > 256) total -= lengths[first++];
    int n = 0;
    for (size_t i = first; i < count; ++i) {
        memcpy(m->tokens + n, messages[i], (size_t)lengths[i] * sizeof(int)); n += lengths[i];
    }
    char cue[16];
    (void)snprintf(cue, sizeof(cue), "- %s%s", account->confidence < 40 ? "? " : "", account->retellings >= 4 ? "~ " : "");
    int added = CcCoreModelEncode(cue, m->tokens + n, CONTEXT - n);
    if (added < 0) return false;
    n += added; m->candidates = 0;
    memset(m->meta, 0, sizeof(m->meta));
    for (size_t i = 0U; i < account->field_count; ++i) {
        CcCoreField f = account->fields[i];
        if (!f.spoken) continue;
        m->tokens[n] = (int)i + 1;
        m->meta[n][0] = (int)f.role; m->meta[n][1] = (int)f.knowledge;
        m->meta[n][2] = 3; m->meta[n][3] = 1;
        if (f.knowledge != CC_CORE_UNKNOWN) {
            int c = m->candidates++;
            m->positions[c] = n; m->feedback[c] = (int)i + 1;
            memcpy(m->literals[c], account->text + f.start, f.length); m->literals[c][f.length] = '\0';
        }
        ++n;
    }
    added = CcCoreModelEncode("\n", m->tokens + n, CONTEXT - n);
    if (added < 0) return false;
    n += added;
    for (int i = 0; i < n; ++i) m->meta[i][4] = meaning;
    m->prefix = n; m->used = 0; m->actions = 0; m->status = 0;
    return true;
}

int CcCoreModelStep(CcCoreModel *m, unsigned int budget)
{
    if (m == NULL) return -1;
    while (budget-- > 0U && m->status == 0) {
        if (m->used < m->prefix) {
            int position = m->used;
            Hidden(m, m->tokens[position], m->meta[position]);
            for (int i = 0; i < m->candidates; ++i)
                if (m->positions[i] == position) memcpy(m->sources[i], m->hidden, sizeof(m->hidden));
            continue;
        }
        if (m->actions >= MAX_ACTIONS || m->used >= CONTEXT) { m->status = -1; break; }
        float gate = Dot(m->weights[57], m->hidden, D) + m->weights[58][0];
        int token = 0; const char *bytes = NULL; size_t length = 0U;
        if (m->candidates > 0 && gate > 0.0f) {
            float a[D], b[D], best = -FLT_MAX; int chosen = 0;
            Matvec(m->weights[55], m->hidden, a, D, D); Matvec(m->weights[56], m->hidden, b, D, D);
            for (int i = 0; i < m->candidates; ++i) {
                float score = Dot(a, m->sources[i], D) + Dot(b, m->sources[i], D);
                if (score > best) { best = score; chosen = i; }
            }
            token = m->feedback[chosen]; bytes = m->literals[chosen]; length = strlen(bytes);
        } else {
            float best = -FLT_MAX;
            for (int i = 0; i < VOCAB; ++i) {
                if (i >= 1 && i <= 8) continue;
                float score = Dot(m->weights[0] + i * D, m->hidden, D);
                if (score > best) { best = score; token = i; }
            }
            if (token == 0) { m->status = 1; break; }
            bytes = CORE_TOKENS[token].bytes; length = (size_t)CORE_TOKENS[token].length;
        }
        if (length >= sizeof(m->text) - m->length || memchr(bytes, 0, length) != NULL) { m->status = -1; break; }
        memcpy(m->text + m->length, bytes, length); m->length += length; m->text[m->length] = '\0';
        ++m->actions; Hidden(m, token, NULL);
    }
    if (m->status == 1) {
        size_t at = 0U;
        while (at < m->length) {
            uint32_t code = 0U; size_t width = 0U;
            if (!Utf8(m->text + at, m->length - at, &code, &width) || code < 32U || code == 127U) { m->status = -1; break; }
            at += width;
        }
        if (m->length == 0U) m->status = -1;
    }
    return m->status;
}
const char *CcCoreModelText(const CcCoreModel *model)
{
    return model != NULL && model->status == 1 ? model->text : NULL;
}
bool CcCoreModelGenerate(CcCoreModel *model, const CcCoreAccount *account,
                         CcId speaker, const CcCoreSpoken *history, size_t count,
                         char *text, size_t capacity)
{
    if (text == NULL || capacity == 0U) return false;
    text[0] = '\0';
    if (!CcCoreModelBegin(model, account, speaker, history, count)) return false;
    if (CcCoreModelStep(model, CONTEXT + MAX_ACTIONS) != 1 || model->length >= capacity) return false;
    memcpy(text, model->text, model->length + 1U); return true;
}

#include "sim/cc_wants.h"
#include <string.h>

/* Explicit little-endian fields. Padding and native struct layout never enter
   a save or hash. The same field walk covers both, including unused slots. */
typedef struct Wire { uint8_t *out; const uint8_t *in; size_t size, at; bool ok; } Wire;
static void Bytes(Wire *w, void *value, size_t size)
{
    if (w->at > w->size || size > w->size - w->at) { w->ok = false; return; }
    if (w->in != NULL) memcpy(value, w->in + w->at, size);
    else memcpy(w->out + w->at, value, size);
    w->at += size;
}
static void U64(Wire *w, uint64_t *value)
{
    uint8_t bytes[8];
    for (int i = 0; i < 8; ++i) bytes[i] = (uint8_t)(*value >> (i * 8));
    Bytes(w, bytes, sizeof(bytes));
    if (w->in != NULL && w->ok) {
        *value = 0;
        for (int i = 0; i < 8; ++i) *value |= (uint64_t)bytes[i] << (i * 8);
    }
}
static void I32(Wire *w, int32_t *value)
{
    uint32_t bits = (uint32_t)*value;
    uint8_t bytes[4];
    for (int i = 0; i < 4; ++i) bytes[i] = (uint8_t)(bits >> (i * 8));
    Bytes(w, bytes, sizeof(bytes));
    if (w->in != NULL && w->ok) {
        bits = 0;
        for (int i = 0; i < 4; ++i) bits |= (uint32_t)bytes[i] << (i * 8);
        *value = bits <= INT32_MAX ? (int32_t)bits : (int32_t)((int64_t)bits - INT64_C(4294967296));
    }
}
static void State(Wire *w, CcWantsState *s)
{
    int32_t version = 1; I32(w, &version);
    if (version != 1) { w->ok = false; return; }
    I32(w, &s->initialized); I32(w, &s->last_day);
    for (int i = 0; i < CC_PERSONAL_WANTS; ++i) {
        CcPersonalWant *v = &s->wants[i];
        U64(w, &v->id); U64(w, &v->person_id); U64(w, &v->item_id); U64(w, &v->parent_id);
        U64(w, &v->source_place_id); U64(w, &v->home_id); U64(w, &v->cause_event_id); U64(w, &v->outcome_event_id);
        uint64_t escrow = (uint64_t)v->escrow; U64(w, &escrow);
        if (escrow > INT64_MAX) w->ok = false;
        else v->escrow = (int64_t)escrow;
        I32(w, &v->kind); I32(w, &v->status); I32(w, &v->good); I32(w, &v->quantity);
        I32(w, &v->created_day); I32(w, &v->known_day); I32(w, &v->settled_day); I32(w, &v->revision);
    }
    for (int i = 0; i < CC_BELONGINGS; ++i) {
        CcBelonging *v = &s->items[i];
        U64(w, &v->id); U64(w, &v->custody_id); U64(w, &v->home_id); U64(w, &v->last_place_id); U64(w, &v->cause_event_id);
        I32(w, &v->repair_iron); I32(w, &v->last_wear_day); Bytes(w, v->name, sizeof(v->name));
    }
}
size_t CcWantsEncode(const CcWantsState *state, uint8_t *bytes, size_t capacity)
{
    if (state == NULL || bytes == NULL) return 0;
    CcWantsState copy = *state;
    Wire w = {.out = bytes, .size = capacity, .ok = true}; State(&w, &copy);
    return w.ok ? w.at : 0;
}
bool CcWantsDecode(CcWantsState *state, const uint8_t *bytes, size_t length)
{
    if (state == NULL || bytes == NULL) return false;
    CcWantsState copy = {0};
    Wire w = {.in = bytes, .size = length, .ok = true}; State(&w, &copy);
    if (!w.ok || w.at != length) return false;
    *state = copy; return true;
}
uint64_t CcWantsHash(const CcWantsState *state)
{
    uint8_t bytes[CC_WANTS_WIRE_CAPACITY];
    size_t size = CcWantsEncode(state, bytes, sizeof(bytes));
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0; i < size; ++i) { hash ^= bytes[i]; hash *= UINT64_C(1099511628211); }
    return hash;
}

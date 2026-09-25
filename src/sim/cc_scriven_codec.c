#include "sim/cc_scriven.h"
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
static void Finding(Wire *w, CcScrivenFinding *f)
{
    U64(w, &f->dragon_id);
    for (int i = 0; i < 2; ++i) { U64(w, &f->book_ids[i]); U64(w, &f->school_ids[i]); }
    for (int i = 0; i < 6; ++i) U64(w, &f->signers[i]);
    I32(w, &f->proposed_day); I32(w, &f->earliest_day); I32(w, &f->latest_day);
    I32(w, &f->agreed_day); I32(w, &f->votes); I32(w, &f->schools);
    Bytes(w, f->citations, sizeof(f->citations));
}
static void State(Wire *w, CcScrivenState *s)
{
    int32_t version = 1; I32(w, &version);
    if (version != 1) { w->ok = false; return; }
    for (int i = 0; i < CC_SCRIVEN_BOOKS; ++i) {
        CcScrivenBook *b = &s->books[i];
        U64(w, &b->id); U64(w, &b->source_id); U64(w, &b->school_id); U64(w, &b->almanac_id);
        I32(w, &b->edition_day); I32(w, &b->condition); I32(w, &b->loan_due_day);
        U64(w, &b->borrower_id); U64(w, &b->return_place_id);
        Bytes(w, b->passages, sizeof(b->passages));
        for (int j = 0; j < CC_SCRIVEN_NOTES; ++j) {
            CcScrivenNote *n = &b->notes[j];
            U64(w, &n->dragon_id); U64(w, &n->author_id); U64(w, &n->place_id); U64(w, &n->source_book_id);
            I32(w, &n->day); I32(w, &n->kind); I32(w, &n->value); Bytes(w, n->text, sizeof(n->text));
        }
    }
    for (int i = 0; i < CC_SCRIVEN_DELEGATES; ++i) {
        CcScrivenDelegate *d = &s->delegates[i];
        U64(w, &d->person_id); U64(w, &d->book_id); U64(w, &d->home_id); U64(w, &d->place_id);
        U64(w, &d->route_id); U64(w, &d->hop_id);
        I32(w, &d->phase); I32(w, &d->arrival_day); I32(w, &d->wheat); I32(w, &d->spent); I32(w, &d->notice_day);
        U64(w, &d->edition_id);
    }
    for (int i = 0; i < CC_SCRIVEN_AGES; ++i) { U64(w, &s->ages[i].dragon_id); I32(w, &s->ages[i].first_deep_day); }
    Finding(w, &s->finding);
    for (int i = 0; i < 32; ++i) Finding(w, &s->almanacs[i]);
    for (int i = 0; i < 6; ++i) Finding(w, &s->local[i]);
    Finding(w, &s->company);
    U64(w, &s->host_id); I32(w, &s->meeting_year); I32(w, &s->opens_day); I32(w, &s->closes_day); I32(w, &s->status);
    for (int i = 0; i < 6; ++i) { I32(w, &s->last_hosted[i]); I32(w, &s->notice_arrives[i]); }
    I32(w, &s->meetings); I32(w, &s->comparisons); I32(w, &s->returns); I32(w, &s->failed_trips);
    I32(w, &s->editions); I32(w, &s->age_count); I32(w, &s->player_observed_day); I32(w, &s->player_read_day);
    U64(w, &s->player_read_book); Bytes(w, s->report, sizeof(s->report)); Bytes(w, s->player_report, sizeof(s->player_report));
}
size_t CcScrivenEncode(const CcScrivenState *state, uint8_t *bytes, size_t capacity)
{
    if (state == NULL || bytes == NULL) return 0;
    CcScrivenState copy = *state;
    Wire w = {.out = bytes, .size = capacity, .ok = true}; State(&w, &copy);
    return w.ok ? w.at : 0;
}
bool CcScrivenDecode(CcScrivenState *state, const uint8_t *bytes, size_t length)
{
    if (state == NULL || bytes == NULL) return false;
    CcScrivenState copy = {0};
    Wire w = {.in = bytes, .size = length, .ok = true}; State(&w, &copy);
    if (!w.ok || w.at != length) return false;
    *state = copy; return true;
}
uint64_t CcScrivenHash(const CcScrivenState *state)
{
    uint8_t bytes[CC_SCRIVEN_WIRE_CAPACITY];
    size_t size = CcScrivenEncode(state, bytes, sizeof(bytes));
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0; i < size; ++i) { hash ^= bytes[i]; hash *= UINT64_C(1099511628211); }
    return hash;
}

static void CrownNote(Wire *w, CcScrivenNote *n)
{
    U64(w, &n->dragon_id); U64(w, &n->author_id); U64(w, &n->place_id); U64(w, &n->source_book_id);
    I32(w, &n->day); I32(w, &n->kind); I32(w, &n->value); Bytes(w, n->text, sizeof(n->text));
}
static void CrownState(Wire *w, CcCrownCalendar *s)
{
    int32_t version = 1; I32(w, &version);
    if (version != 1) { w->ok = false; return; }
    for (int i = 0; i < CC_SCRIVEN_AGES; ++i) CrownNote(w, &s->sightings[i]);
    CrownNote(w, &s->company_sighting);
    for (int i = 0; i < 32; ++i) Finding(w, &s->almanacs[i]);
    for (int i = 0; i < 6; ++i) Finding(w, &s->local[i]);
    Finding(w, &s->company);
    for (int i = 0; i < CC_SCRIVEN_BOOKS; ++i) U64(w, &s->book_editions[i]);
    I32(w, &s->sighting_count); I32(w, &s->editions);
}
size_t CcCrownCalendarEncode(const CcCrownCalendar *state, uint8_t *bytes, size_t capacity)
{
    if (state == NULL || bytes == NULL) return 0;
    CcCrownCalendar copy = *state;
    Wire w = {.out = bytes, .size = capacity, .ok = true}; CrownState(&w, &copy);
    return w.ok ? w.at : 0;
}
bool CcCrownCalendarDecode(CcCrownCalendar *state, const uint8_t *bytes, size_t length)
{
    if (state == NULL || bytes == NULL) return false;
    CcCrownCalendar copy = {0};
    Wire w = {.in = bytes, .size = length, .ok = true}; CrownState(&w, &copy);
    if (!w.ok || w.at != length) return false;
    *state = copy; return true;
}
uint64_t CcCrownCalendarHash(const CcCrownCalendar *state)
{
    uint8_t bytes[CC_SCRIVEN_WIRE_CAPACITY];
    size_t size = CcCrownCalendarEncode(state, bytes, sizeof(bytes));
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0; i < size; ++i) { hash ^= bytes[i]; hash *= UINT64_C(1099511628211); }
    return hash;
}

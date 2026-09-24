#ifndef CC_SCRIVEN_TYPES_H
#define CC_SCRIVEN_TYPES_H
#include <stdint.h>
#define CC_SCRIVEN_BOOKS 24
#define CC_SCRIVEN_DELEGATES 6
#define CC_SCRIVEN_AGES 32
#define CC_SCRIVEN_NOTES 4
#define CC_SCRIVEN_TEXT 144
#define CC_SCRIVEN_WIRE_CAPACITY 65536

typedef enum CcScrivenNoteKind {
    CC_SCRIVEN_NOTE_EMPTY, CC_SCRIVEN_NOTE_CROWNED, CC_SCRIVEN_NOTE_DEEP,
    CC_SCRIVEN_NOTE_GOBLINS, CC_SCRIVEN_NOTE_SKY
} CcScrivenNoteKind;
typedef struct CcScrivenNote {
    uint64_t dragon_id, author_id, place_id, source_book_id;
    int32_t day, kind, value;
    char text[CC_SCRIVEN_TEXT];
} CcScrivenNote;
typedef struct CcScrivenFinding {
    uint64_t dragon_id, book_ids[2], school_ids[2], signers[6];
    int32_t proposed_day, earliest_day, latest_day, agreed_day, votes, schools;
    char citations[2][CC_SCRIVEN_TEXT];
} CcScrivenFinding;
typedef struct CcScrivenBook {
    uint64_t id, source_id, school_id, almanac_id;
    int32_t edition_day, condition, loan_due_day;
    uint64_t borrower_id, return_place_id;
    /* Text is frozen on binding. Notes are appended to unused margins. */
    char passages[3][CC_SCRIVEN_TEXT];
    CcScrivenNote notes[CC_SCRIVEN_NOTES];
} CcScrivenBook;
typedef enum CcScrivenJourney {
    CC_SCRIVEN_IDLE, CC_SCRIVEN_EXPEDITION, CC_SCRIVEN_OUTWARD,
    CC_SCRIVEN_ATTENDING, CC_SCRIVEN_RETURNING, CC_SCRIVEN_FINISHED,
    CC_SCRIVEN_FALLEN
} CcScrivenJourney;
typedef struct CcScrivenDelegate {
    uint64_t person_id, book_id, home_id, place_id, route_id, hop_id;
    int32_t phase, arrival_day, wheat, spent, notice_day;
    uint64_t edition_id;
} CcScrivenDelegate;
typedef struct CcScrivenAge {
    uint64_t dragon_id;
    int32_t first_deep_day;
} CcScrivenAge;

typedef struct CcScrivenState {
    CcScrivenBook books[CC_SCRIVEN_BOOKS];
    CcScrivenDelegate delegates[CC_SCRIVEN_DELEGATES];
    CcScrivenAge ages[CC_SCRIVEN_AGES];
    CcScrivenFinding finding;
    CcScrivenFinding almanacs[32];
    /* Editions travel with delegates; each town keeps the edition it received. */
    CcScrivenFinding local[6], company;
    uint64_t host_id;
    int32_t meeting_year, opens_day, closes_day, status;
    int32_t last_hosted[6], notice_arrives[6];
    int32_t meetings, comparisons, returns, failed_trips, editions, age_count;
    int32_t player_observed_day, player_read_day;
    uint64_t player_read_book;
    char report[256], player_report[256];
} CcScrivenState;
_Static_assert(sizeof(CcScrivenState) == 47952,
    "Review the scriven codec, save migration, validation, and hash when fields change");
#endif

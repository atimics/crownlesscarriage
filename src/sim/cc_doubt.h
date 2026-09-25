#ifndef CC_DOUBT_H
#define CC_DOUBT_H

/* Doubt marks for cult records: the Read-Debt epistemics as computable
   primitives. A record starts with an explicit doubt mark and the mark
   moves only when independent verifications accumulate. Nothing here
   reads global simulation state: all inputs come from the caller's own
   verification history, so agent-local knowledge cannot leak. */

/* Doubt level carried by a record. '?' means unverified (hedge);
   '~' means drifting (verified once, but recent checks disagree);
   '' means settled (saturated with independent agreement). */
typedef enum CcDoubtMark {
    CC_DOUBT_SETTLED = 0,
    CC_DOUBT_DRIFT = 1,
    CC_DOUBT_UNVERIFIED = 2
} CcDoubtMark;

/* One independent verification of a claim. */
typedef struct CcDoubtRun {
    int verified;      /* 1 if this retelling agrees with the record */
    int age_days;      /* how long ago the verification happened */
} CcDoubtRun;

typedef struct CcDoubtConfig {
    double half_life_days;  /* weight of verifications halves per this span */
    int min_runs;           /* fewer runs than this -> CC_DOUBT_UNVERIFIED */
    int saturation_runs;    /* runs at or above this can settle the claim */
    int recent_run_count;   /* window for drift detection */
} CcDoubtConfig;

/* Default policy: half-life 365 days, 3 runs minimum, 8 saturate, drift
   window of 4. Matches the Read-Debt doctrine defaults. */
CcDoubtConfig CcDoubtDefaultConfig(void);

/* Weight of one verification, (0..1], decaying with age. */
double CcDoubtRunWeight(const CcDoubtConfig *config, int age_days);

/* Sample saturation: how much independent evidence exists, (0..1].
   weight_sum of verified runs / saturation weight of saturation_runs. */
double CcDoubtSample(const CcDoubtConfig *config, const CcDoubtRun *runs,
                     int run_count);

/* Confidence in (0..1]: verified weight fraction times sample saturation.
   Sparse histories keep the mark unverified; the score only ever
   supplements a check, it never replaces one. */
double CcDoubtScore(const CcDoubtConfig *config, const CcDoubtRun *runs,
                    int run_count);

/* Drift: the recent window verified materially worse than the lifetime.
   Returns 1 when the retell chain is degrading. */
int CcDoubtDetectDrift(const CcDoubtConfig *config, const CcDoubtRun *runs,
                       int run_count);

/* The mark for a record given its verification history. Deterministic,
   pure, and derived only from the runs. */
CcDoubtMark CcDoubtMarkOf(const CcDoubtConfig *config, const CcDoubtRun *runs,
                          int run_count);

/* Single-character rendering for the ledger: ' ' settled, '~' drift,
   '?' unverified. */
char CcDoubtGlyph(CcDoubtMark mark);

#endif /* CC_DOUBT_H */

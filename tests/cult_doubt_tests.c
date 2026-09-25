#include "sim/cc_doubt.h"
#include "test_support.h"

#include <math.h>
#include <stdio.h>

/* Golden values: fixed inputs, fixed expected outputs. The formulas are
   re-derivations of the confidence-kernel primitives (project-89, MIT)
   documented in docs/design/cult-read-debt-dynamics.md. */

static void TestFreshRecordStaysUnverified(void)
{
    CcDoubtConfig config = CcDoubtDefaultConfig();
    CcDoubtRun runs[] = {{1, 0}, {1, 10}};
    CC_CHECK(CcDoubtMarkOf(&config, runs, 2) == CC_DOUBT_UNVERIFIED);
    CC_CHECK(fabs(CcDoubtScore(&config, runs, 2) - 0.0) < 1e-12);
}

static void TestSaturationSettles(void)
{
    CcDoubtConfig config = CcDoubtDefaultConfig();
    CcDoubtRun runs[8];
    int i;
    for (i = 0; i < 8; ++i) runs[i] = (CcDoubtRun){1, 0};
    CC_CHECK(CcDoubtSample(&config, runs, 8) == 1.0);
    CC_CHECK(CcDoubtMarkOf(&config, runs, 8) == CC_DOUBT_SETTLED);
}

static void TestAgeDecayWeightHalvesPerHalfLife(void)
{
    CcDoubtConfig config = CcDoubtDefaultConfig();
    CC_CHECK(fabs(CcDoubtRunWeight(&config, 0) - 1.0) < 1e-12);
    CC_CHECK(fabs(CcDoubtRunWeight(&config, 365) - 0.5) < 1e-12);
    CC_CHECK(fabs(CcDoubtRunWeight(&config, 730) - 0.25) < 1e-12);
}

static void TestContradictionsSuppressScore(void)
{
    CcDoubtConfig config = CcDoubtDefaultConfig();
    CcDoubtRun runs[] = {{1, 0}, {0, 1}, {1, 2}, {0, 3}};
    double score = CcDoubtScore(&config, runs, 4);
    CC_CHECK(score >= 0.0 && score < 0.5);
    CC_CHECK(CcDoubtMarkOf(&config, runs, 4) == CC_DOUBT_UNVERIFIED);
}

static void TestRecentCollapseIsDrift(void)
{
    CcDoubtConfig config = CcDoubtDefaultConfig();
    /* Six early verifications, then four recent failures. */
    CcDoubtRun runs[] = {{1, 0},  {1, 0},  {1, 0},  {1, 0},
                         {1, 0},  {1, 0},  {0, 10}, {0, 11},
                         {0, 12}, {0, 13}};
    CC_CHECK(CcDoubtDetectDrift(&config, runs, 10) == 1);
    CC_CHECK(CcDoubtMarkOf(&config, runs, 10) == CC_DOUBT_DRIFT);
    CC_CHECK(CcDoubtGlyph(CC_DOUBT_DRIFT) == '~');
    CC_CHECK(CcDoubtGlyph(CC_DOUBT_UNVERIFIED) == '?');
    CC_CHECK(CcDoubtGlyph(CC_DOUBT_SETTLED) == ' ');
}

static void TestSteadyHistoryDoesNotDrift(void)
{
    CcDoubtConfig config = CcDoubtDefaultConfig();
    CcDoubtRun runs[] = {{1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}, {1, 0}};
    CC_CHECK(CcDoubtDetectDrift(&config, runs, 6) == 0);
}

static void TestEmptyHistoryIsUnverified(void)
{
    CcDoubtConfig config = CcDoubtDefaultConfig();
    CC_CHECK(CcDoubtMarkOf(&config, NULL, 0) == CC_DOUBT_UNVERIFIED);
    CC_CHECK(CcDoubtSample(&config, NULL, 0) == 0.0);
}

int main(void)
{
    TestFreshRecordStaysUnverified();
    TestSaturationSettles();
    TestAgeDecayWeightHalvesPerHalfLife();
    TestContradictionsSuppressScore();
    TestRecentCollapseIsDrift();
    TestSteadyHistoryDoesNotDrift();
    TestEmptyHistoryIsUnverified();
    (void)printf("cult_doubt_tests: all checks passed\n");
    return 0;
}

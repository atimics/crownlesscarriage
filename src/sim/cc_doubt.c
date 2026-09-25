#include "sim/cc_doubt.h"
#include "sim/cc_dmath.h"

#include <math.h>
#include <stddef.h>

CcDoubtConfig CcDoubtDefaultConfig(void)
{
    CcDoubtConfig config;
    config.half_life_days = 365.0;
    config.min_runs = 3;
    config.saturation_runs = 8;
    config.recent_run_count = 4;
    return config;
}

double CcDoubtRunWeight(const CcDoubtConfig *config, int age_days)
{
    if (config == NULL || age_days < 0) return 0.0;
    return CcDmathExp2(-(double)age_days / config->half_life_days);
}

double CcDoubtSample(const CcDoubtConfig *config, const CcDoubtRun *runs,
                     int run_count)
{
    double weight_sum = 0.0;
    int i;
    if (config == NULL || runs == NULL || run_count <= 0) return 0.0;
    for (i = 0; i < run_count; ++i) {
        if (runs[i].verified) weight_sum += CcDoubtRunWeight(config, runs[i].age_days);
    }
    /* Saturation weight equals saturation_runs fresh verifications. */
    if (weight_sum >= (double)config->saturation_runs) return 1.0;
    return weight_sum / (double)config->saturation_runs;
}

double CcDoubtScore(const CcDoubtConfig *config, const CcDoubtRun *runs,
                    int run_count)
{
    double verified = 0.0;
    double total = 0.0;
    int i;
    if (config == NULL || runs == NULL || run_count < config->min_runs) return 0.0;
    for (i = 0; i < run_count; ++i) {
        double w = CcDoubtRunWeight(config, runs[i].age_days);
        total += w;
        if (runs[i].verified) verified += w;
    }
    if (total <= 0.0) return 0.0;
    return (verified / total) * CcDoubtSample(config, runs, run_count);
}

int CcDoubtDetectDrift(const CcDoubtConfig *config, const CcDoubtRun *runs,
                       int run_count)
{
    double recent_verified = 0.0;
    double recent_total = 0.0;
    double lifetime_rate = 0.0;
    int lifetime_verified = 0;
    int recent_start, i;
    if (config == NULL || runs == NULL || run_count < config->min_runs) return 0;
    recent_start = run_count > config->recent_run_count
                       ? run_count - config->recent_run_count
                       : 0;
    for (i = 0; i < run_count; ++i) {
        if (runs[i].verified) lifetime_verified++;
    }
    lifetime_rate = (double)lifetime_verified / (double)run_count;
    for (i = recent_start; i < run_count; ++i) {
        recent_total += 1.0;
        if (runs[i].verified) recent_verified += 1.0;
    }
    if (recent_total <= 0.0) return 0;
    return recent_verified / recent_total < lifetime_rate * 0.5 ? 1 : 0;
}

CcDoubtMark CcDoubtMarkOf(const CcDoubtConfig *config, const CcDoubtRun *runs,
                          int run_count)
{
    if (config == NULL || runs == NULL || run_count < config->min_runs)
        return CC_DOUBT_UNVERIFIED;
    if (CcDoubtDetectDrift(config, runs, run_count)) return CC_DOUBT_DRIFT;
    if (CcDoubtScore(config, runs, run_count) >= 0.75) return CC_DOUBT_SETTLED;
    return CC_DOUBT_UNVERIFIED;
}

char CcDoubtGlyph(CcDoubtMark mark)
{
    switch (mark) {
        case CC_DOUBT_SETTLED: return ' ';
        case CC_DOUBT_DRIFT: return '~';
        case CC_DOUBT_UNVERIFIED:
        default: return '?';
    }
}

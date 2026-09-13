static double RenderBenchmarkTestClock(void) { return 42.0; }

static int RunRenderBenchmarkRegression(void)
{
    CcRenderBenchmark benchmark = {0};
    char *args[] = {"crownless", "--benchmark-render", "2", "4", "street", "5"};
    if (!CcRenderBenchmarkParse(6, args, &benchmark) || !benchmark.active ||
        benchmark.frames != 2 || benchmark.minimum_fps != 4 || benchmark.p95_budget != 5)
        return ClientRegressionFailure("Benchmark options must retain their requested values.");
    for (int frame = 0; frame < 60; ++frame) {
        if (CcRenderBenchmarkAfterFrame(&benchmark, RenderBenchmarkTestClock) || benchmark.count != 0)
            return ClientRegressionFailure("Benchmark warmup must finish before counted frames.");
    }
    if (benchmark.started != 42.0 ||
        CcRenderBenchmarkAfterFrame(&benchmark, RenderBenchmarkTestClock) ||
        !CcRenderBenchmarkAfterFrame(&benchmark, RenderBenchmarkTestClock) || benchmark.count != 2)
        return ClientRegressionFailure("Benchmark must measure exactly the requested frame count.");
    FILE *output = tmpfile();
    FILE *errors = tmpfile();
    if (output == NULL || errors == NULL) {
        if (output != NULL) (void)fclose(output);
        if (errors != NULL) (void)fclose(errors);
        return ClientRegressionFailure("Benchmark test needs temporary report files.");
    }
    CcLocalRendererStats stats = {.hero_skin_updates = 1, .hero_skinned_meshes = 1,
        .high_detail_characters = 1, .p95_frame_milliseconds = 4};
    bool valid = CcRenderBenchmarkReport(&benchmark, 0.5, stats, output, errors) == 0;
    benchmark.minimum_fps = 5;
    valid &= CcRenderBenchmarkReport(&benchmark, 0.5, stats, output, errors) == 2;
    benchmark.minimum_fps = 4;
    stats.p95_frame_milliseconds = 6;
    valid &= CcRenderBenchmarkReport(&benchmark, 0.5, stats, output, errors) == 2;
    stats.p95_frame_milliseconds = 4;
    stats.hero_skin_updates = 0;
    valid &= CcRenderBenchmarkReport(&benchmark, 0.5, stats, output, errors) == 2;
    stats = (CcLocalRendererStats){0};
    benchmark.roadbook = true;
    valid &= CcRenderBenchmarkReport(&benchmark, 0.5, stats, output, errors) == 0;
    valid &= CcRenderBenchmarkReport(&benchmark, 0.0, stats, output, errors) == 2;
    valid &= CcRenderBenchmarkReport(&benchmark, NAN, stats, output, errors) == 2;
    (void)fclose(output);
    (void)fclose(errors);
    if (!valid) return ClientRegressionFailure("Benchmark timing and scene budgets must enforce their limits.");
    const char *invalid_fps[] = {"nan", "inf", "1e999", "0", "-1", "12x"};
    for (size_t i = 0; i < sizeof(invalid_fps) / sizeof(invalid_fps[0]); ++i) {
        args[3] = (char *)invalid_fps[i];
        if (CcRenderBenchmarkParse(6, args, &benchmark))
            return ClientRegressionFailure("Benchmark FPS must be finite and positive.");
    }
    args[3] = "4";
    args[2] = "9999999999999999999999999999";
    if (CcRenderBenchmarkParse(6, args, &benchmark))
        return ClientRegressionFailure("Benchmark frame parsing must detect overflow.");
    char *normal[] = {"crownless"};
    if (!CcRenderBenchmarkParse(1, normal, &benchmark) || benchmark.active ||
        benchmark.count != 0 || benchmark.warmup_count != 0)
        return ClientRegressionFailure("Normal startup must reset benchmark state.");
    (void)puts("PASS render benchmark: options, warmup, frame count, budgets, and finite values");
    return 0;
}

/* Render benchmark orchestration and validation belong to this interface. */
typedef struct CcRenderBenchmark {
    bool active;
    int32_t frames;
    double minimum_fps, p95_budget;
    const char *scene;
    bool roadbook_route, roadbook_network, roadbook;
    int32_t warmup_count, count;
    double started;
} CcRenderBenchmark;

static bool CcRenderBenchmarkParse(int argc, char **argv, CcRenderBenchmark *benchmark)
{
    *benchmark = (CcRenderBenchmark){0};
    benchmark->active = argc >= 2 &&
                            strcmp(argv[1], "--benchmark-render") == 0;
    benchmark->frames = 600;
    benchmark->minimum_fps = 0.0;
    benchmark->p95_budget = 0.0;
    benchmark->scene = "street";
    if (benchmark->active && argc >= 3) {
        char *end = NULL;
        errno = 0;
        long parsed = strtol(argv[2], &end, 10);
        if (errno == ERANGE || end == argv[2] || *end != '\0' || parsed <= 0 || parsed > INT32_MAX) {
            (void)fprintf(stderr, "Render benchmark frame count is invalid.\n");
            return false;
        }
        benchmark->frames = (int32_t)parsed;
    }
    if (benchmark->active && argc >= 4) {
        char *end = NULL;
        double parsed = strtod(argv[3], &end);
        if (end == argv[3] || *end != '\0' || !isfinite(parsed) || parsed <= 0.0) {
            (void)fprintf(stderr,
                          "Render benchmark minimum FPS is invalid.\n");
            return false;
        }
        benchmark->minimum_fps = parsed;
    }
    if (benchmark->active && argc >= 5 && argv[4][0] != '-') {
        benchmark->scene = argv[4];
        if (strcmp(benchmark->scene, "street") != 0 &&
            strcmp(benchmark->scene, "market") != 0 &&
            strcmp(benchmark->scene, "road") != 0 &&
            strcmp(benchmark->scene, "roadbook-route") != 0 &&
            strcmp(benchmark->scene, "roadbook-network") != 0 &&
            strcmp(benchmark->scene, "combat") != 0) {
            (void)fprintf(stderr,
                          "Render benchmark scene must be street, market, road, roadbook-route, roadbook-network, or combat.\n");
            return false;
        }
    }
    if (benchmark->active && argc >= 6 && argv[5][0] != '-') {
        char *end = NULL;
        double parsed = strtod(argv[5], &end);
        if (end == argv[5] || *end != '\0' || !isfinite(parsed) ||
            parsed <= 0.0) {
            (void)fprintf(stderr,
                          "Render benchmark p95 budget is invalid.\n");
            return false;
        }
        benchmark->p95_budget = parsed;
    }
    benchmark->roadbook_route = benchmark->active &&
        strcmp(benchmark->scene, "roadbook-route") == 0;
    benchmark->roadbook_network = benchmark->active &&
        strcmp(benchmark->scene, "roadbook-network") == 0;
    benchmark->roadbook = benchmark->roadbook_route ||
        benchmark->roadbook_network;
    return true;
}

static void CcRenderBenchmarkPrepareWorld(const CcRenderBenchmark *benchmark, CcSim *sim)
{
    if (benchmark->roadbook) {
        PrepareReviewJourney(sim, (CcReviewJourneyOptions){
            .suppress_encounter = true, .advance_trip = true,
            .target_progress = 420, .tick_step = CC_WORLD_TICKS_PER_SECOND,
        });
    }
    if (benchmark->roadbook_network) {
        uint32_t catalogue_mask = 0U;
        for (int32_t map_index = 0;
             map_index < sim->map_count && map_index < CC_MAX_MAPS &&
             map_index < 32; ++map_index) {
            sim->maps[map_index].owner_id = sim->player.id;
            catalogue_mask |= UINT32_C(1) << (uint32_t)map_index;
        }
        sim->player.map_catalogue_mask = catalogue_mask;
        sim->player.map_archive_mask = 0U;
        CcSimInitializePlayerRouteKnowledge(sim);
    }
}

static void CcRenderBenchmarkPrepareScene(const CcRenderBenchmark *benchmark, CcSim *sim,
                                         LocalState *local, ClientView *view, ClientView *return_view)
{
    if (benchmark->active) {
        (*view) = VIEW_LOCAL;
        (*return_view) = VIEW_LOCAL;
        if (strcmp(benchmark->scene, "market") == 0) {
            local->market_interior = true;
            RepositionHero(local, (Vector2){4.60f, 5.10f}, true);
            local->agent.facing_yaw = 0.14f;
        } else if (strcmp(benchmark->scene, "road") == 0) {
            BeginRoadTravelState(sim, local);
            local->convoy.pace = 0.0f;
        } else if (benchmark->roadbook) {
            BeginRoadTravelState(sim, local);
            local->convoy.pace = 0.0f;
            local->world_carriage.pace = 0.0f;
            local->world_carriage.camera_weight = 1.0f;
            local->world_carriage.camera_target = 1.0f;
            local->world_carriage.storybook_travel = !benchmark->roadbook_network;
            local->travel_time_blend = 1.0f;
        } else if (strcmp(benchmark->scene, "combat") == 0) {
            PrepareRoadCombatReel(sim, local);
        } else {
            RepositionHero(local, (Vector2){44.25f, 28.85f}, false);
            local->agent.facing_yaw = -0.35f;
            local->course.alarm_countdown = 1000.0f;
        }
    }
}

static bool CcRenderBenchmarkAfterFrame(CcRenderBenchmark *benchmark, double (*clock)(void))
{
    const int32_t warmup_frames = 60;
    if (benchmark->warmup_count < warmup_frames) {
        benchmark->warmup_count += 1;
        if (benchmark->warmup_count == warmup_frames) {
            CcLocalRendererResetPerformanceMetrics();
            benchmark->started = clock();
        }
        return false;
    }
    benchmark->count += 1;
    return benchmark->count >= benchmark->frames;
}

static int CcRenderBenchmarkReport(const CcRenderBenchmark *benchmark, double elapsed,
    CcLocalRendererStats final_renderer_stats, FILE *output, FILE *errors)
{
    if (benchmark->count <= 0 || !isfinite(elapsed) || elapsed <= 0.0) {
        (void)fprintf(errors, "Render benchmark elapsed time or frame count is invalid.\n");
        return 2;
    }
    double frames_per_second =
        (double)benchmark->count / elapsed;
    double p95_budget = benchmark->p95_budget > 0.0 ?
        benchmark->p95_budget :
        (benchmark->minimum_fps > 0.0 ?
            1500.0 / benchmark->minimum_fps : 0.0);
    (void)fprintf(output, "render: scene=%s frames=%d seconds=%.6f ms/frame=%.3f fps=%.1f p95=%.3f p95_budget=%.3f p99=%.3f max=%.3f hitches=%d skin_updates=%d skinned_meshes=%d hero_skin_updates=%d hero_skinned_meshes=%d npc_skin_updates=%d npc_skinned_meshes=%d creature_skin_updates=%d creature_skinned_meshes=%d high_detail=%d lod=%d static_batch_draws=%d static_batch_instances=%d static_batch_vertices=%d\n",
                 benchmark->scene, benchmark->count,
                 elapsed,
                 elapsed * 1000.0 /
                     (double)benchmark->count,
                 frames_per_second,
                 final_renderer_stats.p95_frame_milliseconds,
                 p95_budget,
                 final_renderer_stats.p99_frame_milliseconds,
                 final_renderer_stats.maximum_frame_milliseconds,
                 final_renderer_stats.hitch_count,
                 final_renderer_stats.skin_updates,
                 final_renderer_stats.skinned_meshes,
                 final_renderer_stats.hero_skin_updates,
                 final_renderer_stats.hero_skinned_meshes,
                 final_renderer_stats.npc_skin_updates,
                 final_renderer_stats.npc_skinned_meshes,
                 final_renderer_stats.creature_skin_updates,
                 final_renderer_stats.creature_skinned_meshes,
                 final_renderer_stats.high_detail_characters,
                 final_renderer_stats.low_detail_characters,
                 final_renderer_stats.static_batch_draws,
                 final_renderer_stats.static_batch_instances,
                 final_renderer_stats.static_batch_vertices);
    bool performance_failed = benchmark->minimum_fps > 0.0 &&
                              frames_per_second <
                                  benchmark->minimum_fps;
    bool frame_time_failed = p95_budget > 0.0 &&
        final_renderer_stats.p95_frame_milliseconds > p95_budget;
    bool scene_expects_lod =
        strcmp(benchmark->scene, "combat") == 0;
    bool hero_is_embarked = benchmark->roadbook;
    bool hero_layout_failed = hero_is_embarked ?
        final_renderer_stats.high_detail_characters != 0 :
        (final_renderer_stats.high_detail_characters != 1 ||
         (scene_expects_lod &&
          final_renderer_stats.low_detail_characters <= 0));
    bool skin_layout_failed = hero_is_embarked ?
        (final_renderer_stats.hero_skin_updates != 0 ||
         final_renderer_stats.hero_skinned_meshes != 0 ||
         hero_layout_failed) :
        (final_renderer_stats.hero_skin_updates != 1 ||
         final_renderer_stats.hero_skinned_meshes <= 0 ||
         final_renderer_stats.hero_skinned_meshes >
             CC_LOCAL_HERO_RUNTIME_MESH_BUDGET ||
         hero_layout_failed);
    if (performance_failed) {
        (void)fprintf(errors,
                      "render performance budget failed: %.1f FPS < %.1f FPS\n",
                      frames_per_second, benchmark->minimum_fps);
    }
    if (frame_time_failed) {
        (void)fprintf(errors,
                      "render frame-time budget failed: p95 %.1f ms > %.1f ms\n",
                      final_renderer_stats.p95_frame_milliseconds,
                      p95_budget);
    }
    if (skin_layout_failed) {
        (void)fprintf(errors,
                      "render hero skin budget failed: updates=%d meshes=%d hero=%d lod=%d (mesh budget %d)\n",
                      final_renderer_stats.hero_skin_updates,
                      final_renderer_stats.hero_skinned_meshes,
                      final_renderer_stats.high_detail_characters,
                      final_renderer_stats.low_detail_characters,
                      CC_LOCAL_HERO_RUNTIME_MESH_BUDGET);
    }
    if (performance_failed || frame_time_failed || skin_layout_failed) {
        return 2;
    }
    return 0;
}

static void TestBuildingConditions(void)
{
    CcLocalPlaceProfile profile = *CcLocalPlaceProfileForFunction(CC_SETTLEMENT_FARMING);
    CcSettlement place = {
        .id = 1U, .function = CC_SETTLEMENT_FARMING, .population = 30, .fire_damage = 45,
        .blueprint_id = CC_BLUEPRINT_THORNFORD,
        .blueprint_version = CC_BUILDING_BLUEPRINT_VERSION,
        .building_count = 2, .building_fire_level = 45,
        .buildings = {
            {.plot_id = 1U, .style_seed = 31U, .health = 15, .roof_health = 0,
             .upkeep = 20, .fire_damage = 90, .built_day = -100,
             .last_change_day = 290},
            {.plot_id = 2U, .style_seed = 32U, .health = 90, .roof_health = 90,
             .upkeep = 80, .repair_progress = 40, .built_day = -300,
             .last_change_day = 300, .last_repair_day = 300},
        },
    };
    BuildingAppearance burnt = BuildingAppearanceForPlot(&place, &profile, 0, 306);
    RequireRenderer(burnt.gutted && burnt.burn > 0.89f && burnt.snow > 0.0f,
                    "winter must cover the same saved burnt house");
    BuildingAppearance repairing = BuildingAppearanceForPlot(&place, &profile, 1, 306);
    RequireRenderer(!repairing.gutted && repairing.repair > 0.39f && repairing.fresh > 0.9f,
                    "funded repairs must retain progress and fresh material");
    uint32_t first_id = profile.building[0].plot_id;
    profile.building[0].plot_id = profile.building[1].plot_id;
    profile.building[1].plot_id = first_id;
    BuildingAppearance reordered = BuildingAppearanceForPlot(&place, &profile, 1, 306);
    RequireRenderer(reordered.seed == burnt.seed && reordered.burn == burnt.burn &&
                    reordered.snow == burnt.snow,
                    "house appearance must follow plot identity across profile order changes");
    BuildingAppearance summer = BuildingAppearanceForPlot(&place, &profile, 1, 110);
    RequireRenderer(summer.snow == 0.0f && summer.burn == burnt.burn,
                    "a season change must preserve house damage");
    BuildingAppearance rain = BuildingAppearanceForPlot(&place, &profile, 1, 98);
    BuildingAppearance dry = BuildingAppearanceForPlot(&place, &profile, 1, 96);
    RequireRenderer(rain.wetness > 0.80f && dry.wetness == 0.0f &&
                    rain.burn == dry.burn && rain.seed == dry.seed,
                    "rain must wet the saved house while keeping its damage and identity");
    CcLocalRendererSetAtmosphere(CC_LOCAL_ATMOSPHERE_MOONLIT_NIGHT, 0.0f);
    BuildingAppearance night = BuildingAppearanceForPlot(&place, &profile, 1, 306);
    CcLocalRendererSetAtmosphere(CC_LOCAL_ATMOSPHERE_CLEAR_DAY, 0.0f);
    RequireRenderer(night.snow == burnt.snow && night.burn == burnt.burn,
                    "night lighting must retain snow and damage");
    (void)puts("PASS saved house appearance, repair, seasons and plot identity");
}

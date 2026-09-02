if(NOT DEFINED CC_WEB_SOURCE_DIR OR NOT DEFINED CC_WEB_OUTPUT_DIR)
    message(FATAL_ERROR "Web asset source and output directories are required")
endif()

set(asset_source "${CC_WEB_SOURCE_DIR}/assets")
set(asset_output "${CC_WEB_OUTPUT_DIR}/assets")
set(lazy_asset_output "${CC_WEB_OUTPUT_DIR}/lazy-assets")
find_program(CC_WEB_PYTHON3_EXECUTABLE python3 REQUIRED)

file(REMOVE_RECURSE "${asset_output}")
file(REMOVE_RECURSE "${lazy_asset_output}")
file(MAKE_DIRECTORY
    "${asset_output}/exports/creatures"
    "${asset_output}/exports/glb"
    "${asset_output}/exports/npc"
    "${asset_output}/exports/world_kit"
    "${asset_output}/shaders"
    "${lazy_asset_output}/exports/glb"
    "${lazy_asset_output}/maps"
)

# The desktop renderer keeps authored stepped poses for every ambient role.
# The browser keeps one idle silhouette per role and falls back to it while
# walking. Combatants use the shared skinned body and modular pieces below.
file(GLOB npc_runtime_assets
    "${asset_source}/exports/npc/npc_module_*.glb"
    "${asset_source}/exports/npc/npc_wayfarer_v01.glb"
    "${asset_source}/exports/npc/npc_guard_v01.glb"
    "${asset_source}/exports/npc/npc_raider_v01.glb"
    "${asset_source}/exports/npc/npc_merchant_v01.glb"
    "${asset_source}/exports/npc/npc_laborer_v01.glb"
    "${asset_source}/exports/npc/npc_traveller_v01.glb"
    "${asset_source}/exports/npc/npc_refugee_v01.glb"
    "${asset_source}/exports/npc/npc_scout_v01.glb"
    "${asset_source}/exports/npc/npc_healer_v01.glb"
)
file(COPY ${npc_runtime_assets}
     DESTINATION "${asset_output}/exports/npc")

foreach(asset IN ITEMS
        creature_horse_v01.glb
        creature_cow_v01.glb
        creature_sheep_v01.glb
        creature_dragon_v01.glb
        creature_dragon_whelp_v01.glb
        creature_dragon_wanderer_v01.glb
        creature_dragon_deep_wyrm_v01.glb)
    file(COPY "${asset_source}/exports/creatures/${asset}"
         DESTINATION "${asset_output}/exports/creatures")
endforeach()

file(COPY "${asset_source}/exports/glb/carriage_base_v01.glb"
     DESTINATION "${asset_output}/exports/glb")

# Scene-specific rigid models stay available at their normal URLs, but are not
# copied into the startup data package. The browser downloads and uploads only
# the scene it is about to draw.
foreach(asset IN ITEMS
        environment_bridge_checkpoint_v01.glb
        environment_market_granary_v01.glb
        environment_mine_entrance_v01.glb
        state_food_shortage_v01.glb
        state_harsh_enforcement_v01.glb
        state_market_recovery_v01.glb)
    file(COPY "${asset_source}/exports/glb/${asset}"
         DESTINATION "${lazy_asset_output}/exports/glb")
endforeach()

file(GLOB world_kit_runtime_assets
    "${asset_source}/exports/world_kit/wk_body_skin_standard_athletic_balanced_v01.glb"
    "${asset_source}/exports/world_kit/wk_head_*.glb"
    "${asset_source}/exports/world_kit/wk_hair_*.glb"
)
file(COPY ${world_kit_runtime_assets}
     DESTINATION "${asset_output}/exports/world_kit")

# Authored source files keep their editable object boundaries. The deployed
# copies combine rigid parts by material so WebGL does not allocate hundreds
# of tiny vertex arrays and buffers.
set(web_batch_assets
    "${asset_output}/exports/glb/carriage_base_v01.glb"
)
file(GLOB web_head_hair_assets
    "${asset_output}/exports/world_kit/wk_head_*.glb"
    "${asset_output}/exports/world_kit/wk_hair_*.glb"
)
file(GLOB web_environment_assets
    "${lazy_asset_output}/exports/glb/*.glb"
)
list(APPEND web_batch_assets
    ${web_head_hair_assets}
    ${web_environment_assets}
)
foreach(asset IN LISTS web_batch_assets)
    execute_process(
        COMMAND "${CC_WEB_PYTHON3_EXECUTABLE}"
                "${CC_WEB_SOURCE_DIR}/tools/blender/batch_static_glb.py"
                "${asset}"
        RESULT_VARIABLE batch_result
    )
    if(NOT batch_result EQUAL 0)
        message(FATAL_ERROR "Could not batch web asset ${asset}")
    endif()
endforeach()

file(COPY
    "${asset_source}/maps/gloamgate_to_alderwatch.png"
    "${asset_source}/maps/collectible_map_atlas.png"
    DESTINATION "${lazy_asset_output}/maps"
)

file(GLOB shader_sources
    "${asset_source}/shaders/*.vs"
    "${asset_source}/shaders/*.fs"
)
foreach(shader IN LISTS shader_sources)
    get_filename_component(shader_name "${shader}" NAME)
    file(READ "${shader}" shader_text)
    string(REGEX REPLACE
        "^#version 330[\r\n]+"
        "#version 300 es\n\nprecision highp float;\nprecision highp int;\n"
        shader_text
        "${shader_text}"
    )
    file(WRITE "${asset_output}/shaders/${shader_name}" "${shader_text}")
endforeach()

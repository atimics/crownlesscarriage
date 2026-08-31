if(NOT DEFINED CC_WEB_SOURCE_DIR OR NOT DEFINED CC_WEB_OUTPUT_DIR)
    message(FATAL_ERROR "Web asset source and output directories are required")
endif()

set(asset_source "${CC_WEB_SOURCE_DIR}/assets")
set(asset_output "${CC_WEB_OUTPUT_DIR}/assets")

file(REMOVE_RECURSE "${asset_output}")
file(MAKE_DIRECTORY
    "${asset_output}/exports/hero"
    "${asset_output}/exports/glb"
    "${asset_output}/exports/world_kit"
    "${asset_output}/shaders"
)

foreach(directory IN ITEMS npc creatures)
    file(COPY "${asset_source}/exports/${directory}"
         DESTINATION "${asset_output}/exports")
endforeach()

file(COPY
    "${asset_source}/exports/hero/crownless_hero_engine_rig_v01.glb"
    DESTINATION "${asset_output}/exports/hero"
)

foreach(asset IN ITEMS
        environment_bridge_checkpoint_v01.glb
        carriage_base_v01.glb
        module_cargo_rack_v01.glb
        environment_market_granary_v01.glb
        environment_mine_entrance_v01.glb
        state_food_shortage_v01.glb
        state_harsh_enforcement_v01.glb
        state_market_recovery_v01.glb)
    file(COPY "${asset_source}/exports/glb/${asset}"
         DESTINATION "${asset_output}/exports/glb")
endforeach()

file(GLOB world_kit_runtime_assets
    "${asset_source}/exports/world_kit/wk_body_skin_*.glb"
    "${asset_source}/exports/world_kit/wk_head_*.glb"
    "${asset_source}/exports/world_kit/wk_hair_*.glb"
)
file(COPY ${world_kit_runtime_assets}
     DESTINATION "${asset_output}/exports/world_kit")

file(COPY "${asset_source}/maps" DESTINATION "${asset_output}")

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

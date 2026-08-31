if(NOT DEFINED CC_WEB_SOURCE_DIR OR NOT DEFINED CC_WEB_OUTPUT_DIR)
    message(FATAL_ERROR "Web asset source and output directories are required")
endif()

set(asset_source "${CC_WEB_SOURCE_DIR}/assets")
set(asset_output "${CC_WEB_OUTPUT_DIR}/assets")

file(REMOVE_RECURSE "${asset_output}")
file(MAKE_DIRECTORY "${asset_output}/exports" "${asset_output}/shaders")

foreach(directory IN ITEMS hero npc creatures glb world_kit)
    file(COPY "${asset_source}/exports/${directory}"
         DESTINATION "${asset_output}/exports")
endforeach()

file(COPY "${asset_source}/maps" DESTINATION "${asset_output}")

foreach(manifest IN ITEMS
        creature_manifest.json
        npc_archetype_manifest.json
        npc_dynamic_module_manifest.json
        world_kit_manifest.json
        world_kit_connections.json)
    file(COPY "${asset_source}/${manifest}" DESTINATION "${asset_output}")
endforeach()

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

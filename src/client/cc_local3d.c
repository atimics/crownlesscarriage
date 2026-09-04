/*
 * Local renderer unity root.
 *
 * The focused modules stay in source order so they can share private types and
 * helpers. CMake builds this translation unit once in crownless_local_renderer.
 * See docs/RENDERER_ARCHITECTURE.md for the module and lifecycle contracts.
 */
#define CC_LOCAL3D_UNITY_BUILD 1
#include "client/local3d/context_state.inc"
#include "client/local3d/terrain_navigation.inc"
#include "client/local3d/actor_simulation.inc"
#include "client/local3d/camera_composition.inc"
#include "client/local3d/asset_loading.inc"
#include "client/local3d/authored_places.inc"
#include "client/local3d/heraldry.inc"
#include "client/local3d/actor_rendering.inc"
#include "client/local3d/road_book.inc"
#include "client/local3d/open_world.inc"

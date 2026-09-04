.PHONY: art-assets-check blender-assets blender-assets-catalog blender-assets-check \
	blender-exports-check blender-hero-assets blender-hero-assets-check blender-hero-animation \
	blender-hero-actions blender-hero-engine blender-hero-procedural-preview \
	blender-hero-procedural-check blender-hero-paint-channels \
	blender-npc-assets blender-npc-assets-check \
	blender-creature-assets blender-creature-assets-check \
	blender-world-kit blender-world-kit-check blender-world-kit-review-check \
	blender-character-experiments blender-character-animations blender-character-animations-check \
	blender-character-hair-v08 \
	blender-painted-market-pilot \
	blender-character-engine \
	art-check \
	configure-play build-play test-play \
	configure-release build-release test-release \
	configure-web build-web

BLENDER ?= blender

art-assets-check:
	python3 tools/art/check_asset_inventory.py

configure-play:
	cmake --preset play

build-play: configure-play
	cmake --build --preset play

test-play: build-play
	ctest --preset play

configure-release:
	cmake --preset release

build-release: configure-release
	cmake --build --preset release

test-release: build-release
	ctest --preset release

configure-web:
	emcmake cmake --preset web

build-web: configure-web
	cmake --build --preset web

blender-assets:
	$(BLENDER) --background --factory-startup --python tools/blender/build_asset_library.py

blender-assets-catalog:
	$(BLENDER) --background assets/blender/crownless_asset_library.blend --python tools/blender/render_asset_catalog.py
	python3 tools/blender/compose_asset_catalog.py

blender-assets-check:
	$(BLENDER) --background assets/blender/crownless_asset_library.blend --python tools/blender/validate_asset_library.py

blender-exports-check:
	python3 tools/blender/inspect_glb.py --profile library --manifest assets/asset_manifest.json --export-dir assets/exports/glb assets/exports/glb/*.glb
	python3 tools/blender/inspect_glb.py assets/exports/hero_glb/*.glb assets/exports/hero/*.glb

blender-hero-assets:
	$(BLENDER) --background --python-exit-code 1 --factory-startup --python tools/blender/build_hero_component_library.py
	python3 tools/blender/compose_hero_preview.py

blender-hero-assets-check:
	$(BLENDER) --background --python-exit-code 1 assets/blender/crownless_hero_components.blend --python tools/blender/validate_hero_component_library.py

blender-hero-animation:
	$(BLENDER) --background --python-exit-code 1 assets/blender/crownless_hero_components.blend --python tools/blender/render_hero_animation.py
	python3 tools/blender/encode_hero_animation.py

blender-hero-actions:
	$(BLENDER) --background --python-exit-code 1 assets/blender/crownless_hero_components.blend --python tools/blender/render_hero_actions.py
	python3 tools/blender/encode_hero_actions.py

blender-hero-engine: blender-hero-assets blender-hero-assets-check
	$(BLENDER) --background --python-exit-code 1 assets/blender/crownless_hero_components.blend --python tools/blender/render_hero_actions.py -- --preview
	$(BLENDER) --background --python-exit-code 1 assets/blender/crownless_hero_actions.blend --python tools/blender/export_engine_hero.py

blender-hero-paint-channels: blender-character-engine
	$(BLENDER) --background --python-exit-code 1 assets/blender/crownless_hero_components.blend --python tools/blender/export_engine_hero.py
	python3 tools/blender/validate_character_paint_channels.py

blender-hero-procedural-preview:
	$(BLENDER) --background --python-exit-code 1 --factory-startup --python tools/blender/render_procedural_character_variants.py

blender-hero-procedural-check:
	python3 tools/blender/procedural_character.py

blender-character-experiments:
	$(BLENDER) --background --python-exit-code 1 --factory-startup --python tools/blender/render_screen_first_character_experiments.py
	python3 tools/blender/compose_screen_first_character_experiments.py

blender-character-animations:
	$(BLENDER) --background --python-exit-code 1 --factory-startup --python tools/blender/render_screen_first_character_animation.py
	python3 tools/blender/validate_screen_first_character_animation.py
	python3 tools/blender/compose_screen_first_character_animation.py

blender-character-animations-check:
	python3 tools/blender/validate_screen_first_character_animation.py

blender-painted-market-pilot:
	$(BLENDER) --background --python-exit-code 1 --factory-startup --python tools/blender/build_painted_market_pilot.py
	python3 tools/blender/inspect_glb.py --profile library assets/exports/glb/environment_market_granary_v01.glb

blender-character-engine:
	$(BLENDER) --background --python-exit-code 1 assets/blender/crownless_hero_actions.blend --python tools/blender/export_screen_first_engine_hero.py
	python3 tools/blender/inspect_glb.py assets/exports/hero/crownless_screen_first_engine_rig_v08.glb

blender-character-hair-v08: blender-character-engine
	$(BLENDER) --background --python-exit-code 1 assets/blender/crownless_hero_actions.blend --python tools/blender/render_screen_first_hair_v08.py
	python3 tools/blender/compose_screen_first_hair_v08.py
	$(BLENDER) --background --python-exit-code 1 assets/blender/crownless_hero_actions.blend --python tools/blender/render_screen_first_character_sheet_v08.py
	python3 tools/blender/compose_screen_first_character_sheet_v08.py

blender-npc-assets:
	$(BLENDER) --background --python-exit-code 1 --factory-startup --python tools/blender/build_npc_archetype_library.py
	$(BLENDER) --background --python-exit-code 1 --factory-startup --python tools/blender/build_npc_dynamic_modules.py

blender-npc-assets-check:
	python3 tools/blender/validate_npc_archetype_library.py
	python3 tools/blender/validate_npc_dynamic_modules.py

blender-creature-assets:
	$(BLENDER) --background --python-exit-code 1 --factory-startup --python tools/blender/build_creature_library.py

blender-creature-assets-check:
	python3 tools/blender/validate_creature_library.py

blender-world-kit:
	$(BLENDER) --background --python-exit-code 1 --factory-startup --python tools/blender/build_world_kit.py

blender-world-kit-check:
	python3 tools/blender/validate_world_kit.py

blender-world-kit-review-check:
	python3 tools/blender/validate_world_kit.py --review-previews

art-check: art-assets-check test-play blender-character-animations-check blender-npc-assets-check blender-creature-assets-check blender-world-kit-check
	python3 tools/blender/validate_character_paint_channels.py
	python3 tools/art/run_art_check.py

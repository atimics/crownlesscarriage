.PHONY: blender-assets blender-assets-catalog blender-assets-check \
	blender-exports-check blender-hero-assets blender-hero-assets-check blender-hero-animation \
	blender-hero-actions blender-hero-engine blender-hero-procedural-preview \
	blender-hero-procedural-check configure-play build-play test-play \
	configure-release build-release test-release

BLENDER ?= blender

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

blender-hero-procedural-preview:
	$(BLENDER) --background --python-exit-code 1 --factory-startup --python tools/blender/render_procedural_character_variants.py

blender-hero-procedural-check:
	python3 tools/blender/procedural_character.py

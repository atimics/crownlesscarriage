# Test registration files

PR #683 addresses the test-registration portion of issue #640. Eight area files hold the existing declarations. The root discovers sorted `cmake/tests/*.cmake` files when `BUILD_TESTING` is enabled. `CONFIGURE_DEPENDS` triggers regeneration after files are added or removed.

All 902 original registration lines are preserved in order, apart from leading indentation. This includes platform conditions, interpreter searches, target links, options, and test properties.

## Verification

The File API target descriptions and generated CTest commands matched before and after in the same build directories:

| Configuration | Targets | Tests |
| --- | ---: | ---: |
| Headless Release | 164 | 135 |
| Native Release | 192 | 173 |
| Tests disabled | 30 | 0 |
| Benchmarks disabled | 144 | 114 |

`snapshot.py` excludes source backtraces and sorts dependency IDs. It preserves source lists, compiler options, ordered link commands, and test properties. `comparison.json` records matching normalized hashes. The comparison was run on macOS with the configured Node and Python interpreters. Native target settings were compared through configuration; the native client was not rebuilt for this CMake-only change.

The Release headless build passed with strict warnings enabled. Seven representative tests passed: license page, macOS configuration, site wear, gossip corpus, archive convoy, SQLite round trip, and the shared carriage server.

A temporary registration file added one probe test. A normal build discovered it and regenerated CMake; the probe passed. Deleting the file caused the next normal build to remove its test registration. `discovery.json` records those checks. The temporary file was removed.

## Reproduce

Use an isolated worktree and the same build paths before and after the implementation revision. For each configuration, create `.cmake/api/v1/query/codemodel-v2` in its build directory, configure, and run:

```sh
python3 docs/reviews/test-registration-2026-09-12/snapshot.py <build-directory> <output.json>
```

Compare the normalized target descriptions and generated tests. Keep the toolchain, configuration options, source path, and build path the same across the two snapshots. Build and run the tests after moving the registrations.

The contributor guide is `cmake/tests/README.md`. The experiment-artifact size convention from #640 remains separate work.

## Container follow-through

Remote CI found that the shared-world Docker build copied `tests/` while omitting the newly required `cmake/` files. The Docker recipe now copies that directory, and `.dockerignore` includes it. CMake reports a clear error when testing is enabled and registration files are missing.

The native inputs declared by the Docker recipe were staged in a temporary source tree. Configuration, the `coop_bridge_tests` build, and both shared-carriage tests passed. A staged package with the registration directory removed produced the expected diagnostic. The local Docker daemon was unavailable, so the complete image build remains a remote CI check.

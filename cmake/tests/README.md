# Test registration

CMake includes every `*.cmake` file here in sorted order when `BUILD_TESTING` is enabled. `CONFIGURE_DEPENDS` makes the build reconfigure when a registration file is added or removed.

Add a file for a new test area. Keep its targets, links, compiler settings, tests, and test properties together. Keep platform, client, benchmark, and interpreter conditions around the registrations that need them. Python and Node tests can keep their full argument lists, including target-file expressions.

The numbered files preserve the existing registration order. Add independent files after them, or choose an earlier position when an explicit dependency requires it. Paths stay relative to the project source directory: use `CMAKE_CURRENT_SOURCE_DIR` for scripts and assets. Includes share the top-level directory scope.

A new test area needs its registration file and sources. CMake discovers the file automatically. Configure and run the relevant tests before opening a PR.

# Research artifact budget

This draft completes the convention part of issue #640 on top of the test-registration draft #683.

The checker reads exact Git trees and measures unique new blobs under both research directories. Defaults are 1 MiB per blob and 2 MiB total per PR. Existing content is recognized by blob ID across the whole base tree, so a move or copy of an existing large file retains its existing status. A changed large file receives a new blob ID and uses the current budget.

Seven tests cover existing large content, new oversize files, deduplication, exact boundaries, total growth, replacement, unrelated paths, deletions, and actual Git filenames containing spaces and tabs. The real-tree test also checks CLI success and failure exits. Tests use temporary local repositories.

The workflow uses read-only permissions and the PR's exact base and head commits. The new CTest registration lives in its own file, exercising the registration convention from #683. The guide covers figure formats, compressed sampling, source provenance, checksums, and durable external storage.

The inventory motivating the default included existing studies at 25.8 MB, 16.4 MB, and 7.8 MB on the base branch. This change preserves those historical trees. The limits are a proposed convention for review in this draft.

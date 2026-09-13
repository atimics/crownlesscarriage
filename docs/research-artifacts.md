# Research artifacts

Keep research under `docs/experiments/<study>/` or `docs/reviews/<study>/`. A study should include its question, commands, seeds or inputs, source revision, result summary, and the limits of its evidence.

## Repository budget

The default PR budget is **1 MiB per new research blob** and **2 MiB total new research blobs**. One MiB is 1,048,576 bytes. The checker measures the bytes Git stores in the file, including compression already applied to the file.

The comparison uses the PR's base and head trees. A blob already present anywhere in the base tree is existing content. Renaming or copying it retains that status. Identical new blobs count once toward the total, with every research path listed in the report. Replacing a historical file creates a new blob and uses the current budget. Existing historical studies remain available.

Run the same check locally after committing:

```sh
python3 tools/check_research_artifacts.py --base origin/main --head HEAD
```

Use `--json` for a machine-readable report. Fetch both revisions before running the check. The PR workflow uses its exact base and head commits. Local byte-limit options support planning; the workflow uses the defaults above.

## Choose compact evidence

- Keep the reproducible source and a useful summary in Git.
- Compress raw tables, and select a sampling interval suited to the claim. Record the full-run size, selected interval, and any coverage gaps.
- Prefer SVG for plots. Use PNG or JPEG for screenshots and raster evidence. Keep one primary format for each figure; explain a second format when it serves a separate review need.
- Keep failed runs and partial coverage visible in the summary and manifest.

For larger raw results, use a durable release attachment or managed artifact store. Commit a manifest with the download link, SHA-256, exact byte count, generation command, input identity, and retention policy. CI artifacts can support short reviews; record their expiry and arrange durable storage for evidence that must remain available. Uploading or publishing data follows the normal project approval rules.

Large existing files can be moved with their original content. For a revised large result, keep a compact summary in the study and place the full result in the artifact store. This keeps the repository budget tied to new data rather than to the age or name of a study folder.

# Saved-world recovery check

Issue #614 asks whether the affected shared world can load, retain its players, accept a normal action, and survive a host restart. The public service is running `e67b6f82a7ae69e3f20ebe7081f4dad3c84de9c2` and `/healthz` reports `ready` with zero worlds needing recovery. The release workflow and its current-main checks passed.

A consistent private backup of the live host database was made after explicit owner approval. It contains three saved worlds and seven existing members. The backup stays in a private local directory with restricted permissions. Player records, world IDs, tokens, and saved blobs are outside the repository.

The current engine decoded all three saved states without repair and reproduced each cached view. An isolated copy of each world accepted a normal travel action to a route offered by its own state. A fresh host process loaded each resulting state. Replaying each command returned one existing receipt and left its revision unchanged. The original members, appearances, sessions, lives, starts, and previous receipts matched the backup. The backup bytes stayed unchanged throughout the check. These actions were applied only to isolated copies.

The focused `test_historical_cast_recovery_preserves_backup_players_and_retry` regression uses the shipped schema-73 retired-cast fixture, the kind of old save that previously failed validation. It confirms the older record upgrades, player records survive, travel is accepted, and retry after restart applies once. It also checks a consistent pre-recovery backup and its SQLite integrity. The focused shared-carriage and quest-cast checks passed.

The retained evidence for the 8 September failure is the historical error report and a public synthetic fixture. This check covers every saved world currently on the host and the earlier failure shape.

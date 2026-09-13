# Crownless dialogue pair review

Twelve recorded simulation accounts, each with two assistant-written conversations. These are editorial drafts for human choice. Training remains paused.

Open `index.html` directly or serve this folder. Choose A, B, or skip. The page shows one pair at a time and pauses after five, ten, and twelve decisions. Undo revisits the last choice. Optional notes and choices save in this browser. Use **Save choices to a file** to export a portable JSON record and share it with the next writing pass.

## What is recorded and what is written

The held accounts come from the real-world simulation sample in [the source review](../synthetic-speech-2026-09-12/REVIEW.md). `sources.jsonl` keeps the twelve original rows intact, including world seed, event ID, holder ID, observation day, confidence, and circulation. The selection contains twelve event kinds with four clear, four retold, and four uncertain accounts.

The conversations, meetings, listener, questions, hopes, and reactions are authored. A source holder's occupation or relationship does not establish a scene aim. Each first speaker holds the selected account. The listener learns about it through the exchange. The authored aim is shared by both versions and is available under **About this situation**.

Both drafts preserve general quantities and the scope of the held account. Questions about seed, an unnamed treasure, or a craft process leave space for incomplete knowledge. Hopes and wishes express a response. Spoken promises and decisions remain dialogue; any later game action needs simulation support.

These twelve accounts were chosen for editorial range. They cover bread, repair delays, a harvest, bandit recruitment, a goblin promotion, a raid, calving, stone, a bridge, paper, food reserves, and a crown. The goblin promotion concerns a goblin; the drafts leave speaker species unspecified. Additional embodied scenes can use recorded species and sensory context in a later study.

## How to read the choices

The review asks which exchange the user would rather hear in Crownless. Useful signs are a responsive next line, ordinary language, a distinct concern, and an ending that fits the exchange. The two candidates have stable concealed IDs; their A/B positions alternate across pairs. The exported decisions retain the displayed side and candidate ID.

This is a small editorial preference study. It compares authored targets and supplies direction for the next writing pass. A later model experiment can test whether the chosen qualities appear in fresh generations. Keep these review situations separate from held-out model evaluation worlds.

The current model accepts a prepared account, uncertainty, circulation, and recent speech. When scene aims, occupations, or relationships become training conditions, the live game must supply the same supported context. Plain event lines and spoken continuation remain the target format. `dialogues.txt` provides a readable copy of all drafts; the JSON files hold review and provenance data.

## Craft basis

The prior research suggested clear wants, concrete concern, varied rhythm, and lived perspectives. Relevant sources include [Milne's Winnie-the-Pooh](https://www.gutenberg.org/ebooks/67098), [Grahame's The Wind in the Willows](https://www.gutenberg.org/ebooks/289), [Kyell Gold on dialogue](https://kyellgold.substack.com/p/writing-advice-dialogue), and [Jess E. Owen on animal characters](https://furrywritersguild.com/2015/03/10/guest-post-5-tips-by-jess-e-owen/). These drafts are original writing based on Crownless records.

## Rebuild and evidence

Run `python3 docs/reviews/dialogue-pairs-2026-09-12/build.py` from the repository root. It checks source-row hashes, account wording, confidence, circulation, provenance, event diversity, and turn counts before producing the offline page, plain text, and file manifest. The study digest binds saved choices to the exact scene set; editing it creates a fresh review identity.

The offline page contains its own data. It uses browser storage for progress and offers file export. The review collects choices locally. Source files and all 24 drafts remain available for inspection.

Validation completed on 12 September 2026: the builder verified all twelve source links and produced stable output on a repeat build. Browser checks covered A and B choices, skipping, note retention, reload/resume, undo, pauses at five and ten, completion at twelve, and undo from completion. The export button ran successfully. A separate local address held the test choices; the user-facing review starts empty. The desktop layout was inspected visually.

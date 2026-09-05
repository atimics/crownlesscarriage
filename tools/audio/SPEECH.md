# Procedural speech

`CcSpeech` holds the exact words, speaker, voice, delivery, priority, and source
event for a spoken turn. The story module creates these records from the same
state used for dialogue text. Its builders leave the simulation unchanged.

The version-one cast has five named voices and eight voices for generated
people. Each profile includes a description for voice design. A generated
person's assignment comes from the world seed, birthplace, and character ID.
Local people use the place and interaction object ID. Travel, work, stress, and
appearance changes preserve the assignment. Successors use their own IDs.
Keep the first eight generated profiles and their selection rule fixed when
extending this cast. Give a revised reference recording a new profile version.

Recordings share a key when their cast profile, language, delivery, and exact
words match. The key includes the speech format version. This allows people
with a shared voice to reuse a greeting. Character and source event IDs remain
on the turn so playback can follow the correct person and event. Cache tools
should validate the complete record before reusing a file with the same key.

The speech builder also gives generated mine testimony a line ID. This lets
successor characters use the same speech path as the opening cast.

Run the foundation checks with `ctest --test-dir <build> -R
'speech_identity|authored_story' --output-on-failure`.

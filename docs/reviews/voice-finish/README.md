# Pocket voice finish

A reusable offline finishing pass for mono 16-bit speech masters. It runs after Pocket generation and before delivery encoding. Use the clean masters as input. The listening trial covers all 24 takes from the second audition, including original and revised cast voices.

The preset uses a 65 Hz high-pass filter, up to 4 dB of bright-band reduction, soft-knee 2:1 compression with 12 ms attack and 140 ms release, a -20 dBFS active speech RMS target, and 6 ms edge fades. A whole-line gain trim keeps four-times-oversampled peaks at or below -2 dBFS. Makeup gain is capped at 6 dB. Active speech RMS uses 20 ms windows above -50 dBFS and within 20 dB of the loudest window. This is an active RMS target; listening remains the cast-quality check.

The command preserves its source folder and writes separate finished masters, raw copies, and a listening pair at matched active speech level. Any headroom trim needed by the listening pair applies to both players. The `master.wav` files hold the final delivery level. Receipts record the preset, source/output hashes, and measured levels.

```sh
python tools/audio/voice_finish.py --input PATH_TO_CLEAN_WAVS --output PATH_TO_FINISHED_AUDIO
python tests/voice_finish_tests.py
```

Dependencies: NumPy and SciPy, available in the existing Pocket environment. The pass is opt-in through this command. The game's speech worker can adopt it after listening review.

The full listening page and lossless masters are saved locally at `/Users/ratimics/develop/crownless/out/voice-finish/index.html`. The complete ZIP is alongside that folder. This review folder includes compact matched/finished examples for the six revised bargain lines and receipts for all 24 takes.

Validation covers silence, invalid input, repeatability, edge fades, duration preservation, peak headroom, compression, low-rumble reduction, and bright-band reduction. All 72 browser players loaded with positive durations and zero audio errors.

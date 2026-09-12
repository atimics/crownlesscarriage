# Crownless conversation core

This is ZERO's 4,935,937-parameter conversation model, trained on held simulation accounts and prior spoken lines. The model and tokenizer use the included MIT license.

- Source: https://github.com/atimics/zero/pull/37
- Model SHA256: `244a809aba71679efe129495bb981ca8d7fbe0d9012948a5f88731dabcb9c48b`
- Tokenizer SHA256: `c572de53eb4e739a8ce941ac03d1d5fb6173623af786787cf786b41fd34e9af4`
- Grammar SHA256: `ad017639e911c482e86e25f063207504769353dcd05e263d31dd9a6ef9409fb6`

The C runtime loads the original row-scaled 8-bit artifact and expands weights to floats. Attention memory belongs to the caller. The step API spreads work across frames. Each completed reply can become a spoken event for the next avatar. Held accounts supply meaning, knowledge labels, and literal copy fields.

Run `python3 tools/language/compile_model.py --check` to verify the compiled tokenizer and tensor tables. Their Unicode category ranges are pinned in the compiler inputs. The saved model has 61 meanings across 44 event kinds. Its report records 1,247/1,250 approved main responses and 1,124/1,250 with new question wording. Broader dialogue remains experimental.

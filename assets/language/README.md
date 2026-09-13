# Crownless conversation core

This is ZERO's 4,935,937-parameter conversation model, trained on held simulation accounts and prior spoken lines. The model and tokenizer use the included MIT license.

- Source: https://github.com/atimics/zero/pull/37
- Model SHA256: `244a809aba71679efe129495bb981ca8d7fbe0d9012948a5f88731dabcb9c48b`
- Tokenizer SHA256: `c572de53eb4e739a8ce941ac03d1d5fb6173623af786787cf786b41fd34e9af4`
- Grammar SHA256: `ad017639e911c482e86e25f063207504769353dcd05e263d31dd9a6ef9409fb6`

The C runtime loads the original row-scaled 8-bit artifact and expands weights to floats. Attention memory belongs to the caller. The step API spreads work across frames. Each completed reply can become a spoken event for the next avatar. Held accounts supply meaning, knowledge labels, and literal copy fields.

Run `python3 tools/language/compile_model.py --check` to verify the compiled tokenizer and tensor tables. Their Unicode category ranges are pinned in the compiler inputs. The saved model has 61 meanings across 44 event kinds. Its report records 1,247/1,250 approved main responses and 1,124/1,250 with new question wording. Broader dialogue remains experimental.

Ordinary conversations offer **Chat** and **Farewell**. Chat exchanges news through the simulation command, then generates the player's line from the player's held version. The NPC receives that exact spoken line alongside their own held version. The last four spoken events form the conversation history. The screen retains the player's line above the NPC reply. Leaving the conversation clears its local history.

The model file occupies 5,044,766 bytes. The runtime uses about 26 MB for expanded weights and attention memory. Native builds include the model beside the game. Browser builds fetch it as a separate asset so it can be cached independently of the startup pack. The compiled tokenizer supplies the runtime vocabulary.

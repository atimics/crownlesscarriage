# Direct semantic IDs

The semantic-ID model consumes integer IDs and predicts integer IDs. It bypasses
the byte-pair text tokenizer in both directions. JSON is a debug view of the act.
The existing 5M architecture and 4,096-row embedding table are retained; selected
rows have version-one semantic meanings. The output projection scores only the
51 legal vocabulary IDs, including EOS. It applies a vocabulary mask rather
than a position-dependent grammar mask. The codec and state validator check
the predicted act afterward.

Each output is three IDs followed by EOS:

```text
PROPOSE  SEEK_PAID_WORK  REPLY_0  EOS
11       38              64       0
```

Its serialized act record is `03 06 00 00`: opcode byte 3, argument byte 6,
little-endian uint16 reply index 0. Every version-one act record is four bytes.
`0xffff` means no reply. Version is carried by the enclosing protocol; records
are not self-describing. Reply indices are bounded to 0–31.

The input has a version token, state marker, goal, hunger/distress predicates,
courage, four coin bytes, then each public act with a self/other tag. It ends
with a next-act marker. `semantic_ids.py` defines the versioned mappings and
bounds. Names and other strings remain in the caller's participant record.
The current knowledge scope is audited in [KNOWLEDGE-COVERAGE.md](KNOWLEDGE-COVERAGE.md).

## Train and run

```sh
python tools/dialogue/train_semantic_ids.py --zero /path/to/zero \
  --reference models/dialogue-syntax/model.ccv2 \
  --tokenizer assets/language/tokenizer.json --output /tmp/semantic-run --steps 800
python tools/dialogue/build_syntax_probe.py --model /tmp/semantic-run/last.ccv2 \
  --build /path/to/build --output /tmp/semantic-native
python tools/dialogue/syntax_student.py --semantic-ids --snapshot /tmp/pair.json \
  --model /tmp/semantic-run/last.ccv2 --probe /tmp/semantic-native/probe \
  --surface-probe /path/to/build/core_account_probe --output /tmp/semantic-pair
```

The old tokenizer is used during dataset construction to measure the JSON
baseline lengths and to preserve the export container's metadata. Training loss
and inference operate directly on semantic IDs. The native API is
`CcCoreModelBeginSemantic` plus `CcCoreModelSemanticTokens`. The command-line
probe prints numeric IDs for inspection; it calls the integer API directly.
The pair result includes `binary_hex` for the four-byte record, the decoded act
and its human/goblin renderings.

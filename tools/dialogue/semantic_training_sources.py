"""Select source changes that require a semantic 5M training run."""
import sys

RELEVANT_PREFIXES = (
    ".github/workflows/dialogue-semantic-training.yml",
    "tools/dialogue/",
    "tools/data/core_account_rules.json",
    "models/dialogue-syntax/",
    "assets/language/",
    "CMakeLists.txt",
    "src/story/",
    "src/sim/",
    "src/rules/",
    "tools/core_model_probe.c",
)


def is_relevant(paths):
    return any(path == prefix or path.startswith(prefix) for path in paths for prefix in RELEVANT_PREFIXES)


if __name__ == "__main__":
    print("true" if is_relevant(line.strip() for line in sys.stdin if line.strip()) else "false")

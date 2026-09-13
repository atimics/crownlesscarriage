#!/usr/bin/env python3
"""Compare the shared topic rules with the original probe on main."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
BASE = "4f7635cc3a115492470a60ef9223dab0d3c859db"
original = subprocess.check_output(
    ["git", "show", f"{BASE}:tools/letter_probe.c"], cwd=ROOT, text=True
)
start = original.index("static bool TopicMatches(")
end = original.index("static const char *TopicName(", start)
control = original[start:end]
with tempfile.TemporaryDirectory(prefix="gossip-topic-parity-") as directory:
    source = Path(directory) / "parity.c"
    binary = Path(directory) / "parity"
    source.write_text(
        '#include "sim/cc_gossip_topics.h"\n#include <stdio.h>\n'
        + control
        + """
int main(void) {
    int count = 0;
    for (int topic = 0; topic < CC_GOSSIP_TOPIC_COUNT; ++topic)
        for (int kind = 0; kind <= CC_EVENT_ROAD_SITE_PRODUCTION; ++kind) {
            if (TopicMatches(topic, (CcEventKind)kind) !=
                CcGossipTopicMatches((CcGossipTopic)topic, (CcEventKind)kind))
                return 1;
            ++count;
        }
    printf("%d topic/event decisions match the original probe.\\n", count);
    return 0;
}
"""
    )
    subprocess.run(
        ["cc", "-std=c17", "-Wall", "-Wextra", "-Werror", "-Isrc",
         str(source), "src/sim/cc_gossip_topics.c", "-o", str(binary)],
        cwd=ROOT, check=True,
    )
    subprocess.run([str(binary)], check=True)

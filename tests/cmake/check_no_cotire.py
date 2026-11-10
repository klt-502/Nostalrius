"""Check real compiler commands, including after an ON -> OFF cache switch."""
import json
from pathlib import Path
import shlex
import sys

commands = json.loads(Path(sys.argv[1]).read_text())
assert commands, "Expected generated compiler commands"
for entry in commands:
    arguments = entry.get("arguments") or shlex.split(entry["command"])
    assert not any("cotire/" in arg.replace("\\", "/") or "cmake_pch" in arg
                   or arg.endswith((".gch", ".pch")) or arg in {"-fpch-preprocess", "-include", "-include-pch"}
                   for arg in arguments), entry
print(f"Verified {len(commands)} compile commands: no cotire/forced prefix header/PCH")

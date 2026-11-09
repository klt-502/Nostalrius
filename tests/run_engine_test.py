"""Run native engine tests in a throwaway data directory, without a DB/client."""
import pathlib
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

binary, source, scenario = sys.argv[1:]
source = pathlib.Path(source)
with tempfile.TemporaryDirectory(prefix="zedas-engine-") as directory:
    root = pathlib.Path(directory)
    for path in ("data/lib", "data/items", "data/XML", "data/spells/lib", "data/actions/lib",
                 "data/talkactions/lib", "data/talkactions/scripts/easymode"):
        shutil.copytree(source / path, root / path)
    for path in ("data/global.lua", "data/actions/scripts/misc/fluids.lua",
                 "data/spells/scripts/runes/ultimate healing.lua",
                 "data/spells/scripts/runes/great fireball.lua",
                 "data/spells/scripts/runes/explosion.lua"):
        (root / path).parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source / path, root / path)
    spells = ET.Element("spells")
    for node in ET.parse(source / "data/spells/spells.xml").getroot():
        if node.tag == "rune" and node.get("id") in {"3160", "3191", "3200"}:
            spells.append(node)
    ET.ElementTree(spells).write(root / "data/spells/spells.xml")
    talks = ET.Element("talkactions")
    for node in ET.parse(source / "data/talkactions/talkactions.xml").getroot():
        if node.get("script", "").startswith("easymode/"):
            talks.append(node)
    ET.ElementTree(talks).write(root / "data/talkactions/talkactions.xml")
    (root / "data/actions/actions.xml").write_text(
        '<actions><action itemid="2874" script="misc/fluids.lua" /></actions>')
    enabled = "false" if scenario == "disabled" else '"invalid"' if scenario == "invalid_config" else "true"
    endless = "false" if scenario == "feature_disabled" else "true"
    config = (
        f'easyModeEnabled = {enabled}\n'
        f'easyModeEndlessRunes = {endless}\n'
        f'easyModeEndlessManaFluid = {endless}\n'
        'easyModeCommands = true\neasyModeAutoTarget = true\neasyModeAutoGold = true\n'
        'removeChargesFromRunes = true\ntimeBetweenExActions = 25\n'
        'autoStackCumulatives = false\nqueryPlayerContainers = false\n')
    if scenario == "defaults":
        config = 'removeChargesFromRunes = true\ntimeBetweenExActions = 25\n'
    if scenario == "bag_enabled":
        config += 'easyModeGoldBag = true\n'
    for scenario_name, key in (("commands_disabled", "easyModeCommands"),
                               ("target_disabled", "easyModeAutoTarget"),
                               ("gold_disabled", "easyModeAutoGold")):
        if scenario == scenario_name:
            config = config.replace(key + " = true", key + " = false")
    (root / "config.lua").write_text(config)
    if scenario == "invalid_config":
        # Exercise each startup setting independently, including newly reserved flags.
        valid = config.replace('"invalid"', 'false')
        for key in ("easyModeEnabled", "easyModeEndlessRunes", "easyModeEndlessManaFluid",
                    "easyModeCommands", "easyModeAutoTarget", "easyModeAutoGold",
                    "easyModeLootSpill", "easyModeGoldBag", "easyModeRespawn"):
            (root / "config.lua").write_text(valid + f'\n{key} = "invalid"\n')
            subprocess.run([binary, scenario], cwd=root, check=True)
        (root / "config.lua").write_text(valid + '\neasyModeRespawnPolicyFile = 123\n')
    subprocess.run([binary, scenario], cwd=root, check=True)

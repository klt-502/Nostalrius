"""Run native engine tests in a throwaway data directory, without a DB/client."""
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

binary, source, scenario = sys.argv[1:]
source = pathlib.Path(source)
if scenario in {"broker_witness", "broker", "broker_restart", "broker_feedback"} and not os.environ.get("ZEDAS_BROKER_TEST_PORT"):
    sys.exit(77)
with tempfile.TemporaryDirectory(prefix="zedas-engine-") as directory:
    root = pathlib.Path(directory)
    for path in ("data/lib", "data/items", "data/XML", "data/spells/lib", "data/actions/lib",
                 "data/talkactions/lib", "data/talkactions/scripts/easymode"):
        shutil.copytree(source / path, root / path)
    for path in ("data/global.lua", "data/actions/scripts/misc/fluids.lua",
                 "data/actions/scripts/misc/weapons.lua", "data/actions/scripts/misc/food.lua",
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
    if scenario == "spill_enabled":
        shutil.copytree(source / "data/zedas", root / "data/zedas")
        (root / "data/actions/actions.xml").write_text(
            '<actions><action itemid="2874" script="misc/fluids.lua" />'
            '<action itemid="3410" script="misc/fluids.lua" /></actions>')
        (root / "data/movements/lib").mkdir(parents=True)
        (root / "data/movements/lib/movements.lua").write_text("")
        (root / "data/movements/movements.xml").write_text(
            '<movements><movevent event="AddItem" itemid="3351" function="onAddField" />'
            '<movevent event="AddItem" itemid="3352" tileitem="1" function="onAddField" />'
            '<movevent event="AddItem" pos="110;94;7" function="onAddField" /></movements>')
    if scenario in {"spill_hunting", "broker_witness", "broker", "broker_catalog", "broker_restart", "loot_classes", "broker_feedback", "broker_feedback_disabled"}:
        shutil.copytree(source / "data/zedas", root / "data/zedas")
        (root / "data/npc").mkdir()
        for buyer in ET.parse(source / "data/zedas/broker.xml").getroot():
            path = buyer.get("source")
            shutil.copyfile(source / path, root / path)
            for dependency in filter(None, buyer.get("dependencies", "").split(";")):
                path = dependency.split("|", 1)[0]
                shutil.copyfile(source / path, root / path)
        actions = ET.Element("actions")
        for node in ET.parse(source / "data/actions/actions.xml").getroot():
            if node.get("script") in {"misc/fluids.lua", "misc/weapons.lua", "misc/food.lua"}:
                actions.append(node)
        ET.ElementTree(actions).write(root / "data/actions/actions.xml")
        (root / "data/movements/lib").mkdir(parents=True)
        (root / "data/movements/scripts").mkdir()
        (root / "data/movements/lib/movements.lua").write_text("")
        movements = ET.Element("movements")
        for node in ET.parse(source / "data/movements/movements.xml").getroot():
            if node.get("event") in {"Equip", "DeEquip"}:
                movements.append(node)
        ET.ElementTree(movements).write(root / "data/movements/movements.xml")
    enabled = "false" if scenario in {"disabled", "wave2_disabled"} else '"invalid"' if scenario == "invalid_config" else "true"
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
    if scenario in {"spill_hunting", "broker_witness", "broker", "broker_catalog", "broker_restart", "loot_classes", "broker_feedback", "broker_feedback_disabled"}:
        config += 'easyModeGoldBag = true\n'
    if scenario in {"spill_enabled", "spill_hunting", "spill_only", "spill_invalid_policy", "spill_missing_policy", "wave2_disabled", "broker_witness", "broker", "broker_catalog", "broker_restart", "loot_classes", "broker_feedback", "broker_feedback_disabled"}:
        config += 'easyModeLootSpill = true\n'
    if scenario == "wave2_disabled":
        config += 'easyModeGoldBag = true\neasyModeRespawn = true\neasyModeLootBroker = true\n'
    if scenario == "respawn_enabled":
        config += 'easyModeRespawn = true\n'
    if scenario == "spill_only":
        config += 'easyModeAutoGold = false\n'
        shutil.copytree(source / "data/zedas", root / "data/zedas")
    if scenario == "spill_invalid_policy":
        (root / "data/zedas").mkdir(parents=True, exist_ok=True)
        (root / "data/zedas/lootspill.xml").write_text(
            '<lootspill><item id="3357" name="wrong identity" /></lootspill>')
    for scenario_name, key in (("commands_disabled", "easyModeCommands"),
                               ("target_disabled", "easyModeAutoTarget"),
                               ("gold_disabled", "easyModeAutoGold")):
        if scenario == scenario_name:
            config = config.replace(key + " = true", key + " = false")
    if scenario in {"broker_witness", "broker", "broker_catalog", "broker_restart", "loot_classes", "broker_feedback", "broker_feedback_disabled"}:
        config += 'easyModeLootBroker = true\neasyModeBrokerGraceSeconds = 5\n'
    if scenario in {"broker_witness", "broker", "broker_restart", "broker_feedback"}:
        config += ('mysqlHost="127.0.0.1"\nmysqlUser="root"\nmysqlPass="fixture-only"\n'
                   'mysqlDatabase="zedas_broker_test"\nmysqlPort=' + os.environ["ZEDAS_BROKER_TEST_PORT"] + '\n')
    if scenario == "broker_feedback_disabled":
        config += "easyModeLootBroker = false\n"
    (root / "config.lua").write_text(config)
    if scenario == "invalid_config":
        # Exercise each startup setting independently, including newly reserved flags.
        valid = config.replace('"invalid"', 'false')
        for key in ("easyModeEnabled", "easyModeEndlessRunes", "easyModeEndlessManaFluid",
                    "easyModeCommands", "easyModeAutoTarget", "easyModeAutoGold",
                    "easyModeLootSpill", "easyModeGoldBag", "easyModeRespawn", "easyModeLootBroker"):
            (root / "config.lua").write_text(valid + f'\n{key} = "invalid"\n')
            subprocess.run([binary, scenario], cwd=root, check=True)
        for key, values in {
            "easyModeBrokerGraceSeconds": ["4", "241", "5.5", "'45'", "0/0"],
            "easyModeBrokerCommissionBps": ["-1", "10001", "2.5", "'2500'"],
            "easyModeBrokerLeashTiles": ["1", "13", "2.5", "'7'"],
            "easyModeBrokerCatalogFile": ["123", "''"],
        }.items():
            for value in values:
                (root / "config.lua").write_text(valid + f'\n{key} = {value}\n')
                subprocess.run([binary, scenario], cwd=root, check=True)
        (root / "config.lua").write_text(valid + '\neasyModeRespawnPolicyFile = 123\n')
    if scenario == "broker_restart":
        for stage in ("broker_seed", "broker_sale_restart", "broker_payout_restart", "broker_final_restart"):
            subprocess.run([binary, stage], cwd=root, check=True)
    else:
        subprocess.run([binary, scenario], cwd=root, check=True)

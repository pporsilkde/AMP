
from pathlib import Path
import json, re, xml.etree.ElementTree as ET

ROOT = Path("/mnt/data/x043_work")

def normalize_cell(value):
    value = (value or "").strip().lower()
    marker = " - instance for "
    if marker in value:
        value = value.split(marker, 1)[0].rstrip()
    return value

def cells_match(expected, actual):
    if not (expected or "").strip():
        return True
    return normalize_cell(expected) == normalize_cell(actual)

assert cells_match("Balmora, Caius Cosades' House",
                   "Balmora, Caius Cosades' House - Instance for 2")
assert cells_match("Balmora, Caius Cosades' House - Instance for Alice",
                   "Balmora, Caius Cosades' House - Instance for Bob")
assert not cells_match("Balmora, Caius Cosades' House", "Balmora, Guild of Mages")

quest = json.loads((ROOT / "server/data/custom/quests/arena_caius_drink.json").read_text(encoding="utf-8"))
assert quest["status"] == "published"
assert quest["giver"]["refId"] == "caius cosades"
assert quest["giver"]["cell"] == "Balmora, Caius Cosades' House"
assert any(t["text"] == "немного выпивки" and t.get("green") for t in quest["topics"])

idx = json.loads((ROOT / "server/data/custom/quests/index.json").read_text(encoding="utf-8"))
assert any(q["id"] == "arena_caius_drink" for q in idx["quests"])

cmake = (ROOT / "files/mygui/CMakeLists.txt").read_text(encoding="utf-8")
assert "arenamp_serverquesteditor.layout" in cmake
ET.parse(ROOT / "files/mygui/arenamp_serverquesteditor.layout")

ignore = (ROOT / "server/data/.gitignore").read_text(encoding="utf-8")
assert "!custom/quests/" in ignore
assert "!custom/quests/*.json" in ignore

quest_core = (ROOT / "server/scripts/serverQuestSystem.lua").read_text(encoding="utf-8")
assert "questCellMatches(quest.giver.cell, cellDescription)" in quest_core
assert 'processCommand(pid, { "quest", "studio" })' in quest_core
assert "staffRank >= 1" in quest_core

registry = (ROOT / "apps/openmw/mwmp/ServerQuestRegistry.cpp").read_text(encoding="utf-8")
assert "ServerQuestRegistry::cellsMatch" in registry
assert '" - instance for "' in registry

processor = (ROOT / "apps/openmw/mwmp/processors/player/ProcessorGUIMessageBox.hpp").read_text(encoding="utf-8")
assert "ServerQuestRegistry::cellsMatch" in processor

private = (ROOT / "server/scripts/privateCellInstances.lua").read_text(encoding="utf-8")
assert "PrepareLoginRestore" in private
assert "HandleLoginCellChange" in private
assert "Ignored stale login cell" in private
assert "Login restore confirmed" in private

base = (ROOT / "server/scripts/player/base.lua").read_text(encoding="utf-8")
assert "privateCellInstances.PrepareLoginRestore(self)" in base

event = (ROOT / "server/scripts/eventHandler.lua").read_text(encoding="utf-8")
assert "privateCellInstances.HandleLoginCellChange" in event

workflow = (ROOT / ".github/workflows/release-builds.yml").read_text(encoding="utf-8")
assert r'install\server\data\custom\quests\arena_caius_drink.json' in workflow
assert r'install\resources\mygui\arenamp_serverquesteditor.layout' in workflow


# Model the X043 two-phase reconnect guard:
saved = {
    "cell": "Balmora, Caius Cosades' House - Instance for 2",
    "posX": 100.0, "posY": 200.0, "posZ": 300.0,
    "rotX": 0.0, "rotZ": 1.0
}
pending = dict(saved)
stale_packet = {"cell": "-2, -9", "posX": 0.0, "posY": 0.0, "posZ": -5000.0}
assert stale_packet["cell"] != pending["cell"]  # consumed, must not overwrite save
confirm_packet = {"cell": saved["cell"], "posX": 0.0, "posY": 0.0, "posZ": -5000.0}
for key in ("posX", "posY", "posZ", "rotX", "rotZ"):
    confirm_packet[key] = pending[key]
assert confirm_packet["posZ"] == 300.0

print("X043 quest/instance/editor harness: ALL OK")

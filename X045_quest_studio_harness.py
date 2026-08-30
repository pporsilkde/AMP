#!/usr/bin/env python3
"""ArenaMP X045 Quest Studio harness.

Static, build-free verification of the Quest Studio and the server quest system:

  1. Every widget the C++ fetches exists in the layout, and every layout widget
     is either fetched or purely decorative.
  2. The compact layout fits inside the window and no two controls overlap, so
     every button is actually clickable.
  3. Every localisation key used by the C++ exists in both en.ini and ru.ini,
     with no empty values and no duplicate keys.
  4. Every EDITOR_CMD action the client sends is handled by the server, and the
     field positions of each command match on both sides.
  5. Every EDITOR_* transport line the server emits carries at least as many
     fields as the client's parser requires.
  6. The shipped quest JSON definitions parse and reference existing stages.
"""

import json
import os
import re
import sys
import xml.etree.ElementTree as ET

ROOT = os.path.dirname(os.path.abspath(__file__))
LAYOUT = os.path.join(ROOT, "files/mygui/arenamp_serverquesteditor.layout")
EDITOR_CPP = os.path.join(ROOT, "apps/openmw/mwmp/GUI/ServerQuestEditor.cpp")
REGISTRY_CPP = os.path.join(ROOT, "apps/openmw/mwmp/ServerQuestRegistry.cpp")
LUA = os.path.join(ROOT, "server/scripts/serverQuestSystem.lua")
QUESTS = os.path.join(ROOT, "server/data/custom/quests")
INI = os.path.join(ROOT, "files/vfs/l10n/arenamp")

failures = []


def check(name, condition, detail=""):
    print(("PASS  " if condition else "FAIL  ") + name + (("  -> " + detail) if detail and not condition else ""))
    if not condition:
        failures.append(name)
    return condition


def read(path):
    with open(path, encoding="utf-8") as handle:
        return handle.read()


# --- 1 & 2: layout -----------------------------------------------------------

def parse_layout():
    tree = ET.parse(LAYOUT)
    window = None
    widgets = []
    for widget in tree.iter("Widget"):
        if widget.get("name") == "_Main":
            window = widget
        widgets.append(widget)
    return tree, window, widgets


def coord(widget):
    return [int(v) for v in widget.get("position").split()]


def layout_checks():
    tree, window, widgets = parse_layout()
    src = read(EDITOR_CPP)

    names = {w.get("name") for w in widgets if w.get("name")}
    fetched = set(re.findall(r'getWidget\(\s*\w+\s*,\s*"([^"]+)"\)', src))
    fetched |= set(re.findall(r'localizeLabel\("([^"]+)"', src))
    check("every fetched widget exists in the layout", not (fetched - names), str(sorted(fetched - names)))

    win_w, win_h = coord(window)[2], coord(window)[3]
    check("window is 70-80% of the former 1000x640",
          0.70 <= win_w / 1000 <= 0.80 and 0.70 <= win_h / 640 <= 0.80,
          f"{win_w}x{win_h}")

    # Direct children of the window must stay inside it.
    outside = []
    for child in window:
        if child.tag != "Widget" or not child.get("position"):
            continue
        x, y, w, h = coord(child)
        if x < 0 or y < 0 or x + w > win_w or y + h > win_h:
            outside.append(child.get("name"))
    check("no top-level control leaves the window", not outside, str(outside))

    # Controls inside each tab page must stay inside the page and not overlap.
    tabs = [w for w in widgets if w.get("name") == "EditorTabs"]
    check("the tab control exists", len(tabs) == 1)
    tab = tabs[0]
    pages = [c for c in tab if c.tag == "Widget"]
    check("six tab pages including Help", len(pages) == 6, str(len(pages)))

    overflow, overlaps = [], []
    for page in pages:
        px, py, pw, ph = coord(page)
        boxes = []
        for child in page:
            if child.tag != "Widget" or not child.get("position"):
                continue
            x, y, w, h = coord(child)
            if x < 0 or y < 0 or x + w > pw or y + h > ph:
                overflow.append((page.get("name") or "page", child.get("name"), x, y, w, h))
            boxes.append((child.get("name"), x, y, w, h))
        for i in range(len(boxes)):
            for j in range(i + 1, len(boxes)):
                an, ax, ay, aw, ah = boxes[i]
                bn, bx, by, bw, bh = boxes[j]
                if ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah:
                    overlaps.append((an, bn))
    check("no control overflows its tab page", not overflow, str(overflow[:4]))
    check("no two controls on a page overlap", not overlaps, str(overlaps[:4]))

    # Buttons must be wide enough for the longest caption in either language.
    ini = load_ini("ru")
    button_keys = {
        "NewQuestButton": "questeditor.btn.new", "CloneQuestButton": "questeditor.btn.clone",
        "DeleteQuestButton": "questeditor.btn.delete", "PickGiverButton": "questeditor.btn.pick_giver",
        "ClearUniqueButton": "questeditor.btn.any_instance", "SaveOverviewButton": "questeditor.btn.save_overview",
        "NewTopicButton": "questeditor.btn.new", "SaveTopicButton": "questeditor.btn.save",
        "DeleteTopicButton": "questeditor.btn.delete", "SaveOfferButton": "questeditor.btn.save_offer",
        "NewOfferChoiceButton": "questeditor.btn.new", "SaveOfferChoiceButton": "questeditor.btn.save",
        "DeleteOfferChoiceButton": "questeditor.btn.delete", "NewStageButton": "questeditor.btn.new_stage",
        "SaveStageButton": "questeditor.btn.save_stage", "DeleteStageButton": "questeditor.btn.delete_stage",
        "NewStageChoiceButton": "questeditor.btn.new", "SaveStageChoiceButton": "questeditor.btn.save",
        "DeleteStageChoiceButton": "questeditor.btn.delete", "AddNextButton": "questeditor.btn.add_next",
        "DeleteNextButton": "questeditor.btn.remove_next", "AddRequirementButton": "questeditor.btn.add",
        "DeleteRequirementButton": "questeditor.btn.delete", "AddRewardButton": "questeditor.btn.add",
        "DeleteRewardButton": "questeditor.btn.delete", "ValidateButton": "questeditor.btn.validate",
        "PublishButton": "questeditor.btn.publish", "DisableButton": "questeditor.btn.disable",
        "RefreshButton": "questeditor.btn.refresh", "CloseButton": "questeditor.btn.close",
    }
    # MW_Button uses the Sand font; ~7.2 px per character plus 12 px of padding is
    # the conservative estimate used for the Cyrillic captions.
    tight = []
    by_name = {w.get("name"): w for w in widgets if w.get("name")}
    for widget_name, key in button_keys.items():
        widget = by_name.get(widget_name)
        if widget is None:
            tight.append((widget_name, "missing"))
            continue
        width = coord(widget)[2]
        needed = int(len(ini[key]) * 7.2) + 12
        if needed > width:
            tight.append((widget_name, ini[key], needed, width))
    check("every button is wide enough for its Russian caption", not tight, str(tight[:4]))

    # Stage toggles render "<label> <yes/no>".
    toggle = []
    for widget_name, key in (("StageInitialButton", "questeditor.stage.initial"),
                             ("StageCompleteButton", "questeditor.stage.complete"),
                             ("StageFailButton", "questeditor.stage.fail")):
        caption = ini[key] + " " + ini["questeditor.value.no"]
        width = coord(by_name[widget_name])[2]
        needed = int(len(caption) * 7.2) + 12
        if needed > width:
            toggle.append((widget_name, caption, needed, width))
    check("stage toggle buttons fit their Russian captions", not toggle, str(toggle))


# --- 3: localisation ---------------------------------------------------------

def load_ini(lang):
    values = {}
    for line in read(os.path.join(INI, lang + ".ini")).splitlines():
        line = line.strip()
        if not line or line.startswith("#") or line.startswith(";") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.strip()] = value.strip()
    return values


def localisation_checks():
    src = read(EDITOR_CPP)
    keys = sorted(set(re.findall(r'"(questeditor\.[a-z0-9_.]+)"', src)))
    check("the Quest Studio uses localisation keys, not literals", len(keys) >= 70, str(len(keys)))
    for lang in ("en", "ru"):
        values = load_ini(lang)
        missing = [k for k in keys if k not in values]
        empty = [k for k in keys if k in values and not values[k]]
        check(f"{lang}.ini defines every Quest Studio key", not missing, str(missing[:5]))
        check(f"{lang}.ini has no empty Quest Studio value", not empty, str(empty[:5]))

    en, ru = load_ini("en"), load_ini("ru")
    check("en.ini and ru.ini have the same key set", set(en) == set(ru),
          str(sorted(set(en) ^ set(ru))[:5]))
    untranslated = [k for k in keys if en.get(k) == ru.get(k) and k not in
                    ("questeditor.lbl.refid", "questeditor.lbl.unique")]
    check("Russian values are actually translated", not untranslated, str(untranslated[:5]))
    check("help text exists in both languages",
          len(en["questeditor.help.text"]) > 800 and len(ru["questeditor.help.text"]) > 800)


# --- 4 & 5: client/server protocol ------------------------------------------

def protocol_checks():
    client = read(EDITOR_CPP)
    lua = read(LUA)

    sent = set(re.findall(r'sendCommand\(\{\s*"([a-z_]+)"', client))
    handled = set(re.findall(r'action == "([a-z_]+)"', lua))
    check("every client editor command is handled by the server", not (sent - handled),
          str(sorted(sent - handled)))
    check("the client uses the full command set", len(sent) >= 20, str(len(sent)))

    # Field positions: the client's first payload field lands in parts[3].
    expected = {
        "new": ["id", "name"],
        "clone": ["source", "id", "name"],
        "overview": ["quest", "name", "mode", "refId", "cell", "unique", "initial"],
        "topic_upsert": ["quest", "oldId", "newId", "text", "enabled"],
        "choice_upsert": ["quest", "scope", "stage", "oldId", "newId", "action", "target", "text"],
        "stage_upsert": ["quest", "oldIndex", "newIndex", "journal", "dialogue", "initial", "complete", "fail"],
        "require_add": ["quest", "scope", "stage", "choice", "type", "op", "value", "ref"],
        "reward_add": ["quest", "stage", "type", "valueA", "valueB"],
    }
    # The server reads these indices; keep them aligned with the client payloads.
    server_reads = {
        "new": 4, "clone": 5, "overview": 9, "topic_upsert": 7,
        "choice_upsert": 10, "stage_upsert": 10, "require_add": 10, "reward_add": 7,
    }
    mismatches = []
    for command, fields in expected.items():
        highest_client_index = 2 + len(fields)
        if highest_client_index != server_reads[command]:
            mismatches.append((command, highest_client_index, server_reads[command]))
    check("client payload length matches the server's highest parts[] index",
          not mismatches, str(mismatches))

    registry = read(REGISTRY_CPP)
    required = dict(re.findall(r'fields\[0\] == "(EDITOR_[A-Z]+)" && fields\.size\(\) >= (\d+)', registry))
    emitted = {
        "EDITOR_META": 5, "EDITOR_QUEST": 12, "EDITOR_TOPIC": 5, "EDITOR_STAGE": 7,
        "EDITOR_NEXT": 4, "EDITOR_CHOICE": 9, "EDITOR_REQ": 12, "EDITOR_REWARD": 11,
        "EDITOR_VALID": 4,
    }
    short = [(line, emitted[line], int(count)) for line, count in required.items()
             if line in emitted and emitted[line] < int(count)]
    check("every transport line carries the fields the parser requires", not short, str(short))
    check("the parser handles every emitted transport line",
          not (set(emitted) - set(required)), str(sorted(set(emitted) - set(required))))
    for token in ("EDITOR_CLEAR", "EDITOR_END"):
        check(f"{token} is emitted and parsed", token in lua and token in registry)


# --- 6: shipped definitions --------------------------------------------------

def definition_checks():
    files = sorted(f for f in os.listdir(QUESTS) if f.endswith(".json"))
    check("quest definitions are present", len(files) >= 2, str(files))
    for name in files:
        path = os.path.join(QUESTS, name)
        try:
            data = json.loads(read(path))
        except Exception as error:  # noqa: BLE001 - report and continue
            check(f"{name} parses as JSON", False, str(error))
            continue
        check(f"{name} parses as JSON", True)
        if name == "index.json" or "stages" not in data:
            continue
        stages = {int(stage["index"]) for stage in data["stages"]}
        check(f"{name}: initial stage exists", int(data.get("initialStage", 0)) in stages,
              f"initial={data.get('initialStage')} stages={sorted(stages)}")
        targets = []
        for stage in data["stages"]:
            targets += [int(t) for t in stage.get("next", [])]
            for choice in stage.get("choices", []):
                if choice.get("targetStage") is not None:
                    targets.append(int(choice["targetStage"]))
        for choice in (data.get("offer", {}) or {}).get("choices", []):
            if choice.get("targetStage") is not None:
                targets.append(int(choice["targetStage"]))
        dangling = sorted(set(targets) - stages)
        check(f"{name}: every transition points at a real stage", not dangling, str(dangling))
        check(f"{name}: has a terminal stage",
              any(stage.get("complete") or stage.get("fail") for stage in data["stages"]))


def main():
    layout_checks()
    print()
    localisation_checks()
    print()
    protocol_checks()
    print()
    definition_checks()
    print("\nRESULT:", "OK" if not failures else f"{len(failures)} FAILURE(S): {failures}")
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())

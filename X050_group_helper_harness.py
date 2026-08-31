#!/usr/bin/env python3
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
errors = []

def need(path, needle, label=None):
    text = (ROOT / path).read_text(encoding='utf-8')
    if needle not in text:
        errors.append(f"{path}: missing {label or needle}")

# Server core / group API
for symbol in [
    'function groupHelper.GetPlayerGroupId',
    'function groupHelper.ArePlayersInSameGroup',
    'function groupHelper.GetGroupMembers',
    'function groupHelper.IsLeader',
    'function groupHelper.SyncJournalChanges',
    'function groupHelper.SyncTopics',
    'function groupHelper.AwardServerQuestXp',
    'function groupHelper.PreprocessPlayerLevel',
    'function groupHelper.ProcessPendingXp',
    'function groupHelper.IsFriendlySummon',
    'customCommandHooks.registerCommand("groupui"',
    'customEventHooks.registerValidator("OnObjectHit"',
    'customEventHooks.registerHandler("OnActorDeath"',
    'customEventHooks.registerHandler("OnPlayerJournal"',
    'customEventHooks.registerHandler("OnPlayerTopic"',
]:
    need('server/scripts/groupHelper.lua', symbol)
need('server/scripts/serverCore.lua', 'groupHelper = require("groupHelper")')
need('server/scripts/eventHandler.lua', 'groupHelper.PreprocessPlayerLevel')
need('server/scripts/eventHandler.lua', 'groupHelper.ProcessPendingXp')
need('server/scripts/serverQuestSystem.lua', 'groupHelper.AwardServerQuestXp')
need('server/scripts/config.lua', '["server party xp"] = true')

# Client XP bridge / menu controls
need('apps/openmw/mwmechanics/xpleveling.cpp', 'queueServerXpSignal(player, "kill", xp)')
need('apps/openmw/mwmechanics/xpleveling.cpp', 'queueServerXpSignal(player, "quest", xp, questId, journalIndex)')
need('apps/openmw/mwmechanics/xpleveling.cpp', 'void awardServer(float amount, bool scaled, const std::string& reason)')
need('apps/openmw/mwmp/GUI/GUIChat.cpp', '@@AMP_GROUP@@')
need('apps/openmw/mwmp/GUI/GUIChat.cpp', '@@AMP_XP@@')
need('apps/openmw/mwmp/GUI/GUIChat.cpp', 'send("/groupui state")')
need('apps/openmw/mwmp/GUIController.cpp', 'handleServerControlMessage(msg)')

layout = ROOT / 'files/mygui/tes3mp_chat.layout'
try:
    tree = ET.parse(layout)
    names = [e.attrib.get('name') for e in tree.iter('Widget') if e.attrib.get('name')]
    if len(names) != len(set(names)):
        errors.append('tes3mp_chat.layout: duplicate widget names')
    for widget in [
        'GroupPane','GroupInfo','GroupNameEdit','GroupTargetEdit','GroupCreateButton',
        'GroupInviteButton','GroupKickButton','GroupLeaderButton','GroupJournalButton',
        'GroupTopicsButton','GroupLeaveButton','GroupDisbandButton','GroupAcceptButton',
        'GroupDeclineButton','GroupRefreshButton'
    ]:
        if widget not in names:
            errors.append(f'tes3mp_chat.layout: missing {widget}')
except Exception as exc:
    errors.append(f'tes3mp_chat.layout XML error: {exc}')

cpp = (ROOT / 'apps/openmw/mwmp/GUI/GUIChat.cpp').read_text(encoding='utf-8')
keys = sorted(set(re.findall(r'localizeArena\("([^"]+)"\)', cpp)))
for lang in ['en', 'ru']:
    text = (ROOT / f'files/vfs/l10n/arenamp/{lang}.ini').read_text(encoding='utf-8')
    for key in keys:
        if re.search(r'^' + re.escape(key) + r'\s*=', text, re.M) is None:
            errors.append(f'{lang}.ini: missing {key}')

# Safety assertions for the design we want to preserve.
group = (ROOT / 'server/scripts/groupHelper.lua').read_text(encoding='utf-8')
if 'prefs.journalSync = false' not in group or 'prefs.topicSync = false' not in group:
    errors.append('group sync must default OFF')
if 'amount / #recipients' not in group:
    errors.append('same-cell XP split missing')
if 'tes3mp.GetCell(memberPid) == cell' not in group and 'tes3mp.GetCell(pid) == cellDescription' not in group:
    errors.append('same-cell membership check missing')
if 'StopCombat' not in group:
    errors.append('summon StopCombat protection missing')

if errors:
    print('X050 HARNESS FAILED')
    for error in errors:
        print(' -', error)
    sys.exit(1)
print('X050 HARNESS OK')
print(f'Checked {len(keys)} localized GUI keys and group/XP/summon integration markers.')

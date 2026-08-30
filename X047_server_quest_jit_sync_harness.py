from pathlib import Path
import json, re
ROOT=Path('/mnt/data/x047_impl')
s=(ROOT/'server/scripts/serverQuestSystem.lua').read_text(encoding='utf-8')
# Existing persistence fix must remain.
assert 'questIndexRefreshOnServerStart = false' in (ROOT/'server/scripts/config.lua').read_text(encoding='utf-8')
assert 'X046' in (ROOT/'server/scripts/questIndexStore.lua').read_text(encoding='utf-8') or 'persistent' in (ROOT/'server/scripts/questIndexStore.lua').read_text(encoding='utf-8').lower()
# JIT sync handler + registration.
assert 'local function onObjectActivateQuestResync' in s
assert 'customEventHooks.registerHandler("OnObjectActivate", onObjectActivateQuestResync)' in s
assert 'serverQuestSystem.SyncPlayer(pid)' in s[s.index('local function onObjectActivateQuestResync'):s.index('local function onPlayerAuthentified')]
assert 'questCellMatches(quest.giver.cell, cellDescription)' in s[s.index('local function onObjectActivateQuestResync'):s.index('local function onPlayerAuthentified')]
# Manual recovery/diagnostic commands.
assert 'sub == "sync" or sub == "resync"' in s
assert 'sub == "caius" or sub == "caiusdebug"' in s
assert 'processCommand(pid, { "quest", "studio" })' in s
# Built-in quest remains published and points at base Caius cell/topic.
q=json.loads((ROOT/'server/data/custom/quests/arena_caius_drink.json').read_text(encoding='utf-8'))
assert q['status']=='published'
assert q['giver']['refId']=='caius cosades'
assert q['giver']['cell']=="Balmora, Caius Cosades' House"
assert any(t.get('enabled',True) and t.get('green') and t['text']=='немного выпивки' for t in q['topics'])
# Instance normalization still exists on both server and client.
assert 'privateCellInstances.GetBaseCellDescription' in s
reg=(ROOT/'apps/openmw/mwmp/ServerQuestRegistry.cpp').read_text(encoding='utf-8').lower()
assert ' - instance for ' in reg
# X043a opaque token exemption must remain.
obj=(ROOT/'apps/openmw/mwmp/ObjectList.cpp').read_text(encoding='utf-8')
assert 'isServerQuestToken' in obj and '@ArenaQuestChoice:' in obj
print('X047 server quest JIT sync harness: ALL OK')

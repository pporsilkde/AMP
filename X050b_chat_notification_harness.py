from pathlib import Path
root = Path(__file__).resolve().parent
quest = (root/'server/scripts/serverQuestSystem.lua').read_text(encoding='utf-8')
group = (root/'server/scripts/groupHelper.lua').read_text(encoding='utf-8')
config = (root/'server/scripts/config.lua').read_text(encoding='utf-8')
checks = {
    'quest master flag': 'chatNotificationsEnabled = false' in config,
    'quest progress flag wired': 'sendRuntime(pid, tostring(quest.name) .. " — " .. text, "progress")' in quest,
    'journal popup independently gated': 'questConfig.journalUpdatePopup ~= false' in quest,
    'old unconditional journal chat echo removed': 'send(pid, tostring(quest.name) .. " — " .. text)' not in quest,
    'reward runtime gate': 'sendRuntime(pid, tostring(reward.text or reward.value or ""), "reward")' in quest,
    'runtime error gate': 'sendRuntime(pid, "Не выполнено условие — "' in quest,
    'group chat flag': '["chat notifications"] = true' in config,
    'group xp spam default off': '["xp share chat messages"] = false' in config,
    'group notice reads live config': 'groupChatNotificationsEnabled()' in group,
    'group xp notice reads live config': 'groupXpChatMessagesEnabled()' in group,
}
failed=[name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(('OK   ' if ok else 'FAIL ') + name)
if failed:
    raise SystemExit('Failed: ' + ', '.join(failed))
print('X050b chat notification harness: OK')

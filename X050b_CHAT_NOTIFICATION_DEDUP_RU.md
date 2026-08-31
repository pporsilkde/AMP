# ArenaMP X050b — chat notification deduplication

База: X050a.

## Что изменено

### Server Quest runtime

`serverQuestSystem.lua` больше не дублирует обычное обновление серверного квеста одновременно в MessageBox и в чат.
По умолчанию при обновлении этапа остаётся только компактный `MessageBox` «Дневник обновлён: ...».

Автоматические `[Quest]` сообщения вынесены в отдельный runtime-канал и теперь управляются флагами `config.serverQuests`:

```lua
chatNotificationsEnabled = false, -- мастер-флаг для автоматических сообщений
chatProgressMessages = false,     -- старт/этап/текст journal
chatRewardMessages = false,       -- reward type=message
chatRuntimeErrors = false,        -- ошибки условий в игровом диалоге
journalUpdatePopup = true         -- отдельный popup об обновлении журнала
```

Чтобы вернуть сообщения о прогрессе в чат:

```lua
config.serverQuests.chatNotificationsEnabled = true
config.serverQuests.chatProgressMessages = true
```

Если нужен только чат без popup:

```lua
config.serverQuests.journalUpdatePopup = false
```

Команды `/quest` и сообщения Quest Studio не подавляются этими флагами: администратор/редактор по-прежнему получает обратную связь.

### Group runtime

В `config.groupSystem` добавлены:

```lua
["chat notifications"] = true,
["xp share chat messages"] = false
```

- `chat notifications=false` отключает обычные `[Group]` уведомления в чате, не отключая группу и Player Menu.
- `xp share chat messages=false` по умолчанию убирает частый текст `Shared ... XP`; само деление XP продолжает работать.

Проверки читают текущий `config` при отправке, поэтому эти переключатели совместимы с hot-reload `config.lua`.

## Изменённые файлы

- `server/scripts/serverQuestSystem.lua`
- `server/scripts/groupHelper.lua`
- `server/scripts/config.lua`

# ArenaMP X047 — JIT Server Quest Sync + анализ предыдущего crash dump

База: ArenaMP X046 (поверх X045).

## 1. Crash dump из предыдущей проблемы

`tes3mp-client.log-crash(4).dmp` — Windows MiniDump, 18 MiB.

Из ExceptionStream:

- exception code: `0xC0000005` (access violation / read);
- crashing thread: 5764;
- exception address: `tes3mp.exe + 0x68E454`;
- ExceptionInformation: read from address `0xF`.

Инструкция по адресу падения находится в коротком цикле чтения C-string. В регистре,
используемом как указатель на строку, было `0xF`, то есть код получил повреждённый
указатель вместо валидной строки.

Это согласуется с клиентским логом предыдущего запуска: непосредственно перед обрывом
старый `PlayerQuestIndex MODE_UPLOAD/oracle` повторно читал raw CELL contexts и выводил
`Failed to open ''` / некорректные refs. X046 убрал принудительную регенерацию индекса
на каждом старте и заменил raw CELL scan на безопасный scan уже загруженного ESMStore.

X047 не возвращает старый oracle scan и сохраняет X046 persistence logic.

## 2. Почему тестовый топик Кая может не появляться после login

ServerQuest definitions в cumulative присутствуют:

- `arena_caius_drink` — `published`;
- giver: `caius cosades`;
- base cell: `Balmora, Caius Cosades' House`;
- green topic: `немного выпивки`.

До X047 клиентский ServerQuestRegistry синхронизировался главным образом на
`OnPlayerFinishLogin`. В private instance login происходит несколько CELL/dynamic-record
переходов. Если hidden GUI transport `CLEAR/QUEST/END` пришёл в неудобный момент,
registry мог остаться пустым до следующего явного `SyncPlayer`.

## 3. X047: JIT sync на активации квестодателя

Добавлен `onObjectActivateQuestResync`.

Когда игрок активирует NPC, сервер проверяет:

1. NPC является giver хотя бы одного `published` server quest;
2. definition проходит validation;
3. giver cell совпадает с текущей cell через instance-aware `questCellMatches`.

Если совпало — сервер немедленно повторяет `SyncPlayer(pid)`.

Это работает в обоих порядках:

- transport приходит до открытия DialogueWindow -> registry уже заполнен;
- transport приходит после открытия -> `END` вызывает `refreshServerQuestTopics()` и
  зелёная тема добавляется в уже открытый диалог.

Нового сетевого packet id нет; используется существующий X036 hidden quest transport.
Клиентский protocol не меняется.

## 4. Диагностические команды

Для любого игрока:

`/quests resync`

Принудительно повторяет server quest sync.

`/quests caius`

Показывает состояние встроенного тестового квеста Кая и одновременно делает resync.
Пример состояния:

- `not_started` — зелёный topic должен быть доступен;
- `active stage=10` — topic должен оставаться доступен;
- `completed` / `failed` — topic скрыт штатно.

Если built-in квест уже завершён, Moderator/Admin может повторить тест:

`/quest reset arena_caius_drink PID`

После reset сервер немедленно синхронизирует темы снова.

## 5. `/quests studio`

X047 также исправляет alias:

- `/quest`
- `/quest studio`
- `/quests studio`
- `/quests editor`
- `/queststudio`

все ведут в настоящий MyGUI Quest Studio при `staffRank >= 1`.

## 6. Что проверить

1. Перезапустить сервер с существующим `questIndex.json`.
2. Убедиться, что клиент НЕ получает `PlayerQuestIndex mode 2`.
3. Войти в дом Кая / private instance.
4. Если тема сразу не видна, выполнить `/quests caius` и посмотреть состояние.
5. Активировать Кая — в server log должна появиться строка:
   `[ServerQuest] X047 JIT topic sync for pid ...`
6. В диалоге должна появиться зелёная тема `немного выпивки`.
7. Если `/quests caius` показывает `completed`/`failed`, выполнить staff reset.

## 7. Изменённые runtime файлы относительно X046

Только:

`server/scripts/serverQuestSystem.lua`

То есть X047 не требует пересборки `tes3mp.exe` для этого hotfix.

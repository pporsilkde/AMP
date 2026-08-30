# ArenaMP X043 — Quest Studio / Instance-cell / Quest packaging fix

Основа: X042a.

## 1. Почему не открывался MyGUI Quest Studio

`files/mygui/arenamp_serverquesteditor.layout` существовал в исходниках X039+, но не был включён в `files/mygui/CMakeLists.txt`.
Поэтому CMake не копировал его в `resources/mygui`, и `ServerQuestEditorWindow : WindowModal`
не мог загрузить `arenamp_serverquesteditor.layout`.

X043:
- добавляет layout в `MYGUI_FILES`;
- Windows CI требует `install/resources/mygui/arenamp_serverquesteditor.layout`;
- `/quest`, `/quest studio` и `/quest editor` открывают Studio для staffRank >= 1;
- при staffRank 0 выводится явное сообщение с текущим рангом;
- `/quests` остаётся пользовательским списком серверных квестов.

Права:
- Player: staffRank 0 — только чтение/прохождение;
- Moderator: staffRank 1 — Quest Studio;
- Admin: staffRank 2;
- Server owner: staffRank 3.

Владелец сервера может выдать Moderator:
`/addmoderator PID`

## 2. Почему папка quests терялась из GitHub build

`server/data/.gitignore` разрешал только:
`custom/.gitkeep`

Но всё содержимое `server/data/custom/` ниже этого снова попадало под общий `*`.
В результате `custom/quests/*.json` могли оставаться локально, но не попадать в git checkout,
из которого GitHub Actions собирает release artifact.

X043 добавляет:
- `!custom/quests/`
- `!custom/quests/*.json`

И CI теперь требует:
- `server/data/custom/quests/index.json`
- `server/data/custom/quests/_TEMPLATE.json`
- `server/data/custom/quests/arena_caius_drink.json`
- `arena_example_missing_ring.json` как tracked seed

Windows release job дополнительно проверяет наличие `arena_caius_drink.json` уже в `install/server/...`.

## 3. Почему зелёный топик Кая не отображался

Квест был привязан к:
`Balmora, Caius Cosades' House`

Но игрок находился в:
`Balmora, Caius Cosades' House - Instance for <account>`

X036–X042 сравнивали строки cell буквально.

X043 нормализует private instance к base cell:
`... - Instance for <owner>` -> `Balmora, Caius Cosades' House`

Исправлены все три уровня:
1. серверный `GetAvailableTopics`;
2. серверная проверка нажатого quest token;
3. клиентский `ServerQuestRegistry` + проверка RESPONSE в `ProcessorGUIMessageBox`.

Прогресс квеста при этом остаётся персональным.

## 4. Проваливание в пустоту после reconnect внутри instance-cell

На reconnect dynamic CELL мог быть отправлен слишком рано на `OnPlayerConnect`.
Позже сервер делал `LoadCell()` в сохранённый instance, но клиент мог ещё прислать один старый
PlayerCellChange из стартовой exterior-cell.

X043 вводит двухфазное восстановление:

1. Перед `LoadCell()` dynamic CELL records отправляются повторно.
2. Сервер сохраняет authoritative cell/XYZ/rotation в `privateCellLoginRestore`.
3. Любой stale initial cell packet в течение 15 секунд игнорируется.
4. Сервер повторно отправляет dynamic CELL + сохранённую cell/position.
5. Когда клиент подтверждает ожидаемый instance, его incoming XYZ заменяются сохранёнными
   серверными XYZ до `SaveCell()`.
6. Position отправляется ещё раз уже после подтверждения dynamic cell.
7. Restore guard снимается.

Новые строки в server log:
- `[PRIVATE CELL] Prepared login restore ...`
- `[PRIVATE CELL] Ignored stale login cell ...`
- `[PRIVATE CELL] Login restore confirmed ...`

## Проверка

### Quest Studio
1. Убедиться, что аккаунт Moderator/Admin.
2. В чате: `/quest`
3. Должно открыться отдельное MyGUI Quest Studio.
4. Если staffRank=0, будет явное сообщение. Владелец: `/addmoderator PID`.

### Кай
1. Зайти в личный дом Кая.
2. Поговорить с `caius cosades`.
3. Должен появиться зелёный topic `немного выпивки`.
4. Принятие -> stage 10.
5. С `potion_mazte_01` появляется сдача бутылки.
6. Награда: 75 gold + 40 XP.

### Reconnect в instance
1. Остаться внутри `Balmora, Caius Cosades' House - Instance for ...`.
2. Выйти с сервера.
3. Подключиться снова.
4. Игрок должен появиться в сохранённой точке, без падения.
5. В логе должны появиться строки X043 login restore.

## Важно

X043 следует накладывать поверх X042a.
Для GitHub-сборки квестовые JSON после X043 больше не игнорируются `.gitignore`, поэтому их можно
обычно добавить в commit без `git add -f`.

# ArenaMP X046 — Persistent Quest Item Index / Login Crash Fix

База: пользовательский `ArenaMP_X045_QUEST_STUDIO_COMPACT_L10N_CUMULATIVE`.

## Что исправлено

### 1. `questIndex.json` больше не сбрасывается при каждом рестарте сервера

Старое поведение X031:

1. сервер находил валидный `server/data/custom/questIndex.json`;
2. всё равно помечал его `refreshPending`;
3. отключал authoritative index;
4. первому игроку после рестарта отправлял `MODE_UPLOAD`;
5. клиент заново сканировал ESM/ESP.

X046:

- если `questIndex.json` существует и его hash валиден — он сразу загружается и используется;
- клиент получает `MODE_OFF`, поэтому никакого ESM/ESP scan на обычном логине нет;
- база не переписывается на старте и не получает `refreshPending`;
- старый `config.questIndexRefreshOnServerStart = true` игнорируется ради совместимости;
- в поставляемом `config.lua` default теперь `false`.

### 2. Если базы нет — она создаётся заново

Если `server/data/custom/questIndex.json` отсутствует, повреждён или не проходит hash validation:

- quest item phasing временно остаётся выключенным;
- первый полностью авторизованный клиент получает один `MODE_UPLOAD`;
- сервер проверяет hash и сохраняет новый `questIndex.json`;
- после этого всем клиентам отправляется `MODE_OFF`.

Чтобы намеренно пересоздать базу после смены набора ESM/ESP, достаточно остановить сервер и удалить:

`server/data/custom/questIndex.json`

После следующего запуска она будет создана снова.

### 3. Безопасная генерация новой базы

В старом клиенте `QuestItemIndex::scanContentSources()` повторно открывал raw CELL contexts через `ESMReader`.
На больших/смешанных load order это давало сообщения наподобие:

`Quest Item Index skipped refs ... Failed to open ...`

и могло оборвать клиент во время логина.

X046 больше не делает raw CELL replay. Для построения новой базы используются только уже загруженные данные `ESMStore`:

- Dialogue / INFO requirements;
- result scripts;
- global/local scripts;
- inventories базовых Container/NPC/Creature;
- leveled item lists.

Это намеренно более консервативная классификация: лучше пропустить редкий спорный quest item, чем ошибочно фазировать обычный предмет или уронить клиент.

### 4. Дополнительный exception guard

`ProcessorPlayerQuestIndex` теперь оборачивает генерацию/отправку индекса в `try/catch`.
Обычная C++ exception больше не должна валить процесс клиента; oracle mode отключится и ошибка попадёт в client log.

## Что видно по присланному логу

Перед обрывом клиент получил `ID_PLAYER_QUEST_INDEX request, mode 2`, включил oracle mode и начал raw scan CELL refs. Это тот путь, который X046 убирает при наличии сохранённой базы и переписывает для первого создания.

В логе также есть `Error in frame: Spell '' not found`, но после него клиент продолжил обрабатывать Journal, Faction, destination overrides и загрузку instance-cell. Поэтому это отдельная проблема и не является точкой данного обрыва.

## Изменённые рабочие файлы

- `server/scripts/questIndexStore.lua`
- `server/scripts/config.lua`
- `apps/openmw/mwmp/QuestItemIndex.cpp`
- `apps/openmw/mwmp/processors/player/ProcessorPlayerQuestIndex.hpp`

Quest Studio X045, квест Кая, private instance fixes, Combat AI и Launcher этим патчем не изменяются.

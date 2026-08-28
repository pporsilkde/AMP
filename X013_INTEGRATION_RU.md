> **X015:** все пункты раздела «Правки в файлах, которых нет в архиве» теперь уже встроены в cumulative patch. Повторно применять их вручную не нужно.

# ArenaMP X013 — Server-owned Quest Index (вариант B: клиент-оракул, fail-closed)

Кумулятивный патч поверх X012. Protocol version: **806**.
Сервер и клиенты X013 должны быть одной сборки.

---

## Что меняется по существу

**X012:** сервер верил полю `questItem` внутри Container/ObjectDelete. Модифицированный
клиент объявлял квестовым что угодно и получал бесконечный дюп из любого сундука.

**X013:**

1. Классификация принадлежит серверу. Клиент — только **оракул**: по запросу сервера
   он строит индекс из загруженных ESM/ESP и выгружает его. Сервер пересчитывает хэш
   по фактически принятым данным, требует совпадения от N независимых игроков
   (или одной выгрузки от стаффа), сохраняет результат и дальше классифицирует сам.
2. **Fail-closed.** Пока индекс не принят — фазинг полностью выключен, поведение
   ванильное. Никакого «на всякий случай разрешим».
3. `sourceId` вычисляется на сервере. Клиент больше не выбирает ключ, под которым
   пишется его claim.
4. Источник назначается **при SET** (канонический инвентарь), а не при взятии.
   Игрок больше не может превратить обычный стек в phaseable, просто подобрав его.
5. Claims разложены по ячейкам: взятие и загрузка ячейки — O(claims в этой ячейке),
   а не O(всех claims персонажа). Есть миграция v1 → v2 с перевычислением ключей,
   так что уже взятые предметы не выдаются повторно после обновления.
6. Обычная сессия **не платит за сканирование ESM вообще**: если у сервера уже есть
   принятый индекс, он присылает клиенту `MODE_OFF`, и локальный классификатор
   не включается.

---

## Файлы патча

### Новые

```
components/openmw-mp/Base/QuestIndexData.hpp
components/openmw-mp/Packets/Player/PacketPlayerQuestIndex.hpp
components/openmw-mp/Packets/Player/PacketPlayerQuestIndex.cpp
apps/openmw/mwmp/processors/player/ProcessorPlayerQuestIndex.hpp
apps/openmw-mp/processors/player/ProcessorPlayerQuestIndex.hpp
apps/openmw-mp/Script/Functions/QuestIndex.hpp
apps/openmw-mp/Script/Functions/QuestIndex.cpp
server/scripts/questIndexStore.lua
```

### Заменяются целиком

```
components/openmw-mp/Version.hpp          (807 → 806)
apps/openmw/CMakeLists.txt                (+ ProcessorPlayerQuestIndex)
apps/openmw/mwmp/QuestItemIndex.hpp
apps/openmw/mwmp/QuestItemIndex.cpp
server/scripts/questItemPhasing.lua       (переписан)
server/scripts/cell/base.lua
server/scripts/eventHandler.lua
```

`packetBuilder.lua`, `packetReader.lua`, `ObjectList.cpp`, `PacketContainer.cpp`,
`PacketObjectDelete.cpp`, `BaseObject.hpp` **не меняются**. Поля `questItem` /
`questSourceId` остаются в протоколе, но сервер их больше не читает как истину.

---

## Правки в файлах, которых нет в архиве

Все — аддитивные, по одной строке рядом с существующим аналогом.

### 1. `components/openmw-mp/NetworkMessages.hpp`

В enum player-сообщений, **последним** (порядок ID — часть протокола, не вставляйте
в середину):

```cpp
    ID_PLAYER_QUEST_INDEX,
```

### 2. `components/openmw-mp/PacketsController.cpp`

Рядом с остальными player-пакетами:

```cpp
#include "Packets/Player/PacketPlayerQuestIndex.hpp"
...
    AddPacket<PacketPlayerQuestIndex>(&playerPackets, peer);
```

(скопируйте точную форму строки у `PacketPlayerTopic` — в разных ветках
макрос/шаблон записан по-разному).

### 3. `components/CMakeLists.txt`

В список исходников `openmw-mp/Packets/Player`:

```
    openmw-mp/Packets/Player/PacketPlayerQuestIndex
```

### 4. `components/openmw-mp/Base/BasePlayer.hpp`

Вверху:

```cpp
#include <components/openmw-mp/Base/QuestIndexData.hpp>
```

В теле `BasePlayer`, рядом с прочими членами:

```cpp
        // ArenaMP X013: payload of the last ID_PLAYER_QUEST_INDEX packet.
        QuestIndexData questIndex;
```

### 5. `apps/openmw/mwmp/processors/ProcessorInitializer.cpp`

```cpp
#include "player/ProcessorPlayerQuestIndex.hpp"
...
    PlayerProcessor::AddProcessor(new ProcessorPlayerQuestIndex());
```

### 6. `apps/openmw-mp/processors/ProcessorInitializer.cpp`

```cpp
#include "player/ProcessorPlayerQuestIndex.hpp"
...
    PlayerProcessor::AddProcessor(new ProcessorPlayerQuestIndex());
```

### 7. `apps/openmw-mp/CMakeLists.txt`

```
    processors/player/ProcessorPlayerQuestIndex.hpp
    Script/Functions/QuestIndex.cpp
    Script/Functions/QuestIndex.hpp
```

### 8. `apps/openmw-mp/Script/ScriptFunctions.hpp`

```cpp
#include "Functions/QuestIndex.hpp"
```

В общий список API (рядом с `OBJECTAPI`):

```cpp
    QUESTINDEXAPI, \
```

И в список колбэков, ровно в той же форме, что и соседний `OnPlayerTopic`:

```cpp
    {"OnPlayerQuestIndex", Function<void, unsigned short>()},
```

### 9. `server/scripts/serverCore.lua`

```lua
function OnPlayerQuestIndex(pid)
    eventHandler.OnPlayerQuestIndex(pid)
end
```

---

## Конфигурация (`config.lua`, необязательно)

```lua
-- X019: по умолчанию true. Если questIndex.json отсутствует или повреждён,
-- первый полностью принятый клиент автоматически строит индекс, сервер
-- перепроверяет hash payload и сразу сохраняет server/data/custom/questIndex.json.
-- X020: автогенерация теперь жёстко включена по умолчанию. Старый
-- config.questIndexAutoGenerate=false игнорируется, чтобы устаревший config.lua
-- не оставлял phasing выключенным. Для намеренного строгого режима:
config.questIndexRequireQuorum = false

-- Используется только когда questIndexRequireQuorum = true. Сколько независимых
-- игроков должны прислать одинаковый индекс, прежде чем сервер его примет.
-- Выгрузка от стаффа принимается сразу.
config.questIndexRequiredConfirmations = 2

-- Спрашивать хэш у каждого клиента при входе, даже когда индекс уже принят.
-- Ловит расхождение load order у игроков ценой сканирования ESM при логине.
config.questIndexVerifyOnLogin = false
```

Значения по умолчанию X020 — автоматический first-valid bootstrap,
`questIndexRequireQuorum = false`, `questIndexRequiredConfirmations = 2`,
`questIndexVerifyOnLogin = false`. Старый `questIndexAutoGenerate=false`
специально игнорируется как потенциально устаревший параметр.

---

## Первый запуск (X019+)

1. Соберите сервер и клиент из одной ревизии (protocol 806).
2. Ничего вручную создавать не нужно. Если `custom/questIndex.json` отсутствует,
   сервер при входе первого полностью авторизованного игрока пришлёт `MODE_UPLOAD`.
3. Клиент один раз просканирует ESM/ESP и отправит отсортированный индекс. Сервер
   повторно вычислит hash фактически полученных entries и только после успешной
   проверки автоматически сохранит `server/data/custom/questIndex.json`.
4. В логе появится строка вида:
   `[QuestIndex] Accepted index with N records (auto-generated from first valid upload ...); quest item phasing is now active`.
5. Остальные клиенты получают `MODE_OFF` и больше не сканируют ESM/ESP.

Если нужен старый более строгий режим, задайте
`config.questIndexRequireQuorum = true`: тогда индекс принимается от стаффа сразу
или после `questIndexRequiredConfirmations` одинаковых независимых выгрузок.

**После добавления или обновления плагина** — удалите `custom/questIndex.json`
и перезапустите сервер (или вызовите `questIndexStore.Reset()`). Индекс
пересоберётся сам при следующем входе. Больше ничего делать не нужно.

---

## Диагностика

`questIndexStore.GetStatus()` возвращает `{trusted, contentKey, indexHash,
entryCount, pendingConfirmations}` — удобно повесить на админ-команду.

В логе стоит искать:

- `[QuestIndex] No stored index` — фазинг выключен, ждём оракула.
- `[QuestIndex] Upload from X accepted as confirmation 1/2` — набирается кворум.
- `[QuestIndex] Hash mismatch from X` — клиент прислал payload, не совпадающий
  с объявленным хэшем. Отброшено.
- `[QuestIndex] ... their load order differs` — у игрока другой набор плагинов
  (только при `questIndexVerifyOnLogin = true`).
- `[QuestItemPhasing] Migrated claims of X to layout 2` — разовая миграция.
- `[QuestItemPhasing] Stamped pre-X013 quest source ...` — разовая штамповка
  инвентаря контейнера, записанного до X013.

---

## Что осталось на X014

- Привязка `item → questId` и гейт по журналу игрока: источник фазируется только
  пока связанный квест не закрыт. Это то, что окончательно убирает ложные
  срабатывания и течь в экономику.
- `custom/questPhasing/overrides.json` (`force` / `never`) и авто-отчёт по
  классификации — ручной аварийный выход без пересборки движка.
- Админ-команды `/qindex status|reset|why <refId>`.

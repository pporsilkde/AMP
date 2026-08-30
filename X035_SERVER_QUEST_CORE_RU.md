# ArenaMP X035 — Server Quest Core

X035 — первый рабочий слой системы серверных квестов ArenaMP. Он не заменяет оригинальные DIAL/INFO/Journal Morrowind, а создаёт server-authoritative слой, который в X036+ будет визуально встроен в обычное окно диалога и журнал клиента.

## Что уже работает в X035

- Определения квестов хранятся на сервере отдельными JSON-файлами в `server/data/custom/quests/`.
- Есть `index.json`, версия definition, статус `draft/published/disabled`, автор и audit history.
- Новый квест может создать/редактировать только Moderator, Admin или Server Owner.
- Можно запретить модераторам Publish через `config.serverQuests.moderatorsCanPublish = false`.
- Встроенный внутриигровой редактор открывается командой `/quest` и использует стандартные TES3MP `CustomMessageBox/ListBox/InputDialog`.
- Редактор умеет: создать draft, выбрать giver, добавить будущий зелёный topic, стадии, requirements, rewards, Validate, Publish/Disable и запустить квест на себе для теста.
- Персональный прогресс хранится в `player.data.customVariables.serverQuests` и сохраняется обычным player JSON/Quicksave.
- Есть server-side custom journal. В X035 он читается через `/quest journal`; X038 подключит его к обычному Journal GUI.
- Есть API `GetAvailableTopics(pid, actorRefId, cell)` и `GetCurrentDialogue(pid, questId)` для X036. Topics уже помечаются `green=true`.
- Опубликованный definition с ошибками validation автоматически runtime-disabled: он не выдаётся игроку, пока ошибка не исправлена и `/quest reload` не перечитает файл.
- Награда каждой стадии выдаётся at-most-once. Перед выдачей сохраняется `pendingRewardStage`, после успешной выдачи — `rewardedStages`. После аварии reward не дублируется автоматически.
- XP-награда учитывает общий серверный `XP Leveling -> xp gain multiplier`, поэтому изменение скорости XP работает и для новых серверных квестов.

## Установка

Используйте cumulative X035 или наложите FIX поверх X034. Новые runtime-файлы:

- `server/scripts/serverQuestSystem.lua`
- `server/data/custom/quests/index.json`
- `server/data/custom/quests/arena_example_missing_ring.json`
- `server/data/custom/quests/_TEMPLATE.json`

`serverCore.lua` сам загружает `serverQuestSystem` при старте.

## Быстрый старт через интерфейс

1. Зайдите под Moderator/Admin.
2. В чате введите `/quest`.
3. Нажмите `New quest`.
4. Введите, например:

   `arena_rat_problem | Крысы в подвале`

5. В карточке квеста нажмите `Giver` и введите:

   `caius cosades | Balmora, Caius Cosades' House`

   Вторая часть (cell) необязательна. Если cell пустая, refId giver подходит во всех его ячейках.

6. `Add topic`:

   `arena_rat_problem_topic | крысы в подвале`

   Этот topic уже сохраняется как `green=true`. X036 отрисует его зелёным в обычном DialogueWindow.

7. `Add stage`:

   `10 | Кай попросил меня разобраться с крысами. | В подвале развелись крысы. Разберись с ними.`

8. Добавьте вторую стадию:

   `20 | Я разобрался с крысами. | Хорошая работа. Вот твоя награда.`

9. `Reward` для стадии 20:

   `20 | gold | 250`

   Потом ещё один reward:

   `20 | xp | 125`

10. Переход между стадиями пока удобнее задать командой:

    `/quest next arena_rat_problem 10 20`

11. Финальную стадию отметить завершением:

    `/quest complete arena_rat_problem 20 true`

12. Нажмите `Validate`. Исправьте `ERROR`, предупреждения `WARN` не блокируют Publish.
13. `Publish`.
14. Для теста нажмите `Test me`, затем:

    `/quest advance arena_rat_problem 20`

15. Посмотреть журнал:

    `/quest journal arena_rat_problem`

## Основные команды

### Для игрока

- `/quest` — открыть окно Server Quests.
- `/quest my` — активные/завершённые серверные квесты.
- `/quest journal` — весь серверный журнал.
- `/quest journal QUEST_ID` — журнал одного квеста.

Игрок без staff rank не может создавать или редактировать definition.

### Для Moderator/Admin

- `/quest list`
- `/quest new ID NAME...`
- `/quest giver ID REFID [CELL...]`
- `/quest topic ID TOPIC_ID TEXT...`
- `/quest stage ID INDEX JOURNAL_TEXT...`
- `/quest dialogue ID INDEX DIALOGUE_TEXT...`
- `/quest next ID FROM_STAGE TO_STAGE`
- `/quest complete ID STAGE true|false`
- `/quest validate ID`
- `/quest publish ID`
- `/quest disable ID`
- `/quest start ID [PID]`
- `/quest advance ID STAGE [PID]`
- `/quest reset ID [PID]`
- `/quest reload` — перечитать `server/data/custom/quests/*.json`, перечисленные в `index.json`.

Admin дополнительно может `/quest delete ID` (soft-delete, файл остаётся для audit) и разруливать прерванные reward-транзакции:

- `/quest rewardresolve ID PID skip`
- `/quest rewardresolve ID PID retry`

`retry` выполняется только явно администратором, потому что после crash нельзя доказать, какая часть внешних inventory-пакетов успела примениться.

## Requirements X035

Поддерживаются:

- `level`
- `gold`
- `item`
- `questStage`
- `questState`
- `playerVariable`
- `serverVariable`
- `staffRank`

Примеры:

```text
/quest require arena_rat_problem 10 level >= 5
/quest require arena_rat_problem 20 item rat_meat_01 >= 3
/quest require arena_second_quest 10 questStage arena_rat_problem >= 20
```

В JSON requirement выглядит так:

```json
{
  "type": "item",
  "refId": "rat_meat_01",
  "operator": ">=",
  "count": 3,
  "value": 3
}
```

## Rewards/actions X035

Поддерживаются:

- `gold`
- `xp`
- `item` / `giveItem`
- `takeItem`
- `message`
- `setPlayerVariable` (через JSON; GUI/command shortcut добавим позднее)

Примеры:

```text
/quest reward arena_rat_problem 20 gold 250
/quest reward arena_rat_problem 20 xp 125
/quest reward arena_rat_problem 20 item potion_health_standard 2
/quest reward arena_rat_problem 20 takeItem rat_meat_01 3
```

XP по умолчанию масштабируется серверной скоростью XP. В JSON можно поставить `"scaled": false`, если нужна абсолютная XP-награда.

## Формат definition

Минимальная структура:

```json
{
  "schemaVersion": 1,
  "id": "arena_rat_problem",
  "version": 1,
  "name": "Крысы в подвале",
  "author": "ModeratorName",
  "status": "draft",
  "progressMode": "personal",
  "giver": {
    "refId": "caius cosades",
    "cell": "Balmora, Caius Cosades' House"
  },
  "initialStage": 10,
  "topics": [
    {
      "id": "arena_rat_problem_topic",
      "text": "крысы в подвале",
      "enabled": true,
      "green": true
    }
  ],
  "stages": []
}
```

Используйте `_TEMPLATE.json` как основу для ручного редактирования.

## Жизненный цикл

```text
DRAFT -> VALIDATE -> PUBLISHED
                    |
                    v
                 DISABLED
```

Любое редактирование уже опубликованного квеста через editor/commands автоматически возвращает его в `draft`. Это специально: изменение награды или требования не должно незаметно менять живой MMO-квест.

## Что пока намеренно НЕ делает X035

- Не добавляет зелёную строку непосредственно в `DialogueWindow` — server topic/API уже готовы, отображение будет в X036.
- Не вставляет custom journal text прямо в ванильный Journal GUI — это X038, потому что обычный клиент не имеет ESM `JOUR/INFO` records с server quest ID.
- Нет visual graph editor; текущий UI — рабочая первая версия на стандартных TES3MP dialogs.
- `party/server` progressMode уже валидируется и хранится, но исполняется как будущая схема. X035 runtime реализует прежде всего `personal`.
- Нет автоматического event-driven advancement по kill/activate/container. Это следующий server runtime слой после диалогового протокола.

## Следующий этап X036

1. Добавить новый versioned QuestSync packet.
2. При открытии диалога клиент запрашивает доступные server topics для конкретного NPC.
3. `DialogueWindow` объединяет vanilla topics + server topics.
4. Server topics получают отдельный `DialogueTopicSource::ServerQuest` и зелёный цвет.
5. Клик по зелёному topic отправляет `questId/topicId/actor`, сервер выбирает актуальную stage/INFO и возвращает текст/choices.
6. Клиент ничего не решает по conditions/reward самостоятельно.

Так X035 definitions останутся совместимы с X036 без миграции JSON.

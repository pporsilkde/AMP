# ArenaMP X037 — compile fixes + Server Quest Choices + Quest Manager journal

X037 построен поверх X036 и включает оба присланных compile-hotfix без отката Server Quest System.

## Встроенные compile-fix

### X034a
`apps/openmw/mwmechanics/aitravel.hpp`

Добавлен:

```cpp
#include <components/esm/loadcell.hpp>
```

`AiReturnHomeState` хранит `ESM::Cell` по значению, поэтому MSVC обязан видеть полное определение типа. Это исправляет C2079 из `logs_90192704950.zip` для `mFromCell`, `mToCell`, `mHomeCell`.

### X034b
`apps/openmw/mwworld/actionteleport.cpp`

Вызов `recordDoorTransition` приведён к фактической X034-сигнатуре:

```cpp
recordDoorTransition(const ESM::Cell&, const ESM::Position&,
                     const ESM::Cell&, const ESM::Position&)
```

Старый 5-аргументный вызов `CellId + CellName` удалён. Это исправляет MSVC C2660.

## Server Quest Choices

X037 добавляет server-authoritative варианты ответа прямо в обычную правую панель DialogueWindow.

Клиент никогда не выполняет Choice как локальный result script. Сервер отправляет только видимые игроку варианты и при нажатии повторно проверяет их по текущему состоянию, стадии и requirements.

Поддерживаемые действия Choice:

- `start` — принять/начать квест;
- `advance` — перейти на `targetStage`;
- `none` — оставить состояние без изменений.

У каждого Choice может быть свой `requirements`.

### Quest offer

Новый необязательный объект `offer` показывается до принятия квеста:

```json
"offer": {
  "dialogue": "Мне нужна помощь.",
  "choices": [
    {
      "id": "accept",
      "text": "Я помогу.",
      "action": "start",
      "requirements": []
    },
    {
      "id": "decline",
      "text": "Не сейчас.",
      "action": "none",
      "requirements": []
    }
  ]
}
```

Открытие зелёного topic само по себе больше не стартует Choice-driven квест. Состояние создаётся только после server-validated `start`.

### Choices на стадии

```json
"choices": [
  {
    "id": "hand_over",
    "text": "Вот предмет.",
    "action": "advance",
    "targetStage": 20,
    "requirements": [
      {
        "type": "item",
        "refId": "potion_mazte_01",
        "operator": ">=",
        "count": 1
      }
    ]
  },
  {
    "id": "later",
    "text": "Пока нет.",
    "action": "none",
    "requirements": []
  }
]
```

Если requirement не выполнен, сервер вообще не отправляет этот Choice клиенту.

## Команды Moderator/Admin

Добавлены:

```text
/quest offer QUEST_ID ТЕКСТ ПРЕДЛОЖЕНИЯ
```

и:

```text
/quest choice QUEST_ID offer CHOICE_ID start - ТЕКСТ
/quest choice QUEST_ID STAGE CHOICE_ID advance TARGET_STAGE ТЕКСТ
/quest choice QUEST_ID STAGE CHOICE_ID none - ТЕКСТ
```

Сложные `requirements` для Choice сейчас удобнее задавать в JSON. В следующем UI-этапе их можно вынести в отдельный визуальный редактор.

## Встроенный пример Кая Косадеса

Квест `arena_caius_drink` обновлён до version 2.

1. Поговорить с `caius cosades` в `Balmora, Caius Cosades' House`.
2. Нажать зелёный topic `немного выпивки`.
3. Кай предлагает квест, но он ещё НЕ начат.
4. Выбрать `Хорошо, принесу тебе мацт.` — создаётся stage 10 и запись журнала.
5. Пока у игрока нет `potion_mazte_01`, вариант сдачи бутылки скрыт сервером.
6. После получения бутылки снова открыть зелёный topic.
7. Появится `Вот бутылка мацта.`.
8. Сервер проверяет предмет повторно, переводит на stage 20, забирает 1 мацт, выдаёт 75 золота + 40 XP и завершает квест.

## Server quests в Quest Manager

X037 синхронизирует отдельно от dialogue topics:

- `STATE` — quest id/name/giver/status/stage;
- `JOURNAL` — stage/date/text.

Начатые и завершённые server quests добавляются в существующий Quest Manager вместе с обычными JOUR-квестами.

Фильтры:

- Add-on: `ArenaMP Server` / `Сервер ArenaMP`;
- Category: `Server quests` / `Серверные квесты`.

Server quest НЕ записывается как выдуманный ESM JOUR record в локальный save. Авторитетное состояние остаётся в player JSON на сервере и восстанавливается при логине. Это не загрязняет и не повреждает vanilla save.

## Совместимость

- Старые линейные server quests без `offer/choices` продолжают работать по X036-логике.
- X037 client + X037 server рекомендуется использовать вместе.
- X034 Combat AI, X033 Water/Chat, X032 Launcher, X031 Occlusion не откатываются.

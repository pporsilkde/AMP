# ArenaMP X043a — Server Quest Topic Click Fix

Основа: X043.

## Симптом

Зелёный server quest topic отображается у NPC, но после нажатия ничего не происходит.
В server log:

`Rejected server quest token @ArenaQuest:...|@ArenaQuest:...`

## Причина

`ObjectList::addObjectDialogueChoice()` считает любой `TOPIC` обычным локализуемым DIAL topic.
При включённой RU translation storage он сериализует topic как:

`displayText|canonicalTopicId`

Для opaque ArenaMP token translation storage возвращала тот же token, поэтому пакет содержал:

`@ArenaQuest:quest:topic|@ArenaQuest:quest:topic`

ServerQuest parser ожидает один token и отклонял пакет.

## Исправление X043a

### Client
`apps/openmw/mwmp/ObjectList.cpp`

`@ArenaQuest:` и `@ArenaQuestChoice:` теперь считаются opaque transport tokens и не проходят через vanilla topic translation.
Обычные локализованные темы Morrowind продолжают использовать прежний `label|topicId` формат.

### Server
`server/scripts/serverQuestSystem.lua`

Добавлена совместимость с уже существующими X043-клиентами: если сервер получает старый формат
`serverToken|serverToken`, parser безопасно использует первый token.

Это позволяет сначала обновить только сервер, а затем клиенты; после обновления клиента двойной token вообще перестаёт отправляться.

## Проверка

1. Поговорить с Каем Косадесом.
2. Нажать зелёный `немного выпивки`.
3. Должен появиться текст Кая и варианты ответа.
4. В server log больше не должно быть `Rejected server quest token` для корректного topic.
5. После выбора `Хорошо, принесу тебе мацт.` квест переходит на stage 10.

## Совместимость

X043a не меняет packet wire-format, JSON definitions или protocol version.
Все X043 исправления сохраняются.

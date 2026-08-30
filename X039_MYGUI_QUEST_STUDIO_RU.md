# ArenaMP X039 — настоящий MyGUI Server Quest Studio

X039 заменяет основной X038 quest editor на полноценное клиентское MyGUI-окно. Старый CustomMessageBox/InputDialog editor сохранён как fallback `/quest legacy`.

## Открытие

Moderator/Admin в игре:

`/quest`

Сервер отправляет staff-only модель через скрытый `ID_GUI_MESSAGEBOX` transport (`id=-35036`). Клиент X039 перехватывает `EDITOR_*` сообщения до создания message box и открывает `ServerQuestEditorWindow : WindowModal` из `files/mygui/arenamp_serverquesteditor.layout`.

Обычный игрок редакторскую модель не получает.

## Макет

Слева постоянно видны:
- поиск;
- список квестов с `[PUB] / [DRAFT] / [OFF]`;
- New / Clone / Delete(soft delete, Admin).

Справа 5 вкладок:

### Overview
- quest id (read-only);
- имя;
- status/version/author;
- progress mode personal/party/server;
- giver RefId / Cell / uniqueIndex;
- Initial stage;
- `Pick in game`.

`Pick in game` закрывает Studio. Следующая активация NPC сохраняет refId/cell/uniqueIndex, после чего сервер автоматически пересылает модель и Studio открывается снова.

### Dialogue
- зелёные server topics;
- enabled/disabled;
- offer text;
- offer choices;
- action none/start/advance;
- target stage.

### Stages
- список стадий;
- journal text;
- NPC dialogue;
- initial/complete/fail;
- stage choices;
- transitions.

При переименовании индекса стадии сервер перепривязывает initialStage, next и choice.targetStage. При удалении стадии ссылки на неё очищаются.

### Logic
Requirements можно привязать к:
- выбранной стадии;
- выбранному Choice стадии;
- выбранному Offer Choice.

Поддерживаются level/item/gold/questStage/questState/playerVariable/serverVariable/staffRank.

Rewards привязаны к стадии: gold/xp/item/giveItem/takeItem/setPlayerVariable/message.

### Validation
Показывает server-side errors/warnings. Publish всегда повторно запускает Validation на сервере. Если `moderatorsCanPublish=false`, Moderator может редактировать, но Publish доступен только Admin/Owner.

## Безопасность

Клиент не сохраняет definitions. Любая кнопка отправляет только `EDITOR_CMD` через существующий надёжный GUI packet. Сервер повторно проверяет роль, quest id, stage/choice target, requirement/reward type и публикацию. Published quest при любом содержательном изменении возвращается в Draft.

## Совместимость

- X035 core сохранён.
- X036 зелёные topics сохранены.
- X037 Choices + серверные записи в обычном Quest Manager сохранены.
- X034a/X034b compile fixes сохранены через X037 cumulative.
- Wire packet structure не изменяется: используется существующий `ID_GUI_MESSAGEBOX`, но для MyGUI Studio нужен клиент X039.
- Для старого клиента или диагностики: `/quest legacy`.

## Пример Кая

Открой `/quest`, выбери `Выпивка для Кая` / `arena_caius_drink`.

В Dialogue видны offer и choices. В Stages — стадия просьбы и завершения. В Logic у варианта сдачи находится item requirement `potion_mazte_01 >= 1`, а на финальной стадии — 75 gold + 40 XP.

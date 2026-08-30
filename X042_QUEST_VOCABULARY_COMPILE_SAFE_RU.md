# X042 — Quest Vocabulary + MSVC compile fix (safe merge)

База: ArenaMP X041 cumulative с MyGUI Quest Studio X039, Combat AI X034 и предыдущими слоями.

## Исправление сборки из лога

`apps/openmw/mwmp/ActorList.cpp` вызывал `mwmp::MechanicsHelper::getTarget()`, но `MechanicsHelper` в этой ветке объявлен в глобальном namespace. Исправлено на `MechanicsHelper::getTarget()`. Это закрывает MSVC C2039/C3083/C3861 на ActorList.cpp:95.

## Quest vocabulary

Условия теперь поддерживают старые типы плюс:
- skill, attribute
- faction, factionRank
- reputation, bounty
- race, class, cell
- global, vanillaJournal
- questCompleted, questNotStarted
- realTime, cooldown

Массив requirements по-прежнему означает AND. В JSON можно строить вложенные группы `all`, `any`, `not` (глубина до 8). Старые definitions совместимы без миграции.

Награды/действия дополнены:
- setServerVariable
- addSpell / removeSpell
- setReputation / setBounty
- teleport
- playSound
- messageBox
- setVanillaJournal

## Дополнительные safety-fix после ревью X042

1. `addSpell/removeSpell` используют реальный API этой ветки `SetSpellbookChangesAction`, а не отсутствующий `SetSpellbookAction`.
2. reputation/bounty читаются и сохраняются в authoritative `player.data.fame`, а не в устаревший `data.stats`.
3. skill/attribute/faction lookup сделан без учёта регистра.
4. `setServerVariable` немедленно делает world Quicksave, если API доступен.
5. vanilla journal whitelist сравнивается без учёта регистра.
6. `setServerVariable` и `setVanillaJournal` действительно требуют Administrator approval. Moderator может собрать draft, но Publish блокируется. После любого редактирования admin approval сбрасывается и требуется повторно.
7. `config.serverQuestVanillaJournalWhitelist = {}` добавлен явно. По умолчанию изменение оригинального журнала полностью запрещено.

## MyGUI Quest Studio

Новые простые типы conditions/rewards добавлены в списки Quest Studio. Поля остаются универсальными:
- Requirement: Type / Operator / Value / Ref-Key
- Reward: Type / Value A / Value B

Примеры:
- skill: Ref=`Long Blade`, Value=`40`, Operator=`>=`
- factionRank: Ref=`Fighters Guild`, Value=`3`
- vanillaJournal: Ref=`A1_1_FindSpymaster`, Value=`14`
- cooldown: Ref=`arena_repeatable_01`, Value=`86400`
- addSpell: Value A=`firebite`
- setReputation: Value A=`25`
- setServerVariable: Value A=`eventStage`, Value B=`2` (Admin publish required)
- setVanillaJournal: Value A=`A1_1_FindSpymaster`, Value B=`15` (Admin + whitelist required)

`all/any/not` пока создаются через JSON; отдельный визуальный boolean-tree editor логично сделать следующим этапом.

## ID_PLAYER_BASEINFO

В `apps/openmw-mp/Networking.cpp` после полной inline-обработки BaseInfo для POSTLOADED добавлен `return`. Это убирает ложный серверный warning `Unhandled PlayerPacket with identifier 142` на входе игрока.

## Проверки

- Lua syntax: OK (serverQuestSystem.lua, config.lua)
- X039 MyGUI regression: 15/15
- X037 Caius/Choices regression: 18/18
- X042 vocabulary/safety harness: 21/21
- ActorList namespace check: `mwmp::MechanicsHelper` больше отсутствует
- `SetSpellbookAction` больше отсутствует

Полная MSVC сборка в текущем окружении не выполнялась. Исправление ActorList сделано непосредственно по присланной ошибке компилятора.

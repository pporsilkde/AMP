# ArenaMP X034 — Combat AI State Sync / Home Route Recovery

X034 перерабатывает multiplayer-состояние боевого AI NPC. Цель — убрать зависания и потерю состояния при 2+ игроках, смене cell authority и переходах через teleport doors.

## Основные исправления

### 1. Authority-only ActorAI
Сервер принимает `ID_ACTOR_AI` только от текущего authority ячейки. Запоздавший пакет от прежнего владельца больше не может перезаписать свежую цель, завершить бой или испортить маршрут NPC.

### 2. Полный Combat snapshot вместо одной цели
`PacketActorAI` теперь передаёт до 8 combat targets, текущую primary target, return-home anchor и до 12 door breadcrumbs. `LocalActor` отправляет snapshot при изменении набора целей/основной цели/маршрута и периодический heartbeat во время боя.

Добавлен внутренний AI action `COMBAT_END = 7`: он снимает только Combat/Pursuit, но не уничтожает исходный Wander/Travel и return-home state.

**Важно:** X034 меняет формат `ID_ACTOR_AI`; сервер и клиенты должны быть собраны из одной X034 версии.

### 3. Handoff между игроками
Перед превращением DedicatedActor -> LocalActor последний сетевой AI snapshot повторно применяется к реальному Ptr. Основная цель сохраняется первой. Если вторая/третья цель ещё не создана после cell load, DedicatedActor повторяет resolution через 0.5 с вместо окончательного удаления цели.

### 4. Возврат по дверям
`AiInternalTravel` теперь хранит полные from/to cell + positions и возвращается по breadcrumbs строго в обратном порядке: `A -> B -> C` возвращается `C -> B -> A`.

После каждого обратного teleport-door hop отправляется `COMBAT_END + return-home snapshot`; старое поведение WANDER/CANCEL после первой двери больше не стирает остаток маршрута.

Лимит `combat pursuit max door transitions` берётся из settings и учитывает сохранённое число breadcrumbs, поэтому смена authority не сбрасывает лимит погони.

### 5. Серверное сохранение home/route
`cell/base.lua` сохраняет:
- исходную cell;
- исходные XYZ/rotZ;
- `awayTime`;
- route до 12 переходов (`fromCell/fromXYZ -> toCell/toXYZ`).

Reverse transition снимает последний hop. Destination cell сразу quicksave'ится, чтобы рестарт сервера не потерял последний переход. Server-issued TRAVEL прикладывает home/route к ActorAI нового authority.

`actorHomeReturnDelay` уменьшен с 25 до 8 секунд. Периодический интервал проверки остаётся прежним.

### 6. NPC больше не зависает после двери
После настоящего interior/exterior перехода AiCombat сбрасывает старое attack/action решение, cooldown/attack state и строит решение заново. Проверка «действительно ли NPC участвует в этом бою» теперь использует сам AiCombat target, а не несинхронизированный `hitAttemptActorId`.

### 7. Боевая музыка
Battle music на конкретном клиенте теперь включается только если NPC действительно имеет Combat package против локального игрока. Бой другого игрока в той же multiplayer-cell больше не держит музыку клиента в Battle после его собственного disengage.

### 8. Возвращённые краденые вещи
При restitution снимается Combat только с активного offender, а не со всех целей NPC. Если NPC всё ещё сражается с другим игроком, его агрессия против второй цели сохраняется. Подбор возвращённого предмета запускается только когда других combat targets больше нет.

## Что проверить в игре
1. Два игрока входят в одну cell; оба по очереди атакуют одного NPC. NPC должен сохранять набор агро-целей и не переключаться в idle от чужого запоздавшего AI-пакета.
2. Один игрок выходит через дверь, NPC преследует его, второй остаётся. После перехода NPC должен продолжить атаку без необходимости повторно ударять его.
3. Маршрут через 2–3 двери. После потери всех целей NPC должен пройти/телепортироваться назад по тем же breadcrumbs и вернуться к исходной позиции.
4. Во время возврата сменить authority (выход владельца cell). Новый authority должен продолжить остаток маршрута.
5. Потерять локальную цель, оставив бой NPC с другим игроком: на первом клиенте Battle music должна закончиться.
6. Вернуть/выбросить украденное у NPC: hostility к этому offender должна сниматься без прощения другого активного attacker.

## Проверки в этой сборке
- Lua syntax: OK для `cell/base.lua`, `logicHandler.lua`, `packetBuilder.lua`, `config.lua`.
- X034 logic harness: route, multi-target primary, unresolved retry, stale authority rejection, local battle music, door limit across handoff — OK.
- `git diff --check`: новых whitespace errors нет.
- CMake configure был запущен, но окружение не содержит OpenGL development libraries (`FindOpenGL`), поэтому полная C++ компиляция здесь не выполнена.

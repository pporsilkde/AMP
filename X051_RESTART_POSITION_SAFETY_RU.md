# ArenaMP X051 — Restart Position Safety

База: X050d.

## Проблема

После перезапуска сервера персонаж, оставленный в произвольной ячейке, мог появляться ниже пола/ландшафта и продолжать падать сквозь геометрию.

В стандартном TES3MP 0.8.1 `ID_PLAYER_POSITION` только пересылается другим клиентам и не вызывает Lua callback. Поэтому CoreScripts сохраняли позицию внутри текущей ячейки в основном при `PlayerCellChange`, а на `OnPlayerDisconnect` заново читали `GetPos*()` из native peer и перезаписывали `data.location`. Во время рестарта/закрытия соединения это ненадёжная точка для финального snapshot.

## Что сделано

### 1. OnPlayerPosition на сервере

`apps/openmw-mp/processors/player/ProcessorPlayerPosition.hpp` теперь вызывает:

```cpp
Script::Call<Script::CallbackIdentity("OnPlayerPosition")>(player.getId());
```

перед обычным forwarding пакета.

`apps/openmw-mp/Script/ScriptFunctions.hpp` регистрирует callback `OnPlayerPosition(unsigned short)`.

`serverCore.lua` передаёт callback в `eventHandler.OnPlayerPosition(pid)`.

### 2. Последняя позиция хранится постоянно

Новый `server/scripts/positionSafetyHelper.lua` на каждом принятом `PlayerPosition` обновляет:

- `posX/posY/posZ`;
- `rotX/rotZ`;
- last-known-good transform в `Players[pid].data.location`.

Дисковые quicksave ограничены `autosaveSeconds` (по умолчанию 10 секунд), поэтому callback не пишет JSON на каждый сетевой пакет.

### 3. Disconnect больше не перезаписывает позицию из закрывающегося peer

Из `OnPlayerDisconnect` удалён финальный:

```lua
Players[pid]:SaveCell(packetReader.GetPlayerPacketTables(pid, "PlayerCellChange"))
```

Вместо него сохраняется уже накопленный в памяти last-known-good transform. Обычный `SaveToDrive()` остаётся и сохраняет весь профиль.

### 4. Login collision guard

После `OnPlayerAuthentified` запоминается серверная позиция входа. По умолчанию проверки выполняются через:

- 400 ms;
- 1200 ms;
- 3000 ms.

Если клиент начинает уходить вниз более чем на 24 units при почти неизменных X/Y, такое положение рассматривается как reconnect fall-through:

- плохой Z не сохраняется;
- сервер повторно отправляет исходный `SetPos/SendPos`;
- после последней проверки guard отключается.

Если игрок нормально начинает двигаться по горизонтали более чем на 96 units, guard отключается сразу и больше его не возвращает назад.

Для private dynamic cells существующий X043 restore остаётся главным: новый helper не перезаписывает позицию, пока `privateCellLoginRestore` активен.

## Config

```lua
config.positionSafety = {
    enabled = true,
    autosaveSeconds = 10,
    loginGuardEnabled = true,
    loginGuardDelaysMs = {400, 1200, 3000},
    loginGuardFallThreshold = 24,
    loginGuardMoveCancelDistance = 96,
    maxAbsCoordinate = 100000000
}
```

## Изменённые файлы

- `apps/openmw-mp/processors/player/ProcessorPlayerPosition.hpp`
- `apps/openmw-mp/Script/ScriptFunctions.hpp`
- `server/scripts/positionSafetyHelper.lua`
- `server/scripts/eventHandler.lua`
- `server/scripts/serverCore.lua`
- `server/scripts/config.lua`
- `server/scripts/player/base.lua`

## Проверка

- `texluac -p` изменённых Lua: OK.
- `X051_position_safety_harness.lua`: OK.
- callback добавлен в `ScriptFunctions::callbacks`: OK.
- `OnPlayerDisconnect` больше не вызывает native `SaveCell(PlayerCellChange)`: OK; единственный оставшийся вызов находится в обычном `OnPlayerCellChange`.
- patch dry-run X050d -> X051: OK.

Полная MSVC/Ninja сборка должна быть подтверждена на полном дереве ArenaMP, потому что кумулятив содержит только заменяемые/изменённые исходники, а не весь upstream checkout.

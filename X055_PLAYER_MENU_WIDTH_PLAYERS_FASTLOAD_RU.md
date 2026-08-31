# ArenaMP X055 — ширина Player Menu, быстрый список игроков, подсказка Y

База: ArenaMP X054.

## Изменения

1. Минимальная ширина расширенного Player Menu увеличена с 700 до 800 px. HUD-чат остаётся 260×400 и не меняется.
2. Два ряда палитры по 20 цветов теперь гарантированно помещаются целиком. При 800 px ширины ColorBar имеет 780 px, последний (20-й/40-й) swatch заканчивается на x=762.
3. `tes3mp-client-default.cfg` теперь использует `w = 800`, `h = 500`; новые ключи не добавлялись, поэтому дубликатов `[Chat]` нет.
4. После логина/регистрации выводится только подсказка по Y:
   - RU: `Y: нажмите — быстрый ввод в чат; удерживайте — меню игрока`;
   - EN: `Y: tap — quick chat input; hold — Player Menu`.
   F2 и `/help` из этой стартовой подсказки удалены.
5. Внутренние протокольные сообщения `@@AMP_*@@` теперь обходят фильтр системного чата RP. Это касается состояния Player Menu и не выводит служебные строки пользователю.
6. `playerListHelper` отправляет первичное состояние вкладки «Игроки» сразу после `OnPlayerAuthentified`. При открытии вкладки остаётся и обычный явный `/playerlistui state`, поэтому список обновляется сразу и не зависит от фонового события.

## Изменённые файлы

- `apps/openmw/mwmp/GUI/GUIChat.cpp`
- `files/tes3mp/tes3mp-client-default.cfg`
- `server/scripts/config.lua`
- `server/scripts/eventHandler.lua`
- `server/scripts/coreChat.lua`
- `server/scripts/playerListHelper.lua`

## Проверка

- Lua syntax: OK
- X054 quest localization harness: 31/31
- X054 chat logic harness: 41/41
- X055 geometry harness: OK
- `[Chat]` duplicate keys in `tes3mp-client-default.cfg`: 0

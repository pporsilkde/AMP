# ArenaMP X050c — MyGUI TextBox compile fix

База: X050b.

Исправлена компиляция `GUIChat.cpp` на MyGUI из ветки OpenMW/TES3MP 0.47.

## Причина

`GroupNameLabel` и `GroupTargetLabel` в `files/mygui/tes3mp_chat.layout` имеют тип `TextBox`, но в `GUIChat.hpp` были объявлены как `MyGUI::Widget*`. В этой версии MyGUI базовый `Widget` не имеет метода `setCaption()`, поэтому MSVC выдавал C2039 на строках 312-313 `GUIChat.cpp`.

## Исправление

- добавлена forward declaration `class TextBox;`
- `mGroupNameLabel` изменён с `MyGUI::Widget*` на `MyGUI::TextBox*`
- `mGroupTargetLabel` изменён с `MyGUI::Widget*` на `MyGUI::TextBox*`

Layout и поведение не менялись. Серверные Lua-файлы X050b не менялись.

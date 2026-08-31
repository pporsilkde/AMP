# ArenaMP X053a — MyGUI 3.2 FontManager compatibility

База: ArenaMP X053 COLOR EMOJI.

## Причина

X053 проверял наличие шрифта через `MyGUI::FontManager::isExist()`.
В MyGUI 3.2 `FontManager` предоставляет `getByName()`, но не `isExist()`.
Поэтому три проверки переведены на совместимый API:

```cpp
MyGUI::FontManager::getInstance().getByName(name) != nullptr
```

Поведение не меняется: отсутствующий `ArenaMPChatColor` по-прежнему приводит к
fallback на DejaVuLGCSansMono/ASCII-палитру.

## Лицензия OpenMoji

Добавлен `OPENMOJI_ATTRIBUTION.txt` для атрибуции OpenMoji, так как цветной
атлас содержит производные OpenMoji-изображения (CC BY-SA 4.0).

## Изменения

- `apps/openmw/mwmp/GUI/GUIChat.cpp`
- новый `OPENMOJI_ATTRIBUTION.txt`
- новый `X053a_MYGUI32_FONT_COMPAT_RU.md`

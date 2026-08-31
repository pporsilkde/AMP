# ArenaMP X053b — MyGUI 3.2 GUIChat compile fix

База: X053a.

Исправляет ошибки MSVC из GUIChat.cpp:

- `MyGUI::FontManager::isExist` отсутствует в MyGUI 3.2: все проверки используют `FontManager::getByName(...) != nullptr`.
- `MyGUI::Widget::setCaption` отсутствует в MyGUI 3.2: X052 caption-aware layout больше не вызывает текстовый API на базовом `Widget*`.
- Добавлен единый `setWidgetCaption(MyGUI::Widget*, ...)`, который безопасно работает с `MyGUI::Button`, `MyGUI::TextBox` и `MyGUI::EditBox` через `castType(..., false)`.
- `setMenuCaption`, `refitCaption` и `layoutRow` используют новый helper.

Серверная логика X051 position safety, X050 groupHelper, квесты, конфиги и emoji atlas не изменялись.

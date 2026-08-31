# ArenaMP X050a — helperAreana refactor

База: X050 Group Helper / Player Menu.

Изменения:
- `server/scripts/custom/arenampOwnerConsole.lua` удалён из встроенных custom-скриптов.
- Его логика перенесена в `server/scripts/helperAreana.lua`.
- Внутреннее имя модуля переименовано в `helperAreana`.
- `server/scripts/serverCore.lua` теперь загружает модуль напрямую: `helperAreana = require("helperAreana")`.
- `server/scripts/customScripts.lua` больше не загружает owner/console helper и снова содержит только шаблон для пользовательских custom-скриптов.
- Поведение владельца сервера, `staffRank 3`, console gating и команда `/owner` сохранены.

Важно при ручном наложении только изменённых файлов:
удалить старый `server/scripts/custom/arenampOwnerConsole.lua`, иначе он останется физически в дереве, хотя больше не загружается.

# ArenaMP X034c — compile fix

Исправлены два блокера компиляции, найденные после X034b:

1. `apps/openmw/mwmp/ActorList.cpp`
   - `MechanicsHelper` в ветке TES3MP 0.8.x находится в глобальном namespace.
   - Исправлен ошибочный вызов `mwmp::MechanicsHelper::getTarget(ptr)` на `MechanicsHelper::getTarget(ptr)`.

2. `apps/launcher/graphicspage.cpp`
   - В выражении пресета `small feature culling pixel size` не хватало одной закрывающей скобки вызова `Settings::Manager::setFloat(...)`.
   - Добавлена недостающая `)`.

Также синхронизированы исходные patch-файлы X033 и X034, чтобы cumulative оставался воспроизводимым.

Предупреждения D9025/C4245/C4305/C4101/C4458 из предоставленного лога не являются причиной остановки сборки.

# X034b — compile fix actionteleport

Исправлена ошибка MSVC C2660 в `apps/openmw/mwworld/actionteleport.cpp`.

После X034 `AiSequence::recordDoorTransition` принимает 4 аргумента:
`(const ESM::Cell&, const ESM::Position&, const ESM::Cell&, const ESM::Position&)`.
Старый код в `ActionTeleport` продолжал передавать 5 аргументов через `CellId + CellName`.

Исправление переводит этот вызов на полные `ESM::Cell`, в соответствии с `aisequence`, `aitravel` и `aicombat`.
Логический harness X034 после исправления: ALL OK.

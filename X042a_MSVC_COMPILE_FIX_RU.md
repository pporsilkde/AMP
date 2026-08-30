# ArenaMP X042a — MSVC compile fix

Основа: X042 Quest Vocabulary Compile Safe.

Исправлены две ошибки из MSVC/Ninja build log:

1. `ProcessorGUIMessageBox.hpp(49): C2027 use of undefined type MWWorld::CellStore`
   - добавлен полный include `../../../mwworld/cellstore.hpp` перед использованием `actor.getCell()->getCell()->getDescription()`.
   - `MWWorld::Ptr` содержит только forward declaration CellStore, которого недостаточно для вызова методов.

2. `graphicspage.cpp(1206): C2143 missing ')' before ';'`
   - закрыт внешний вызов `Settings::Manager::setFloat("small feature culling pixel size", ...)`.
   - значения water preset сохранены без изменения: 32 / 28 / 20 / 18 / 14 / 10 px.

Все X042 Quest Vocabulary / Quest Studio / Combat AI / X040/X041 изменения сохранены.
Предупреждения D9025/C4458/C4267/C4189 не являются build blockers и этим hotfix не изменяются.

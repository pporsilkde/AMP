# ArenaMP X048 — Quest chat / Placement rotation / QuickLoot focus

Основа: X047.

## 1. Квесты больше не дублируют runtime-события в чат

Server Quest уже показывает состояние через обычный DialogueWindow и Quest Manager. Поэтому автоматические chat echoes теперь opt-in.

В `server/scripts/config.lua`:

```lua
config.serverQuests = {
    ...
    chatProgressMessages = false,
    chatRewardMessages = false,
    chatRuntimeErrors = false
}
```

По умолчанию отключены:
- `Started: <quest>`;
- переходы `quest -> stage` / complete / failed;
- reward action `type = "message"` в чате;
- runtime-ошибка auto-start линейного quest.

Явные ответы Moderator/Admin на `/quest ...` остаются в чате, потому что это не автоматический дубль, а подтверждение команды редактора.

Чтобы вернуть нужный класс сообщений, переключить соответствующий ключ в `true`.

## 2. Ready Weapon / Ready Magic реально вращают размещаемый предмет

Причина старого поведения: `keyboardmanager.cpp` жёстко съедал literal `R/F` до `BindingsManager`. На стандартных биндах Ready Weapon / Ready Magic не доходили до `ActionManager`, хотя сам ActionManager уже умел использовать эти actions для вращения.

X048 убирает hardcode R/F. Работают реальные пользовательские bindings, включая controller.

Во время placement:
- Ready Weapon: поворот по горизонтали;
- Ready Magic: поворот по вертикали;
- короткое нажатие: сразу шаг 15°;
- удержание: непрерывное плавное вращение;
- Run + Ready Weapon/Magic: то же вращение в обратном направлении;
- обычное доставание/убирание оружия и подготовка магии в placement не выполняются.

То есть для обычного вращения **Run не нужен**.

## 3. QuickLoot автоматически скрывается при размещении

Если placement был начат на контейнере, старый non-modal QuickLoot мог остаться висеть и продолжать перехватывать ввод.

X048:
- при появлении placement hint делает `QuickLoot::clear()`;
- пока `World::isPhysicsGrabActive()`:
  - QuickLoot не получает focus object;
  - не получает screen bounds;
  - `activateQuickLoot()` возвращает false;
  - колесо QuickLoot возвращает false;
  - клавиши QuickLoot возвращают false;
  - `isQuickLootVisible()` возвращает false;
- после finish/cancel placement QuickLoot снова работает штатно.

## 4. Колесо мыши меняет дистанцию предмета

Во время placement колесо мыши больше не попадает ни в QuickLoot, ни в обычный camera zoom.

- Wheel Up — приблизить предмет к игроку;
- Wheel Down — отдалить предмет;
- один шаг колеса меняет hold distance примерно на 18 world units;
- минимальная и максимальная дистанция автоматически ограничиваются с учётом размера объекта;
- событие колеса полностью consumed placement mode, поэтому зависший QuickLoot не может одновременно листаться.

Внутри подсказки placement добавлена строка `Колесо мыши — приблизить / отдалить предмет`.

## Проверка

1. Навести прицел на свободный предмет и удержать Activate примерно 1 секунду.
2. Placement hint должен появиться; QuickLoot, если был видим, должен исчезнуть.
3. Нажать Ready Weapon без Run — предмет сразу поворачивается по горизонтали на 15°.
4. Удерживать Ready Weapon — предмет продолжает вращаться.
5. Нажать/удерживать Ready Magic — вертикальное вращение.
6. С Run направление обратное.
7. Взять контейнер в placement — QuickLoot не должен висеть и принимать ввод.
8. Покрутить колесо: вверх — предмет ближе, вниз — дальше; камера и QuickLoot не должны реагировать.
9. Отпустить/отменить placement — QuickLoot снова доступен.
10. Пройти server quest: автоматические `[Quest] Started/stage/...` сообщения в чат по умолчанию отсутствуют; Dialogue/Quest Manager продолжают работать.

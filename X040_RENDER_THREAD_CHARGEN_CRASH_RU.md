# X040 — Краш клиента при регистрации / CharGen

## Диагноз

Падение воспроизводится на входе нового игрока, сразу после того как сервер
принял пароль регистрации и вызвал `SetCharGenStage(pid, 1, 5)`.

### Что говорят логи

`tes3mp-server.log` обрывается на:

    [03:39:12] Registration password accepted for 1 (1); starting character generation
    [03:39:27] Client at 127.0.0.1|61567 has lost connection

15 секунд тишины — это RakNet-таймаут, а не корректный дисконнект. Момент
падения к 03:39:27 отношения не имеет.

`MyGUI.log` показывает, что до падения CharGen успел полностью отработать:

    03:39:08  tes3mp_register.layout          карточка регистрации
    03:39:12  openmw_chargen_race.layout      RaceDialog #1 (внешность)
    03:39:14  openmw_chargen_race.layout      RaceDialog #2 (класс)
    03:39:15  openmw_chargen_race.layout      RaceDialog #3 (знак рождения)
    03:39:16  openmw_chargen_review.layout    Review

То есть регистрация и логика CharGen исправны. Падение — после Review.

### Что говорит дамп

    ExceptionCode  0xC0000005   ACCESS_VIOLATION
    Parameter[0]   8            попытка ИСПОЛНИТЬ неисполняемую память
    Parameter[1]   0x100000000
    Rip            0x100000000  прыжок по мусорному указателю на функцию

Стек упавшего потока 6616 — без единого кадра `tes3mp.exe`:

    OpenThreads.dll+0x3eff
    osg.dll+0x1b21f6
    osg.dll+0x123028          osg::OperationThread::run
    osgViewer.dll+0x3b6f0
    osgUtil.dll+0x1058d9
    osgUtil.dll+0xfb86f       RenderStage / SceneView draw
    ig9icd64.dll (x3)         драйвер Intel
    osgUtil.dll+0xeb541
    osg.dll+0x1f5998
    osg.dll+0x6836e
    >>> Rip = 0x100000000

Это поток отрисовки OSG. Мусорный vptr, виртуальный вызов, DEP.

## Корневая причина

Две независимые проблемы, которые в сумме дают детерминированный краш.

### 1. Настройка модели потоков OSG никогда не применялась

`apps/openmw/engine.cpp` содержит `getViewerThreadingModelSetting()` и
`getViewerThreadingModelName()` в анонимном namespace. Ни одна из них не
вызывалась. `setThreadingModel` во всём дереве встречается только в
`apps/opencs/view/render/scenewidget.cpp:149`.

При этом настройку пишет лаунчер (`graphicspage.cpp:1177`,
`advancedpage.cpp:642`) и документирует `settings-default.cfg:500`.

Итог: `settings.cfg` с `threading model = SingleThreaded` не давал ничего.
osgViewer оставался на `AutomaticSelection`, что для одного графического
контекста на многоядерной машине означает `DrawThreadPerContext`.

### 2. CharacterPreview рвёт свой граф прямо посреди кадра

`CharacterPreview::~CharacterPreview()` делал `mParent->removeChild(mCamera)` и
`mCamera->removeChildren(...)` синхронно, из главного потока, во время
`updateTraversal`.

Под `DrawThreadPerContext` поток отрисовки в этот момент ещё дописывает
предыдущий кадр, а его render graph держит голые указатели на `StateSet` и
`Drawable` внутри уничтожаемого поддерева. Виртуальный вызов по освобождённому
объекту — ровно наблюдаемая сигнатура.

CharGen делает это четыре раза за четыре секунды: три `RaceSelectionPreview`
(каждая новая страница создаёт новый `RaceDialog`, `closeRaceDialog()` убивает
предыдущий) плюс превью в `ReviewDialog`. На Intel UHD с медленной отрисовкой
окно гонки широкое, поэтому падение стабильное.

Класс проблемы в дереве уже был известен — см. комментарий в
`renderingmanager.cpp:404` про `CullDrawThreadPerContext` и `RenderStage`,
держащий предыдущий `DrawCallback`. Существующий механизм `mPendingOpenMode` в
`CharacterCreation::onFrame` лечил только порядок разрушения виджетов MyGUI на
главном потоке и на время жизни GL-объектов не влиял никак.

## Что изменено

### `apps/openmw/engine.cpp`

- Модель потоков наконец применяется к вьюверу, до `realize()`, с записью
  выбранного значения в лог. Заодно уходят предупреждения о неиспользуемых
  функциях.
- В цикле кадра, сразу после `mViewer->advance()`, вызывается
  `MWRender::collectRetiredCharacterPreviews(frameNumber)`.
- После `mViewer->stopThreading()` — принудительный слив очереди.

### `apps/openmw/mwrender/characterpreview.hpp` / `.cpp`

- Добавлена очередь отложенного освобождения: `RetiredPreview` держит
  `ref_ptr` на родителя, камеру, узел и анимацию.
- Деструктор больше ничего не разрушает. Он гасит `nodeMask` (безопасно: маску
  читает только cull, который в этом кадре ещё не отработал), снимает
  `DrawOnceCallback` и передаёт граф в очередь.
- Освобождение происходит только в `collectRetiredCharacterPreviews()`: между
  кадрами, вне traversal, с простоем потока отрисовки.
- Запас — 3 кадра. Cull кадра N идёт параллельно draw кадра N-1, а кэш
  GL-объектов освобождается ещё кадром позже; трёх хватает и для
  `DrawThreadPerContext`, и для `CullDrawThreadPerContext`.

## Проверка

`X040_preview_retire_harness.cpp` моделирует правило времени жизни без OSG:
последовательность «превью уничтожается почти каждый кадр» плюс принудительный
слив при выходе.

    g++ -std=c++17 -o harness X040_preview_retire_harness.cpp && ./harness

    preview 2 retired on frame 2, released on frame 5
    preview 3 retired on frame 3, released on frame 6
    preview 4 retired on frame 4, released on frame 7
    preview 5 retired on frame 5, released on frame 8
    forced drain released preview 99
    ALL OK

## После установки

`threading model` в `settings.cfg` можно вернуть на `DrawThreadPerContext` —
краш должен уйти и на нём. Если на `SingleThreaded` не падает, а на
`DrawThreadPerContext` падает, значит остался ещё один объект, разрушаемый
внутри кадра, и его надо искать отдельно.

В `tes3mp-client_log.log` при старте теперь должна быть строка:

    OSG viewer threading model: <значение>

## Не входит в X040

`Unhandled PlayerPacket with identifier 142` — это `ID_PLAYER_BASEINFO`
(134 + 8). Обрабатывается инлайном в `apps/openmw-mp/Networking.cpp:183` и
проваливается в `PlayerProcessor::Process`, где обработчика нет. На поведение
не влияет, лечится `return` в конце инлайн-ветки. Оставлено на X041, чтобы
X040 не смешивал краш-фикс с чисткой логов.

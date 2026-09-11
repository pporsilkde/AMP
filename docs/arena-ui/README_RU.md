# Arena Glass — встроенное оформление U011

U011 продолжает присланный U010. Все его исправления и проектные файлы сохранены.
Оформление компилируется в Qt EXE и Android APK. Внешнего QSS, скачиваемой темы,
Python-оболочки или отдельного skin-пакета не требуется.

## Где подключено

| Приложение | Изменение |
| --- | --- |
| PC Launcher | Общая тема, собственная верхняя панель, скругления, тени, SVG-вкладки и кнопки |
| PC Wizard | Общая тема и верхняя панель; содержимое визарда размещается ниже неё |
| PC Server dialog | Общая тема и верхняя панель, изменение размера через нижний угол |
| PC Updater | Та же тема и верхняя панель, золотой прогресс, сохранена кнопка подробностей |
| Android Launcher | Фон, скруглённая панель инструментов, векторные Play/Update/Globe, единая тема |
| Android Server | Общая палитра и стиль кнопок; общие диалоги |
| Android Updater / Changelog | Скруглённые диалоги, золотой прогресс, системный blur при поддержке |

ArenaMW, оптимизаторы и пакеры отсутствуют в присланном архиве: их проекты этим
патчем не изменены. Ниже описано подключение общего Qt-модуля к ним. Внутриигровой
MyGUI не менялся. Протокол ArenaMP не менялся.

## Материал и blur

Основа — угольный/обсидиановый материал, латунь, тёплое золото, пергаментный текст,
слабый холодный серо-синий отсвет. Это собственное оформление по мотивам macOS;
оптическая рефракция Apple Liquid Glass не реализована.

- Qt рисует материал, скругления и тень средствами QPainter. На Windows 11 22H2+
  выполняется запрос документированного DWM Acrylic через `DWMWA_SYSTEMBACKDROP_TYPE`.
  Windows-эффект здесь не проверялся на реальном композиторе; результат вызова API
  не гарантирует одинаковый вид на всех Qt/драйверах. Плотная заливка сохраняет
  читаемость. На Windows 10, Linux и macOS остаётся нарисованный матовый материал;
  размытия рабочего стола на этих системах этот патч не добавляет.
- Android: системный background blur диалогов запрашивается на API 31+.
  Тема делает окно translucent; при отсутствии blur фон становится непрозрачным.
  Слушатель изменения доступности blur снимается при отсоединении окна.
  API вызывается через reflection только на Android 12+, поэтому `compileSdk 29`
  и `minSdk 21` сохранены. Основной экран имеет градиентный фон, без постоянного blur.
- Статические SVG включены в QRC. Для Qt-сборок без SVG-плагина предусмотрены
  встроенные PNG-копии тех же иконок в разрешении 96×96. Android использует VectorDrawable.

Документация платформ:
[Qt translucent widgets](https://doc.qt.io/archives/qt-5.15/qwidget.html#creating-translucent-windows),
[Microsoft DWM attributes](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute),
[Android window blurs](https://source.android.com/docs/core/display/window-blurs).

## Обновление самого лаунчера

Обновление уже реализовано нативно в C++/Qt на PC и Kotlin/Java на Android.
`arena-updater.exe` остаётся отдельным процессом: после выхода лаунчера он заменяет
EXE и DLL, затем запускает лаунчер снова. Встроенная тема у обоих процессов одна.
`build.ini`, адреса обновлений, версии и журналы продолжают работать по правилам U010.

Закрытие PC updater во время загрузки отправляет запрос отмены. На этапе применения,
когда отмена запрещена, закрытие теперь игнорируется: прогресс остаётся видимым.
После завершения окно можно закрыть. Автоматическая проверка `check` остаётся без окна.

## Подключение к другим Qt-утилитам

Скопируйте `arenatheme.hpp`, `arenaglasswindow.hpp` в проект и подключите после
создания QApplication, до первого показа главного окна:

```cpp
ArenaUi::applyMorrowindGlassPalette(app);
MainWindow window;
ArenaUi::installGlassWindow(window);
window.show();
```

Оба модуля требуют Qt Widgets и не требуют moc. Не накладывайте поверх них другой
глобальный `app.setStyleSheet`, иначе он заменит общую тему. Собственная верхняя
панель резервирует 52px сверху и 12px по краям; для фиксированного окна предусмотрите
это место в размере. Перемещение: верхняя панель; изменение размера: нижний правый
угол. Для Qt 5.15+ используется системное перемещение, для более старых — Qt fallback.

Для иконок добавьте `arenaglassicons.hpp`, `arenaicons.qrc` и каталог `arenaicons/`:

```cmake
qt5_add_resources(ARENA_ICONS path/to/arenaicons.qrc)
target_sources(your-tool PRIVATE ${ARENA_ICONS})
```

```cpp
button->setIcon(ArenaUi::glassIcon(QStringLiteral("update")));
button->setProperty("arenaPrimary", true); // до показа/полировки кнопки
```

## Проверка U011

В Linux-контейнере с Qt 5.15.13 выполнены:

- Компиляция настоящего `arena_updater.cpp` вместе с новым оформлением в standalone-тесте.
- `arena-glass-smoke`: native updater self-test; видимость прогресса; запрос отмены;
  запрет скрытия во время применения; отсутствие повторного отступа при двойном
  подключении темы; загрузка иконок; размещение QMainWindow и QWizard.
- Имеющийся `updater-ui`: жизненный цикл окна/контроллера обновлений.
- Разбор XML/SVG/QRC и проверка наличия всех QRC-файлов.
- Визуальный просмотр PNG окна обновлятора и демонстрационных окон на тех же компонентах.

`preview/` в корне архива содержит настоящий ProgressWindow обновлятора и
**демонстрации компонентов** Launcher/Wizard; это не скриншоты полностью собранного движка.

Полная сборка ArenaMP/Windows/APK и проверка Android на устройстве не выполнялись:
U010 — кумулятив изменённых файлов, а не полный репозиторий. Для проверки после
переноса пересоберите PC и оба Android flavor, откройте Changelog, обновление и
Server; проверьте вид на устройстве с blur и без него. Исправления серверного runtime,
AssetUpdater, ServerController и ArenaServerService из U010 оставлены побайтно.

Команды standalone-проверок из корня MP:

```sh
cmake -S components/misc/arena-ui-tests -B build-glass
cmake --build build-glass
ctest --test-dir build-glass --output-on-failure
cmake -S apps/launcher/updater/qt-tests -B build-updater-ui
cmake --build build-updater-ui
ctest --test-dir build-updater-ui --output-on-failure
```

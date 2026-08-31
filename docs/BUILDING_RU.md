# Сборка ArenaMP

Эталон релизной сборки — GitHub Actions workflow. Сейчас он создаёт пакеты Windows x64, Linux x64/Steam Deck и экспериментальный macOS ARM64.

## Получение исходников

```bash
git clone --recurse-submodules https://github.com/MrZer0x0/TES3MP.git
cd TES3MP
git submodule sync --recursive
git submodule update --init --recursive
bash CI/ensure-bundled-deps.sh
```

До конфигурации `extern/raknet` должен содержать TES3MP/CrabNet. Скрипт добавляет зависимость, если каталог отсутствует или неполон.

После смены компилятора, архитектуры, набора зависимостей или основных опций CMake создавайте чистый build-каталог.

## Linux x64 / Steam Deck

Workflow использует Ubuntu 22.04 и Ninja:

```bash
bash CI/linux/install-deps.sh
bash CI/linux/configure.sh
bash CI/linux/build-package.sh
```

Готовый переносимый архив появится в `artifacts/`.

## macOS ARM64

Используются Apple Silicon runner и зависимости Homebrew:

```bash
bash CI/macos-arm64/install-deps.sh
bash CI/macos-arm64/configure.sh
bash CI/macos-arm64/build-package.sh
```

Цель экспериментальная. Скрипты проверяют зависимости bundle и создают DMG либо резервный архив.

## Windows x64

Workflow использует Visual Studio 2022, Ninja и `CI/before_script.msvc.sh`. Точное окружение зафиксировано в `.github/workflows/release-builds.yml`.

Запуск из совместимой с Visual Studio/MSYS оболочки:

```bash
bash CI/before_script.msvc.sh -c Release -p x64 -v 2022 -N -V -i "$PWD/install"
source MSVC2022_64_Ninja/activate_msvc.sh
cmake --build MSVC2022_64_Ninja --config Release --parallel 2
cmake --install MSVC2022_64_Ninja --config Release
```

Не используйте старый build-каталог после смены генератора, архитектуры или зависимостей. Инкрементальный кэш безопасен только для совместимой ревизии исходников.

## Основные опции CMake

| Опция | По умолчанию | Назначение |
|---|---:|---|
| `BUILD_OPENMW` | `ON` | Клиент ArenaMP/TES3MP |
| `BUILD_OPENMW_MP` | `ON` | Выделенный сервер |
| `BUILD_LAUNCHER` | `ON` | Лаунчер |
| `BUILD_BROWSER` | `ON` | Браузер серверов |
| `BUILD_MASTER` | `ON` | Master server |
| `BUILD_OPENCS` | `OFF` | OpenMW-CS |
| `BUILD_UNITTESTS` | `OFF` | Модульные тесты |
| `OPENMW_LTO_BUILD` | `OFF` | LTO при поддержке toolchain |
| `OPENMW_GL4ES_MANUAL_INIT` | `OFF` | Ручная инициализация gl4es для специальных платформ |

## Проверка перед публикацией

1. Выполнить конфигурацию в чистом каталоге.
2. Собрать клиент, сервер, лаунчер и браузер.
3. Запустить `bash CI/check-lua-api-bindings.sh` и `bash CI/check-server-core.sh`.
4. Проверить финальный install tree, а не только build tree.
5. Подключить к серверу минимум два клиента той же ревизии.
6. Проверить вход/CharGen, переходы между ячейками, группы, чат, бой, инвентарь, квесты, выход и рестарт сервера.
7. Публиковать ровно ту ревизию исходников, из которой получен пакет.

Android-скрипты сохранены в `CI/` для разработки, но Android пока не является релизной целью.

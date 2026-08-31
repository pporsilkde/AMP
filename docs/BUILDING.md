# Building ArenaMP

The GitHub Actions workflow is the canonical description of release builds. It currently produces Windows x64, Linux x64/Steam Deck, and experimental macOS ARM64 artifacts.

## Source checkout

```bash
git clone --recurse-submodules https://github.com/MrZer0x0/TES3MP.git
cd TES3MP
git submodule sync --recursive
git submodule update --init --recursive
bash CI/ensure-bundled-deps.sh
```

`extern/raknet` must contain the bundled TES3MP/CrabNet source before configuration. The helper retrieves it when the directory is missing or incomplete.

Use a clean build directory after switching compiler, dependency set, architecture, or major CMake options.

## Linux x64 / Steam Deck

The release workflow uses Ubuntu 22.04 and Ninja:

```bash
bash CI/linux/install-deps.sh
bash CI/linux/configure.sh
bash CI/linux/build-package.sh
```

The packaging script creates a portable archive under `artifacts/`.

## macOS ARM64

The macOS job uses an Apple Silicon runner and Homebrew dependencies:

```bash
bash CI/macos-arm64/install-deps.sh
bash CI/macos-arm64/configure.sh
bash CI/macos-arm64/build-package.sh
```

This target is experimental. The packaging scripts perform bundle dependency checks and produce a DMG or fallback archive.

## Windows x64

The release workflow uses Visual Studio 2022, Ninja, and `CI/before_script.msvc.sh`. Its exact environment setup is defined in `.github/workflows/release-builds.yml`.

Run from a Visual Studio/MSYS-compatible shell:

```bash
bash CI/before_script.msvc.sh -c Release -p x64 -v 2022 -N -V -i "$PWD/install"
source MSVC2022_64_Ninja/activate_msvc.sh
cmake --build MSVC2022_64_Ninja --config Release --parallel 2
cmake --install MSVC2022_64_Ninja --config Release
```

Remove the old build directory before changing generator, architecture, or dependency configuration. Incremental reuse is supported only when the cached source revision is compatible.

## Main CMake switches

| Option | Default | Purpose |
|---|---:|---|
| `BUILD_OPENMW` | `ON` | Build the ArenaMP/TES3MP client |
| `BUILD_OPENMW_MP` | `ON` | Build the dedicated server |
| `BUILD_LAUNCHER` | `ON` | Build the launcher |
| `BUILD_BROWSER` | `ON` | Build the server browser |
| `BUILD_MASTER` | `ON` | Build the master server |
| `BUILD_OPENCS` | `OFF` | Build OpenMW-CS |
| `BUILD_UNITTESTS` | `OFF` | Build unit tests |
| `OPENMW_LTO_BUILD` | `OFF` | Enable LTO when the toolchain supports it |
| `OPENMW_GL4ES_MANUAL_INIT` | `OFF` | Manual gl4es initialization for special platforms |

## Validation before publishing

At minimum:

1. configure from a clean directory;
2. build client, dedicated server, launcher, and browser;
3. run `bash CI/check-lua-api-bindings.sh` and `bash CI/check-server-core.sh`;
4. inspect the final install tree, not only the build tree;
5. start a server and connect at least two matching clients;
6. test login/CharGen, cell transitions, groups, chat, combat, inventory, quests, disconnect, and server restart;
7. publish the exact source revision used to build the package.

Android scripts remain in `CI/` for development history, but Android is not a current release target.

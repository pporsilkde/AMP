# Contributing to ArenaMP

ArenaMP changes networking, server scripts, gameplay rules, UI, and rendering in one codebase. Keep contributions focused and document their compatibility impact.

## Before starting

- Search existing issues and recent changes.
- Discuss large protocol, save-format, server-data, or architecture changes before implementation.
- Confirm whether the change belongs in the engine, bundled CoreScripts, optional content, or documentation.
- Do not silently copy code or assets with an incompatible license.

## Pull requests

Include:

1. the problem and intended behavior;
2. a concise summary of the implementation;
3. every affected platform and subsystem;
4. protocol, save/data migration, and script API impact;
5. exact build and test results;
6. screenshots for visible UI/rendering changes;
7. rollback or compatibility notes when server data changes.

Keep unrelated fixes in separate pull requests. Preserve the existing style and avoid broad formatting-only rewrites.

## Compatibility rules

- Never claim stock TES3MP network compatibility without verifying protocol number and packet layout.
- Bump or explicitly migrate protocol/save/data formats when a compatible read path is impossible.
- Treat the server as authoritative for security-sensitive classification, rewards, progression, groups, and world state.
- New player-visible strings must be added to both `files/vfs/l10n/arenamp/en.ini` and `ru.ini`.
- Server messages must use the localization layer instead of hardcoded bilingual branches where practical.
- Preserve existing player/world data or provide a documented migration and backup path.

## Minimum validation

- Clean configure and build for every affected target.
- Run `bash CI/check-lua-api-bindings.sh` and `bash CI/check-server-core.sh` for native/Lua changes.
- Validate modified JSON and XML/layout files.
- Test one server with at least two matching clients for multiplayer behavior.
- Test reconnect, authority transfer, cell transition, normal exit, and server restart when relevant.
- State clearly when a target was not compiled or a scenario was not tested.

See [Building ArenaMP](docs/BUILDING.md) for the release build path.

## Bug reports

Include the ArenaMP source revision, OS, CPU/GPU, client and server logs, content-file order, reproduction steps, expected result, and whether the problem reproduces with a clean profile. Crash dumps may contain paths or account names; review them before public upload.

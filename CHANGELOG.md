# Changelog

This changelog consolidates the ArenaMP development notes that previously existed as separate patch, revert, manifest, and validation files. Internal patch identifiers are retained so older builds and reports can still be mapped to the current source.

## Unreleased — source distribution cleanup

### Added

- English landing page with a direct Russian-language entry point.
- Dedicated EN/RU build and TES3MP comparison documents.
- Consolidated feature overview, credits, third-party notices, security policy, and EncoreMP guide.
- Explicit client/server compatibility matrix.

### Changed

- Reorganized EncoreMP content under `extras/encoremp/`.
- Moved retained TES3MP history under `docs/upstream/`.
- Replaced the inherited OpenMW contribution guide with ArenaMP-specific instructions.
- Corrected documentation from EncoreMP 0.92 filenames to the included 0.93 files.
- Corrected the protocol description: ArenaMP protocol 806 is not stock TES3MP 0.8.1 protocol 10.

### Removed

- Nested `AMP.zip`, obsolete cumulative/revert patches, one-off changed-file lists, old checksums, and temporary validation reports.
- Patch harnesses tied to deleted `/mnt/data/...` workspaces or superseded UI behavior.
- Legacy Travis, GitLab CI, AppVeyor, and inherited funding metadata; GitHub Actions remains the release workflow.
- An unreferenced, divergent shadow copy at `cmake/CMakeLists.txt`; the repository now has one authoritative root build definition.
- The unfinished Home tab placeholder from the Player Menu.
- Uncompiled KTX2 loader/converter files that were not referenced by CMake or runtime code. KTX2 is therefore not advertised as a current ArenaMP feature.

## X057 — core logging, data bootstrap, summons, and parties

### Added

- Shared `server/scripts/ampCore.lua` helpers for normalized `[ArenaMP Core]` logging and guarded JSON creation.
- Automatic creation of registered JSON data files when they are genuinely absent.
- Client/server handling for summon ownership cleanup and party-aware mechanics.

### Changed

- Consolidated legacy server log tags without changing player-visible chat channel tags.
- Group membership now mirrors into native ally lists used by friendly-fire mode `group`.
- Summon and disconnect cleanup avoids depending solely on a delayed global sweep.

### Fixed

- Missing JSON files no longer produce avoidable read errors before defaults are created.
- Party members are correctly protected by group friendly-fire rules.
- Group/allies state is refreshed across login, invite, leave, kick, disband, and disconnect paths.

## X056 — group invitation flow

- Added localized invitation popups and explicit accept/decline actions.
- Added invite-state refresh and group UI synchronization.
- Preserved the command path for compatibility with server-side group logic.

## X055 — Player Menu roster and fast load

- Added the full server player list to the Player Menu.
- Improved panel width, player details, invite shortcuts, and initial state loading.
- Reused the authoritative `/list` formatting instead of maintaining a second reduced roster.

## X054 — unified chat and quest localization

- Unified Player Menu controls with the existing `coreChat` channel/state model.
- Added localized server-quest dialogue, journal, validation, rewards, and runtime feedback.
- Expanded chat channels and action styles; removed duplicate color/state implementations.
- Added EN/RU Quest Studio UI strings and UTF-8-safe chat handling.

## X053–X053b — color emoji and MyGUI compatibility

- Added the optional full-color OpenMoji chat atlas and fallback-safe emoji palette.
- Added chat font resource loading and MyGUI 3.2-compatible font handling.
- Fixed MyGUI 3.2 chat compilation and preserved text fallback when emoji glyphs are unavailable.

## X052 — Player Menu UX

- Added adaptive caption-aware layouts for EN/RU text.
- Added online rosters, group member state, player details, and nickname color controls.
- Improved geometry persistence, resizing, and high-resolution behavior.

## X051 — restart position safety

- Added server visibility of player position packets and continuous safe transform caching.
- Avoided sampling a peer during disconnect teardown.
- Added restart/login collision protection to reduce falling through terrain or unloaded geometry.

## X050–X050d — integrated groups and chat stability

- Added persistent groups, leader actions, invitations, same-cell XP sharing, and optional journal/topic synchronization.
- Added the group page to Player Menu and shared helper APIs.
- Refactored Arena helper ownership and removed duplicate notification paths.
- Fixed MyGUI TextBox compatibility and HUD/chat input-mode focus handling.

## X049 — Player Menu chat

- Introduced the expandable Player Menu shell while preserving lightweight HUD chat.
- Added structured chat controls, draggable/resizable geometry, and a return-to-game action.

## X048 — quest interaction and Quick Loot integration

- Added better server-quest requirement feedback, journal injection, and deferred inventory/cell requests.
- Integrated Quick Loot placement and input behavior with the quest/player interface.
- Added built-in example quests and owner-console safeguards.

## X047 — just-in-time quest synchronization

- Re-sent visible server-quest topics/dialogue at interaction time.
- Reduced stale topic state after relog, delayed loading, and cell transitions.

## X046 — quest-index persistence and login safety

- Reused valid persistent quest indexes instead of rebuilding them on every start.
- Added guards for mixed binary/script revisions during login.
- Fixed a login crash path caused by unavailable or incomplete quest-index state.

## X045 — compact localized Quest Studio

- Reworked Quest Studio into a compact multi-tab interface.
- Added EN/RU labels, help, validation output, and caption-safe sizing.

## X044 — idle AI movement

- Preserved idle/wander packages across actor authority handoffs.
- Prevented idle actors from emitting unnecessary AI heartbeats or clearing queued movement.
- Restored original record AI when a client becomes authority.

## X043–X043a — quest instances and topic selection

- Added instance-aware quest givers and personal dynamic-interior handling.
- Added safe topic tokens for translated game data.
- Fixed server-quest topic clicks without treating transport tokens as ordinary Morrowind topics.

## X042–X042a — quest vocabulary and compile safety

- Expanded requirements, rewards, conditions, and safe boolean composition.
- Restricted destructive vanilla/world actions behind explicit policy.
- Fixed MSVC compilation in graphics/settings expressions.

## X041 — launcher layout

- Consolidated Arena settings into the launcher's active pages.
- Moved XP and render/streaming controls out of the removed placeholder settings tab.

## X040 — character-preview render safety

- Fixed render-thread lifetime handling for retired CharGen and inventory render-to-texture previews.
- Prevented stale preview objects from surviving engine teardown.

## X039 — MyGUI Quest Studio

- Added a native staff-only Quest Studio window backed by server-authoritative JSON definitions.
- Added editor model transport, validation, draft/publish workflow, and in-game giver selection.

## X038 — server quest editor v2

- Expanded server-side editing, clone/delete/disable behavior, and regression validation.
- Kept definitions authoritative on the server.

## X037 — quest choices and journal fixes

- Fixed C++ integration for quest choices and journal projection.
- Added independent synchronization of started/completed server quests.

## X036 — quest dialogue integration

- Connected server-authored quests to NPC dialogue and hidden reliable GUI transport.
- Added the first integrated Caius example quest and registry support.

## X035 — server quest core

- Introduced JSON quest definitions, server-side progression, requirements, rewards, and persistence.
- Added staff commands and initial examples.

## X034–X034c — combat AI state synchronization

- Synchronized full combat target sets, suspended pursuit state, and authority ownership.
- Preserved combat through valid door transitions and prevented stale/non-authority mutation.
- Added compile fixes for teleport actions and legacy compiler constraints.

## X033 — water and chat presets

- Added launcher water presets and synchronized full water controls.
- Added chat presentation settings without coupling them to graphics presets.

## X032 — preset separation

- Separated gameplay/server presets from graphics presets.
- Added hardware-safe fallback behavior and migration from the earlier combined profile.

## X031 — live configuration and occlusion controls

- Added transactional hot reload for supported raw `config.lua` values.
- Added conservative live occlusion budgets, caching, diagnostics, and launcher integration.

## X030 — launcher presets

- Added named gameplay/server and quality profiles.
- Added validation and safe default restoration for launcher-controlled settings.

## X029 — software occlusion culling

- Added CPU hierarchical depth-buffer occlusion testing and terrain occluders.
- Added conservative bounds, budgets, cache reuse, and debug statistics.

## X028 — rendering optimization foundation

- Established the staged render/scene optimization work used by later occlusion patches.
- Added initial MGE-inspired settings and safety limits.

## X027 — dialogue and combat-bar positioning

- Improved NPC turning during dialogue, docking distance, and presentation transitions.

## X026–X026a — exit stability and combat-bar tuning

- Reordered multiplayer teardown before engine-environment cleanup.
- Fixed reference cleanup during normal client exit.
- Tuned compact head bars and combat presentation.

## X025 — docked combat bars

- Added distance-aware docking, overlap control, names, and compact presentation for multiple combatants.

## X024 — actor synchronization and health bars

- Improved actor authority synchronization, return-home anchors, combat state, and health-bar stability.
- Preserved the slower, natural NPC dialogue-pose timer.

## X023 — dialogue pose timing

- Stopped topic clicks and voice-line changes from restarting NPC poses.
- Kept pose changes on a natural randomized 30–60 second timer.

## X022 — multiplayer stability and authority

- Fixed normal-exit crashes and invalid transitional cell coordinates.
- Stabilized cell authority and actor recovery across cells.
- Added player scale limits and combat-return safeguards.

## X021 — combat-bar review

- Corrected ally/enemy skin reuse, off-screen visibility, pool behavior, and distance fading.

## X018–X020 — quest-index bootstrap

- Published the native QuestIndex API to Lua.
- Added automatic generation, validation, hashing, persistence, and safe bootstrap defaults.
- Kept phasing disabled until a valid authoritative index exists.

## X012–X015 — per-player quest items

- Added personal world/container claims for authored quest sources.
- Moved classification ownership to the server and treated clients only as index data providers.
- Added packet/API integration and fail-closed guards for mixed revisions.

## Foundation through X011

Earlier work established the modern CharGen, inventory/interface changes, object placement, lighting and scene updates, native progression/gameplay ports, expanded settings, and the ArenaMP server-core integration that later milestones build upon.

## Upstream history

ArenaMP does not duplicate OpenMW's full historical changelog in the repository root. See the [OpenMW 0.47.0 release notes](https://openmw.org/2021/openmw-0-47-0-released/), retained [OpenMW Stage 1 design document](docs/upstream/OPENMW_STAGE1.md), and retained [TES3MP changelog](docs/upstream/TES3MP_CHANGELOG.md).

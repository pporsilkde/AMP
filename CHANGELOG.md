# Changelog

## Y015 — custom quest chronology / journal title fix

- Server-authored/custom journal entries no longer render after every vanilla JOUR entry unconditionally. Each new custom entry stores a persistent `vanillaAnchor` equal to the number of ordinary journal entries that existed when the stage was written, and the client merges both streams around that anchor.
- A vanilla quest completed after a custom quest therefore becomes newer in both the classic Journal and Quest Manager instead of the custom quest remaining permanently last/newest.
- Quest Manager no longer uses the artificial `serverOrder = 1000000`; pinned quests stay first, then actual recency, with completion state only as a tie-breaker.
- Custom journal entry headers now show the localized quest name together with the recorded date instead of a bare date.
- Existing server-quest saves without chronology metadata are migrated once to a conservative anchor immediately before the newest vanilla entry, preventing legacy custom quests from staying forced to the end forever.
- The server transport adds optional chronology fields to the existing text transport only; ArenaMP network protocol remains 806.

## Y014 — FIX_01 integration / default quest-topic sync / class-weighted SP

- Integrated Arena_Y013_FIX_01: RP chat mode is mirrored server-side; private-cell helpers are guarded; faction-name heuristic stays off; placement rollback survives release; deny text is rate-limited; actor-cell quicksaves are throttled; remote grounded recovery no longer breaks real falling.
- Server-wide journal and dialogue-topic sharing now defaults to enabled (`shareJournal=true`, `shareTopics=true`).
- New group member preferences also default to journal/topic synchronization enabled; explicit saved per-member choices are preserved.
- XP skill purchases keep the existing base 1/2/3/4 SP curve, multiplied by class importance: Major ×1, Minor ×2, Misc ×3. Tooltip/button and charged cost use the same calculation.
- Protocol remains 806; no new packet format is introduced.


## Y013 — HUD dedupe / remote locomotion / RP placement / reliable NPC return

- Legacy centered added-item/harvest MessageBoxes are suppressed when the right HUD item feed already reports the inventory delta.
- DedicatedPlayer self-heals stale airborne state when an observer enters a cell, preventing tucked-leg sliding until the remote player jumps.
- ObjectMove/ObjectRotate are server-authoritative. Moderator+ may decorate anywhere; regular players must be in RP mode and inside their own private/house interior or an allowed faction interior. Denied grabs are rolled back client-side and never relayed.
- Accepted object transforms are stored in cell state and routed only to the loaded-cell interest set.
- ActorCellChange seeds the destination cache before source removal; Lua also guarantees destination actorList membership and transition coordinates, closing exterior->interior NPC disappearance races.
- Protocol remains 806.


## Y012 — shader-water ripple isolation + server-authoritative XP penalties

- Legacy osgParticle movement rings are disabled whenever shader water is active; disabling shader ripples no longer re-enables the old effect.
- XP cards are iconless and localized. Known MP event reasons are localized client-side; gains/losses/neutral status use green/red/black presentation.
- Server death handling wipes all current-level XP and persists it immediately without touching levels, Skill Points or skills.
- If current XP was already zero, the server adds current level × 5 seconds to the ordinary respawn timer; otherwise XP is wiped and no extra level cooldown is added.
- XP-mode jail wipes current-level XP and suppresses random skill changes. Protocol remains 806.

This changelog consolidates the ArenaMP development notes that previously existed as separate patch, revert, manifest, and validation files. Internal patch identifiers are retained so older builds and reports can still be mapped to the current source.


## Y011 — unified HUD notifications + chat input readability

### Changed
- Replaced the three-band Y010 event-card shadow with one uniform medium-opacity `BlackBG` backing (`0.22` alpha), keeping the borderless right-edge presentation.
- Arena XP gameplay rewards now use the same right-side event feed instead of transient MessageBox text. Rewards from the same reason coalesce while the card is alive and preserve fractional XP.
- XP level/system notifications use the shared feed during gameplay; XP feedback triggered while a GUI is open keeps the original MessageBox path so it cannot expire behind menus. Vanilla/scripted game MessageBoxes are otherwise untouched.
- ArenaMP chat input text is now 14 px and pure white; chat history, nickname colours and channel/style formatting are unchanged.
- Network protocol remains 806.

## Y010 — HUD event-feed visual polish

- Moved event cards to a 2 px right-edge margin instead of inheriting the stamina bar's horizontal inset.
- Removed the framed card skin and introduced the borderless backing later simplified by Y011.
- Pickup and gold cards show `+delta`, appending the committed total as `+5 (10)` when the stack already existed.


## Y009 — HUD/event-feed stability + actor packet isolation

### Fixed
- Ported the render-time pooled combat-bar Track reassertion to ArenaMW parity; ArenaMP keeps the same verified-HP-before-visible rule.
- Removed the unnecessary fake `range=1` kick: MyGUI 3.2.2 setters already call `updateTrack()` unconditionally, so real HP is simply reasserted at the first valid visible frame.
- Magic-effect notification durations now count down live and track the exact stacked ActiveSpells instance by id, caster and timestamp.
- Server `InventoryChanges::SET` is detected exactly in ArenaMP and reseeds the pickup snapshot without false loot notifications.
- Generic clear/refill detection now keys off a large loss of existing inventory kinds instead of suppressing any pickup batch larger than four kinds, so normal Take All remains visible.
- HUD event-feed fallback coordinates are now relative to `mGameplayHud`.

### Multiplayer correctness
- `ActorPacket` now constructs a fresh `BaseActor` for every entry in a received actor batch, preventing conditional packet fields from leaking between adjacent actors.
- Receive-side `BaseActor` and packet helper POD fields now have deterministic defaults.
- Network protocol remains 806; no wire-format fields were added.

### Reviewed but intentionally not included
- The proposed `DedicatedActor` current-modifier preservation was rejected: in this OpenMW branch `DynamicStat::readState()` already overwrites the current-modified value from `ESM::StatState::mMod`, so pre-copying the local stat does not preserve it.
- The one-second `AiCombat` settings cache and `Cell.cpp` allocation optimizations are valid performance ideas but are unrelated to this correctness/HUD cumulative and are deferred to a separate performance patch.

## Y007 — HUD event feed

### Added
- Added a fixed six-slot RPG-style HUD event feed above the stamina/combat-bar stack.
- Positive player-inventory deltas now show the real item icon, localized item name and gained quantity.
- Gold gains are aggregated into one live card, so rapid rewards/pickups do not spam the HUD.
- Newly applied lasting spells/potion effects show their first magic-effect icon and remaining duration.
- The feed repositions above close-range docked NPC health bars instead of overlapping them.

### Multiplayer safety
- Notifications are derived only from committed local player state. ArenaMP therefore shows inventory changes only after the corresponding server-confirmed state has been applied; no new packet or protocol change is required.
- Network protocol remains 806.
- Y006 pooled combat-health-bar Track reassertion remains included.

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

## Y006 — combat-bar Track reset at render time

- Keeps the Y005 diagnosis (pooled red/green MyGUI ProgressBar widgets) but removes the assumption that real HP is available in the same scan frame as owner/skin reassignment.
- Added a per-slot `mNeedsTrackReset` flag set on owner reuse and enemy/ally skin changes.
- Defers the forced alternate range until a frame has verified current/max health and the widget is about to become visible.
- A slot waiting for that verified health remains hidden instead of exposing an empty frame.
- Re-asserts the real 0..1000 range and current progress on every visible resolved frame; the forced alternate range is paid only after owner/skin invalidation.
- No ActorStats, authority, packet or server routing changes; network protocol remains ArenaMP 806.

## Y005 — riding rollback and multiplayer combat-bar fill stability

- Rolled back the experimental Y002-Y004 riding subsystem; Y005 is based on the stable Y001 gameplay tree.
- Fixed pooled combat health bars occasionally rendering only their frame after a red/green skin transition.
- Re-prime MyGUI ProgressBar range/position whenever a pooled slot changes owner or switches enemy/ally skin, so the internal Track is always rebuilt.
- No network protocol change; protocol remains ArenaMP 806.

## Y001 — launcher quality persistence and HUD FPS counter

### Fixed

- Restored manual Graphics/Quality-page persistence for Water, Terrain, PBR, lighting, shadows, display and FPS-limit controls.
- Graphics changes are merged into the latest on-disk `settings.cfg` instead of blindly rewriting the launcher's stale in-memory copy, preserving unrelated settings changed by the running game.
- Prevented the later XP/server-settings save pass from discarding graphics changes made immediately before launching the client.
- Made the dedicated HUD FPS counter explicit (`FPS: N`), enabled it once for upgrades through a Y001 migration, and kept subsequent HUD on/off choices persistent.
- Kept F3 assigned to ArenaMP HDR; stale EN/RU text that described F3 as an FPS hotkey was corrected.

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

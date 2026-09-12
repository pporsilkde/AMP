## U023 — Graphics preset persistence, revive fallback and Data Files cleanup

### Fixed
- Auto graphics detection now applies its effective preset to the active `settings.cfg` instead of only updating the recommendation label. Segmented manual preset buttons apply immediately, and the existing Apply button uses the same authoritative path.
- Switching builds re-applies the persisted Auto/manual profile after `Settings::Manager` is rebound to the selected build, preventing stale graphics values from the previous build.
- Every preset writes the required ArenaMP `[Shaders]` baseline for normal/specular maps, Enhanced PBR, shader-compatible lighting, balanced materials, HDR and Bloom.
- Completed Russian coverage for the redesigned Graphics page while retaining the original English strings.
- Data Files now hides only the obsolete Content List profile strip (combo + New/Clone/Delete); the plug-in manager remains fully available.
- Death recovery preserves `nirnRestoreHealth` validation and adds a narrow fallback for a client-confirmed healing potion whose record is unavailable to the server: consume the required potion(s) and revive at 5% base health. Server-known non-healing records are still rejected and normal recognized Restore Health potions keep the 25% path.

### Validation
- Lua syntax: PASS. Recovery harness: unknown record 5%, known non-healing reject, known generated heal 25%, preclassified `nirnRestoreHealth` heal 25%.
- Data Files UI XML and U023 static graphics checks: PASS.

## U022 — Four sections, Settings replaces Advanced, spin-box steppers and Help link

### Changed
- Top navigation is four sections: Play, Data Files, Graphics, Settings.
- The old OpenMW Settings page (Morrowind.ini importer, wizard shortcuts) is no longer reachable; the compact card page that used to be "Advanced" is the Settings page now. `SettingsPage` is still constructed so its load/save keeps owning the same launcher.cfg values.
- The two ArenaMP client options from that page ("Connect to vanilla-build server", "Hide chat messages") are moved into the Play page launch card.
- Help opens https://t.me/arena_mp until a bundled manual exists.
- QSpinBox/QDoubleSpinBox steppers are styled as two separate keys with ArenaMP chevrons, hover/pressed feedback and muted arrows at the range limits (new chevron-up and muted chevron resources).

## U021 — Hero launch action, server-settings list, minimal Advanced, built-in build setup

### Changed
- `ArenaUi::HeroButton`: a self-painted primary action with a soft pulsing glow. The shared QSS `min-height` rule used to override the .ui height and collapse the "Start game" button to ~22 px; the widget now reports its own size. The footer Play/Update button uses the compact variant and pulses stronger when an update is waiting.
- Window controls moved to the right in Windows order (minimize, close); the green maximize control is gone and the title is left-aligned.
- Server settings: a vertical category list (Overview, Players, Combat / NPC, Progress, Network, Raw config.lua) drives the existing form/raw state machine. Both tab bars are hidden, the manual form<->raw buttons are gone (switching synchronizes automatically) and value fields no longer stretch across the page.
- Advanced is now a short card page: Visuals, Gameplay, Sound, Interface, Performance. Normal/specular maps, sheathing and modern movement are single switches in front of the original settings; everything else stays behind "All engine parameters".
- The setup wizard is built into the launcher: a first run opens a build-folder dialog that either loads an existing build.ini or generates one from the chosen name, language and load order. `arenamp-wizard` is no longer spawned, and the Play page can switch builds at any time.
- Data Files keeps the content list only: grass/groundcover plug-ins are detected by name and connected automatically, and the legacy profile selector is hidden.

- New builds choose their language (Russian/English/Polish, preselected from the system locale). It is stored in build.ini and decides the text encoding (win1251/win1250/win1252); an existing build.ini without a language field can be completed the same way without rewriting the file.
- `BUILD_WIZARD` now defaults to OFF: the standalone wizard executable is not built, packaged or validated any more, and the Windows installer finishes into the launcher.

### Notes
- The macOS `CI/macos-arm64/configure.sh` is not part of this archive: switch its `-DBUILD_WIZARD=ON` / `require_cache_exact 'BUILD_WIZARD:BOOL=ON'` to OFF to match the workflow guards.
- `settingspage.*`, `advancedpage.ui` and `datafilespage.*` are not part of this cumulative archive, so the Advanced quick panel and the Data Files cleanup work through the existing widget pointers/object names at runtime.

## U020 — Showcase Play page, system status column and requiredDataFiles.json protection

### Changed
- Launcher window is now 1080×720 (still fits 1366×768 laptops with the taskbar).
- Play page rebuilt: a left glass panel with Connection / Server Console / Server Settings pills, a **Start the game** card and a **Local server** card side by side, and a permanent **System status** column.
- Removed duplicated controls: host mode, bind interface and alternative server live only in the launch card; gameplay preset, hashes, website, auto-restart, DataFiles enforcement, clear/reset and Start/Stop live only in the server card. The server card no longer empties itself when Host mode is off.
- The hero **Start game** button shows the actual target (local server + port, alternative server, selected server) and switches to update/checking hints.
- System status shows launcher state (ready / checking / update available / server running), detected GPU and VRAM, CPU threads, mode, bind interface or target server, build and preset, plus a contextual tip.
- Footer: status dot with two-line server state, server session uptime and fixed action order Update/Play · Changelog · Run Server · Help.
- New Arena Glass icons and Russian strings for all new UI text.

### Fixed
- Desktop engine updates now explicitly skip `requiredDataFiles.json` regardless of its path inside the engine archive (in addition to the protected `server/` root). Self-test covers the rule.

## 2026-09-11 — U016 main launcher Mac glass navigation

- Main launcher navigation received a second polish pass: stronger glass toolbar, compact status footer and consistent gold selection.
- Play / Server Console / Server Settings, Connection / Local server, Graphics, Advanced and server-settings category switches now expand to equal widths and disable scroll arrows, so Russian labels remain inside the fixed 960 px window.
- Play page now has compact section headers for connection and local hosting, with clearer primary/destructive actions and a dedicated build hero card.
- Footer server state is shown as a compact green/neutral status pill instead of raw colored text.
- Fixed-size glass windows keep the familiar third macOS traffic light visible as a disabled green control rather than dropping it entirely.

## U015 — Server console and settings UI polish

### Changed
- Rebuilt the embedded local-server console as compact glass cards with endpoint status, encoding/restart controls and a terminal-style log surface.
- Added monospace rendering plus lightweight INFO/WARN/ERROR/success highlighting without changing the bounded 5000-line console memory behavior.
- Reorganized the server settings editor into category tabs: Overview, Players, Combat & NPC, Progress and Network. Each category scrolls independently inside the fixed-size launcher.
- Kept form/config.lua synchronization actions permanently visible and styled Save as the primary action.
- Raw config.lua now uses a dedicated code-editor presentation with no line wrapping.

### Compatibility
- Existing widget IDs, config.lua keys, persistent/runtime synchronization and ArenaMP protocol are unchanged. This is a UI/layout-only server-management pass.

## U014 — Compact Mac Glass graphics UI polish

### Changed
- Checked boxes now render a real high-contrast check mark instead of a filled square; radio buttons and combo arrows use matching embedded Arena Glass resources.
- Rebuilt Graphics → Quality into a compact two-column card layout that fits the fixed 960×660 launcher without overlapping controls.
- Added segmented quality preset buttons while preserving the original hidden preset combo as the persistence/logic source.
- Hardware recommendation, profile explanation, terrain/PBR/water quick controls and streaming/occlusion are grouped into clearer sections.
- Moved “Link shadow distance to viewing distance” into the Shadows tab.
- Added RU strings for the new compact graphics UI and preset descriptions.

### Compatibility
- No ArenaMP protocol changes. No graphics setting key was renamed or removed. Existing profiles and `settings.cfg` remain compatible.

## U013 — Compact macOS-style Launcher layout

- Desktop Launcher is now a fixed **960×660** window with a denser top navigation strip, a compact glass content surface and one persistent footer action bar.
- The Play page is reorganized into **Connection** and **Local server** sub-tabs. Host-only controls, maintenance actions and server start/stop controls no longer compete with client connection fields in one long form.
- Check boxes, radio buttons, combo boxes, edit fields, tab bars, cards, scroll bars and footer actions now use one consistent macOS-inspired dark glass style with Morrowind brass/gold accents.
- The custom window chrome uses compact macOS-style traffic-light controls and a shorter title bar while preserving the existing inexpensive painted fallback and supported Windows compositor request.
- Removed the legacy `TES3MP 0.8.1 Zer0Custom` watermark from the bottom of the launcher.
- Removed the duplicate page-level Play/Update action from view; the footer remains the single persistent primary Play/Update action.
- Existing U012 Android server recovery, updater transport and build-manifest behavior are retained. ArenaMP network protocol is unchanged.

## U012 — Android Server asset recovery and separate launcher title

- PC and Android website buttons use build.com/url (optional override), then build.ini/url, with https://t.me/arena_mp as the fallback.

- Server preparation runs off the Activity UI thread; preparation/configuration failures show a persistent retry screen and the Update.log path instead of escaping onCreate or refresh.
- ServerActivity uses its original MyTheme and platform dialogs. Other U011 UI work is retained.
- Read file/directory identity and bytes from installed base/split APK ZIPs. Server staging keeps an APK snapshot open, verifies sizes/CRC and reports precise missing paths.
- Serialize server installation across launcher/service processes with a file lock. Preserve server/data and persistent configuration while swapping managed resources.
- Check required server payload before swapping assets; use an APK fingerprint if optional runtime-stamp metadata is absent.
- Verify server resources and native libraries inside the final APK in CI; add JVM asset installation regression tests.
- Place the Android launcher server/build title in its own full-width, wrapping glass card above the action toolbar. Long names no longer compete with Changelog, website and update buttons.
- Full APK/device validation still requires the complete Android project. See Android/tests/U012_VALIDATION_RU.md in this cumulative archive.

## U011 — Compiled-in Arena Glass UI

- Launcher, Wizard, Server dialog and native Qt Updater share the same compiled-in glass material, rounded window chrome and warm gold controls.
- Launcher actions and tabs use minimal SVG icons, with embedded PNG fallbacks when Qt SVG support is unavailable.
- Android launcher, server controls, changelog and updater dialogs share rounded resources and matching colors. Android 12+ dialog background blur is requested through public APIs, with an opaque fallback and unchanged SDK requirements.
- The Windows compositor is asked for Acrylic on supported systems; painted material remains the portable fallback. Native compositor effects still require device testing.
- Updater progress cannot be hidden by closing the window while transaction application has disabled cancellation.
- U010 Android server recovery and the existing update transport, storage and verification logic are retained.
- Added a standalone Qt UI smoke test and Linux CI gate. See `docs/arena-ui/README_RU.md` for integration and validation limits.

## U010 — Android server post-update recovery + Arena palette foundation

### Fixed
- Android local/dedicated server now recovers after an APK replacement instead of trusting a stale external `android-server.status` left by the killed `:arenamp_server` process. Active states are checked against the actual process and the package install time.
- APK asset fingerprints are now cached together with the installed APK identity (`lastUpdateTime`/APK file metadata), so a PackageInstaller return to an existing launcher process cannot reuse client/server fingerprints from the previous APK.
- A newly installed APK invalidates the managed server-asset stamp before the next server start. Saved `server/data`, player/world data and persistent server configuration remain preserved.
- Server start/native-load/runtime failures are written to `Update.log` (`server_start_error`, `server_native_load_error`, `server_service_error`) and host-mode launch no longer silently enters the game when the server already reports an error.

### UI palette foundation
- Desktop ArenaMP Launcher and Wizard now share one restrained Morrowind-inspired Qt palette: obsidian surfaces, warm brass/gold accents, parchment text and softer dark controls. Native window frames are intentionally kept for this first visual pass.
- Android launcher/server UI uses the matching obsidian/brass palette so desktop and mobile begin from the same colour system before the later liquid-glass/blur stage.
- No ArenaMP network protocol changes.

## U009 — Windows updater HTTPS/TLS fix

### Fixed
- Fixed automatic update checks on Windows failing with `TLS initialization failed` after the updater was moved from PyInstaller/Python to native Qt.
- `arena-updater.exe` now uses the native Windows WinHTTP HTTPS stack and Windows certificate store for `check.ini` and update package downloads, so it no longer depends on optional Qt/OpenSSL runtime DLLs.
- Partial downloads are removed after a network/hash failure, and `Update.log` records `transport=winhttp` for Windows downloads.
- Linux keeps the existing Qt Network transport. ArenaMP protocol is unchanged.

## U008 — Launcher Changelog window

### Added
- Desktop ArenaMP launcher now has a dedicated **Changelog** button. It opens the installed project changelog in a scrollable Qt window with formatted headings and lists.
- Android launcher now has the same **Changelog** action in the toolbar and a scrollable, formatted changelog dialog.
- Android ships the same changelog text inside the APK so it remains available offline.

### Update system
- The changelog is part of the normal build/update payload. Windows reads the installed `CHANGELOG.txt`; Android reads its packaged changelog asset.
- No ArenaMP network protocol changes.

## Y057 — Incapacitated Skill-Point Anti-Abuse

- Server rejects every `PlayerSkill` packet while `deathRecoveryActive` is true and immediately restores authoritative skills.
- `PlayerLevel` cannot alter level, level progress, Skill Points, attribute XP progress or reward keys while incapacitated; only death-recovery XP decay is accepted.
- Exploit remains blocked even if inventory/progression UI is opened or the client is modified.

## Y056 — engine-owned dynamic stats

Dynamic stats were stored as results, which is why they could not be checked and, once damaged, never recovered. They are now stored as the things they are computed from.

### Fixed
- Base health, magicka and fatigue no longer reset or collapse when logging in. The stored profile is no longer authoritative for stats the engine derives.
- Maximum fatigue is re-derived as Strength + Willpower + Agility + Endurance whenever `calculateDynamicStats` runs for the player, not only as a side effect of an attribute changing value. The snapshot `LoadStatsDynamic` writes over it at login used to stick forever. Hand-authored NPCs are untouched and keep the maximum fatigue their record gives them.
- Maximum magicka no longer divides by zero when the base is 0. The resulting NaN was written into the stat, sent to the server and stored, after which the character's magicka never recovered.

### Added
- `stats.healthLedger` in the player profile records what maximum health is built from: the level it starts at, the base health at that level, and the Endurance at every level-up since. Health is then `originHealth + fLevelUpHealthEndMult * sum(gains)` — addition only, every term stored, so it can be rebuilt exactly however many times it is lost. The ledger is reconciled against the recorded level, so a missed or duplicated level packet corrects itself.
- New characters seed the ledger at level 1 with their character creation health, a true origin. Characters that predate it seed once from the value the client rebuilds on first login, using the race and class records the server cannot read; everything after that point is exact.
- Client-side rebuild for that one-time seed: floor is `chargen + k*L*E0`, below which nothing legitimate can sit, and the seed walks the same levels with Endurance rising in even steps, scaled by 1.4 to compensate for Endurance usually having been raised earlier than evenly. Werewolf form is exempt and keeps its own base health.

### Multiplayer / compatibility
- No new packet IDs. Protocol remains **806**. One new optional profile field, `stats.healthLedger`; profiles without it are seeded on first login.

## Y043 — archetype presentation, localisation and recovery HUD

- Added a clear selected-archetype card and 0–100% power bar to preset/custom class creation.
- Moved the persistent archetype presentation from Spellbook to Statistics, directly below Class, with detailed Buff/Debuff runtime tooltip.
- Added complete EN/RU l10n entries for all 28 archetype names, buffs and debuffs; Russian Nightblade is displayed as «Клинок ночи».
- Preserved native archetype Active Effect icons while removing the oversized Spellbook description block.
- Reworked death recovery into a frameless multiline lower-HUD prompt with shadowed text and a blinking red Apply (E) action.
- Protocol remains 806.

## Y039 — death XP countdown + potion/party recovery

### Added
- Death starts a server-authoritative 10-second current-level XP decay instead of deleting XP in one frame. The HUD shows the remaining XP, countdown and a shrinking XP bar.
- While incapacitated, pressing **E** consumes one real Restore Health potion from the player's inventory and revives them in place at 25% health.
- A nearby member of the same ArenaMP group can face the incapacitated player and press **E** to consume one of their own Restore Health potions and revive the ally in place.
- A successful Restore Health **Touch** cast by a party member can also revive an incapacitated ally, using the normal spell-cast/magicka path.
- Recovery controls are hidden transport messages and are suppressed from RP/local chat.

### Changed
- Ordinary jail now removes exactly 50% of current-level XP; level, Skill Points and skills are untouched.
- Death XP checkpoints use the server monotonic millisecond clock and incoming PlayerLevel packets are capped to the authoritative remaining-XP ceiling while incapacitated.
- Normal respawn never occurs before the 10-second XP decay window finishes. Potion/touch recovery cancels shrine teleport and death-jail handling and restores the player where they fell.

### Multiplayer / compatibility
- Recovery state is transient; only the already-existing XP value is persisted.
- No new packet IDs or save fields are added. Protocol remains **806**.

## Y038 — archetype UI + NPC parity

### Added
- Magic/Spells now has a persistent archetype block with the current archetype name, perk and trade-off.
- Runtime archetype MagicEffects now appear in the normal Active Effects icon strip with `Archetype: <name>` / `Архетип: <name>` as their source.
- Ordinary NPCs now derive the same one-of-28 archetype from their ESM class favourite attributes and receive the same regeneration, movement, combat, spell, passive-magic and damage-trade-off mechanics as players.
- NPC world tooltips show the archetype name; Full Help also shows perk/trade-off. Dialogue headers show `NPC — Archetype - level`.

### Changed
- Fire Warrior elemental weapon damage, Spell Absorption, hit/evasion, weapon damage, incoming damage, movement, capacity, spell success and regeneration are no longer player-only.
- Merchant archetype barter advantage is now symmetric: both the player and the NPC seller influence the final quote.
- Sneak-only Chameleon/Night Eye remains stance-gated for NPCs as it is for players; no NPC receives free stealth merely from its class.

### Multiplayer / compatibility
- NPC archetypes are deterministic from existing class/attribute state; no new actor/player save fields or network packet fields are added.
- Protocol remains **806**.

## Y037 — signature archetype mechanics

### Added
- **Thief** now gains native Chameleon only while genuinely Sneaking; magnitude scales with Agility + Speed and Sneak skill and is capped at 40.
- **Nightblade** gains Sneak-only Night Eye plus a lighter Chameleon veil; **Rogue** gains a smaller Sneak-only Chameleon effect.
- Strength + Intelligence becomes **Fire Warrior**: weapon hits add deterministic scaling Fire Damage, respect native fire resistance/weakness and use native hit VFX/sound, while retaining Magicka regeneration and the medium/heavy-armour burden.
- Added native passive signatures to multiple other archetypes: Spell Absorption, Sanctuary, Resist Magicka/Fire/Paralysis/Poison/Disease and Detect Animal/Key/Enchantment.
- Passive signatures are derived in realtime for LocalPlayer/DedicatedPlayer only and never create persistent ActiveSpell/save records.

### Multiplayer / compatibility
- Fire Warrior elemental damage remains outside the physical `PlayerAttack.damage` field and is deterministically re-derived from existing synchronized class/attribute/attack data.
- No new save fields or packet fields were added; ArenaMP protocol remains **806**.

## Y036 — 28 attribute-pair class archetypes

### Added
- The two favourite class attributes now select one of exactly 28 archetypes (`C(8,2)`), with EN/RU names, a strong perk and a visible trade-off.
- Custom class creation replaces the stock `Adventurer` name with the pair archetype while preserving a name typed manually by the player.
- The class editor shows the archetype, perk and drawback directly under the major/minor skill grid.
- Archetype strength scales continuously with the current values of both favourite attributes; no new save field or network packet is used.
- Perks can affect realtime Health/Magicka/Fatigue recovery, movement, carrying capacity, weapon damage, hit/evasion, spell success, incoming damage, barter and disposition.
- Medium/heavy armour load is available as a real mechanical drawback for hybrid/mobile archetypes.

### Compatibility
- Existing classes and saves automatically derive their archetype from their two stored favourite attributes.
- ArenaMP protocol remains **806**.

## Y035 — Windows/MSVC proximity voice build fix

### Fixed
- Fixed `VoiceChat.cpp` failing under MSVC because the anonymous namespace referenced `VoiceFrame::SampleRate` and `VoiceFrame::FrameSamples` without the `mwmp::` qualifier.
- The missing type caused the reported C2653/C2065 errors and the following C2131 `std::array` cascade.
- Voice transport, native lip sync and protocol **806** are unchanged.

## Y034 — protocol 806 + native voice lip sync

### Changed
- ArenaMP network protocol is restored to **806**. `ID_PLAYER_VOICE` and `CHANNEL_VOICE` remain appended after the existing ArenaMP IDs/channels, so existing numeric message/channel values are not renumbered.
- Proximity voice now drives OpenMW's native head `talk:start` / `talk:stop` morph from realtime microphone/decoded voice loudness.
- Remote players open and close their mouths according to received speech energy; the local third-person player does the same while holding PTT.
- A small noise gate and short decay prevent microphone background noise or a missing packet from leaving the mouth stuck open.
- Voice lip sync is an audio/head-morph state only; it does not replace or interrupt walking, combat, interaction or persistent pose animations.

### Protocol
- ArenaMP protocol remains **806**. Y034 client/server are still recommended together because older 806 builds do not implement the voice message.

## Y033 — proximity voice foundation

### Added
- Native in-game push-to-talk proximity voice for ArenaMP.
- `V` is the default PTT key and is configurable in `tes3mp-client.cfg` under `[Voice]`.
- SDL capture accepts the microphone native frequency/format/channel count and converts it to the 16 kHz mono voice format when needed.
- 16 kHz mono IMA ADPCM transport (~64 kbit/s payload rate) with independent 20 ms frames.
- Dedicated `ID_PLAYER_VOICE` realtime packet on an unreliable voice channel with per-speaker sequence numbers.
- Server-side interest filtering by loaded-player AOI, interior cell and authoritative distance.
- 3D streamed playback attached to the remote player head with OpenAL distance attenuation.
- PTT is suppressed while the chat edit field is active, so typing the PTT letter does not transmit.
- Server rejects malformed voice frames and rate-limits each sender above 75 voice packets/s (normal 20 ms voice is 50 packets/s).
- Voice traffic is transient and is never written to player/world JSON saves.

### Protocol
- ArenaMP protocol kept at 806; voice appends a new ArenaMP-only message without renumbering existing message IDs.

## Y032 — MMO personal journal/topics + Y029/Y030/Y031 merge

- Consolidates the Y029 cumulative with the Y030 group-escort target fix and Y031 escort-home loop fix into one changed-files package.
- Restores the intended MMO defaults: `shareJournal=false` and `shareTopics=false`; CO-OP can still enable shared progression from the Launcher.
- Launcher preset detection now distinguishes exact MMO, exact CO-OP and `Custom / mixed`. A mixed ruleset can no longer be displayed as MMO merely because it is not fully CO-OP.
- Selecting MMO explicitly writes the personal-progression flags, so the old Y014 mixed state (`journal/topics=true` with other progression flags false) cannot silently survive behind the MMO label.
- `world.json` no longer serializes `journal` or `topics` while their global sharing options are disabled. Existing stale keys are pruned on world load in MMO mode; in-memory empty tables are retained for CoreScripts safety.
- Before pruning a non-empty legacy shared journal/topic set, Y032 stores it in `world/legacySharedProgress.json`; each account merges that former shared progression into its personal save on login. The merge is idempotent, so offline accounts are not lost across restarts.
- Player journal/topic events continue to save through `Players[pid]` when global sharing is disabled; optional group synchronization still merges personal player data and does not use `world.json`.
- ArenaMP network protocol remains 806.

## Y029 — Cell actor resync + group escort

- Reassert actor-relevant persistent cell state on same-session revisits.
- Rehydrate native Cell actor cache from replayed position/stats snapshots.
- Network AiEscort/AiEscortCell and persist authored FOLLOW/ESCORT packages.
- Group-aware player target failover for persistent follower AI, including immediate nearest-member retarget on target death/cell exit and a non-destructive wait state when no group member remains nearby.
- Fix ActorAI builder so escort target + duration + destination are serialized together.

## Y028
- Floating damage numbers are smaller (15 px), red, and use a negative HP format such as `-12`.
- Hostile overhead HP is thinner (3 px near / 2 px minimum far) while the docked HUD bar remains 9 px.
- The decorative HP frame now fades continuously with dockBlend instead of appearing at a hard threshold; Y027 fill safety is retained.

## Y027
- Combat HP frame cannot render without a positive fill width.
- Floating damage feedback is emitted once from the finalized local outgoing PlayerAttack.

## Arena Y025 — combat HP cell reset / damage-number recovery

- Hard reset of combat HP slots on player CellStore changes.
- Frames/fills remain hidden until valid current/max HP is resolved for the current slot owner.
- Damage numbers no longer depend on transient crosshair visibility or a non-empty weapon Ptr.
- Protocol remains 806.

## Arena Y024 — HUD chat live feed / scroll isolation

- Gameplay HUD chat is no longer a history browser: its scroll bar is hidden and mouse-inactive outside the Player Menu.
- The HUD continuously follows the newest message, including while the ordinary game cursor is visible.
- Player Menu Chat keeps manual history scrolling; new messages preserve a scrolled-up review position and auto-follow only when already at the bottom.
- The pause menu keeps its separate scrollable chat history and now also preserves a scrolled-up position when chat arrives.
- Protocol remains 806.

## Arena Y023 — direct combat HP fill

- Combat HP no longer uses MyGUI::ProgressBar or Track state.
- Overhead enemy HP is a thin red direct-width fill with no frame.
- The frame is a separate widget shown only after docking into the HUD.
- Last verified HP is retained through the existing short linger/fade window.
- A living actor always gets at least a 1 px fill, preventing an empty framed bar.

## Arena Y022

- Replaced distance-based combat ProgressBar skin swapping with one permanent `Arena_Progress_Red_Combat` skin.
- Aggressive NPC overhead HP is always a thin 3–4 px red line with no decorative frame.
- The frame is a permanent child widget and fades in only near the end of docking into the HUD stack above stamina.
- Removed combat `changeWidgetSkin()` calls, so MyGUI no longer destroys/recreates ProgressBar Track while a fight is running.
- Combat HP slots are enemy-only; Y021 floating damage numbers remain enabled.

## Arena Y021

- Added compact floating weapon-damage numbers beside the crosshair.
- Numbers use final physical HP loss after resistance, armor and difficulty scaling.
- Misses, blocks, zero damage and spell damage stay silent.
- Numbers alternate left/right, drift upward/outward and fade in about 0.85 seconds.


# Changelog

## Y020 — cell-load desync / NPC AI / actor loot duplication / protected grab

- Hardened `EnsurePacketTables`, `LoadObjectsMoved` and `LoadObjectsRotated`: legacy/partially upgraded cells can no longer hit `pairs(nil)` and abort `LoadInitialCellData` before ActorList/AI/position packets are delivered.
- Removed Y013's runtime destination actor-cache pre-seed from `ProcessorActorCellChange`; exterior authority hand-offs again have one cache owner instead of briefly materializing the same NPC in both adjacent cells.
- Destination `actorList` persistence is now limited to transitions involving an interior, preserving reliable NPC return without contaminating ordinary exterior grid hops.
- Integrated the supplied Y019 server-side actor-loot repair for authoritative container SET, death, and lazy load; repaired legacy corpse data is persisted immediately.
- Throttled ActorEquipment-triggered cell quicksaves to the configured interval.
- Fixed the client-side source of new equipment duplicates in `DedicatedActor::setEquipment`: an existing item is reused by refId across authority/cell reinitialization and mutable charge state is updated in-place.
- Replaced RP/home/faction placement permissions with a narrow physics-grab anti-grief rule: regular players may move only `droppedByPlayer` ObjectPlace items, original world references/statics are protected, Moderator+ bypasses the rule, and non-gameplay script/server transforms are untouched.
- `gold_001` still weighs `0.0001` in real encumbrance and its tooltip now displays that enforced weight instead of reading the zero ESM base value.
- Protocol remains 806.

## Y017 — combat HP readability, healing feedback, ammunition and gold weight

- Kept the overhead enemy HP fill readable at long distance by clamping only the far-distance height to 4 px; close/medium presentation is unchanged.
- Health increases now wake the auto-hidden HP HUD; Magicka and Fatigue still ignore passive increases.
- Successful local bow/crossbow/thrown releases show `-1 <item name> (<remaining>)` in the right-side HUD feed.
- `gold_001` has an engine-enforced weight of `0.0001` per coin.


## Y016 — server build callback hotfix

- Registered `OnObjectMove` and `OnObjectRotate` in `ScriptFunctions::callbacks`.
- Fixes MSVC C2131 (`CallBackData` recursing past callback index 69) when building `tes3mp-server`.
- Keeps the server-authoritative RP object-placement validator enabled; protocol remains 806.


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

## Y018 — weather fog / land optimization stability
- Fixed weather fog flicker/popping while adaptive land optimization changes the live view distance.
- Preserve fog depth values above 1.0 instead of clamping them to 1.0 during far-plane updates.

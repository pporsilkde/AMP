## Y028
- Floating damage numbers are smaller (15 px), red, and use a negative HP format such as `-12`.
- Hostile overhead HP is thinner (3 px near / 2 px minimum far) while the docked HUD bar remains 9 px.
- The decorative HP frame now fades continuously with dockBlend instead of appearing at a hard threshold; Y027 fill safety is retained.

## Arena Y025 — combat HP cell reset / damage-number recovery

- Hard reset of combat HP slots on player CellStore changes.
- Frames/fills remain hidden until valid current/max HP is resolved for the current slot owner.
- Damage numbers no longer depend on transient crosshair visibility or a non-empty weapon Ptr.
- Protocol remains 806.

## Y024 chat HUD

Gameplay HUD chat is always a live feed with no manual scroll. Scrollable history is available only in Player Menu Chat and the in-game pause menu.

## Arena Y023 — direct combat HP fill

- Combat HP no longer uses MyGUI::ProgressBar or Track state.
- Overhead enemy HP is a thin red direct-width fill with no frame.
- The frame is a separate widget shown only after docking into the HUD.
- Last verified HP is retained through the existing short linger/fade window.
- A living actor always gets at least a 1 px fill, preventing an empty framed bar.

## Y022 combat HP stability

- Hostile NPC overhead HP is a thin red frameless line.
- Frame appears only after docking into the HUD above stamina.
- No runtime combat ProgressBar skin swaps.


## Arena Y021

Adds compact final weapon-damage numbers beside the crosshair. They alternate left/right, drift upward/outward and fade. Spell damage, misses and blocks do not create numbers.

# ArenaMP Y013 cumulative

> Y016 build hotfix: `OnObjectMove` / `OnObjectRotate` are now registered in the server callback table; fixes MSVC C2131 without changing protocol 806.

Y013 добавляет HUD item dedupe, remote locomotion self-heal, server-authoritative RP object placement и надёжный NPC exterior/interior return поверх полного Y012. Protocol 806.

# ArenaMP

**English** · [Русский](README_RU.md)

ArenaMP is an experimental multiplayer engine for *The Elder Scrolls III: Morrowind*. It continues the TES3MP 0.8.1 codebase, adds a bundled server core, and integrates extensive interface, gameplay, rendering, AI, and administration work.

> A legal copy of *Morrowind* is required. Bethesda game assets are not included.

## Project identity

| Item | Value |
|---|---|
| Source snapshot | Y012 |
| Engine foundation | OpenMW 0.47.0 |
| Multiplayer heritage | TES3MP 0.8.1 |
| ArenaMP network protocol | 806 |
| License | GPLv3 with the additional terms in [LICENSE](LICENSE) |

ArenaMP is a fork, not an official OpenMW or TES3MP release. Its protocol number and packet layouts differ from stock TES3MP. Use an ArenaMP client and server built from the same source revision.

## Highlights

- integrated client, dedicated server, browser, launcher, and bundled CoreScripts;
- EN/RU interface and per-client server message localization;
- Player Menu with structured chat, groups, player roster, nickname colors, and color emoji;
- server-authoritative groups, friendly-fire policy, shared same-cell XP, and optional journal/topic sync;
- server-authored quests, in-game Quest Studio, and per-player quest-item phasing;
- modern character creation, Quick Loot, inventory improvements, target information, and combat bars;
- native XP progression, equipment requirements, Refined Alchemy, poison coating, and configurable gameplay rules;
- synchronized tactical NPC combat, door pursuit, authority recovery, and return-home behavior;
- HDR, bloom, SMAA, enhanced PBR, water controls, dynamic shadows, groundcover fixes, and CPU occlusion culling;
- optional Project Magnus clustered lighting on supported desktop OpenGL 4.3 hardware;
- release automation for Windows x64, Linux x64/Steam Deck, and experimental macOS ARM64 packages.

See [Feature overview](docs/FEATURES.md) and [Differences from TES3MP](docs/TES3MP_DIFFERENCES.md) for the detailed scope.

## Compatibility

| Combination | Status |
|---|---|
| Matching ArenaMP client and server | Supported target |
| Different ArenaMP revisions | Not guaranteed |
| ArenaMP client and stock TES3MP 0.8.1 server | Not compatible |
| Stock TES3MP client and ArenaMP server | Not compatible |
| Existing TES3MP Lua scripts | Often portable, but ArenaMP APIs and behavior must be reviewed |
| Morrowind/OpenMW 0.47-era content | Generally compatible; every client must use the server's exact data-file order |
| Modern OpenMW Lua mods | Not supported by the OpenMW 0.47 foundation |

## Getting started

### Playing

1. Download a package for your platform from the repository's [Releases](https://github.com/MrZer0x0/TES3MP/releases) page or build it from source.
2. Run `openmw-launcher` and point it to a legal Morrowind installation.
3. Configure the same content files and load order used by the server.
4. Start `tes3mp`, select a matching ArenaMP server, and sign in.

Tap the chat key for quick input; hold it to open the Player Menu. The default server configuration uses `Y` in its welcome hint.

### Hosting

The release package contains `tes3mp-server` and the same server scripts as this repository. Configure the server through the launcher's server page or edit `server/scripts/config.lua`, then start `tes3mp-server`.

Read [Server mechanics](server/ARENAMP_CONFIG.md), [server README](server/README.md), and [server security](server/COREARENAMP_SECURITY.md) before opening a public server.

### Building

The canonical release workflow is [.github/workflows/release-builds.yml](.github/workflows/release-builds.yml). Platform instructions and supported build targets are documented in [Building ArenaMP](docs/BUILDING.md) or [Сборка ArenaMP](docs/BUILDING_RU.md).

```bash
git clone --recurse-submodules https://github.com/MrZer0x0/TES3MP.git
cd TES3MP
bash CI/ensure-bundled-deps.sh
```

## Documentation

- [Feature overview](docs/FEATURES.md)
- [Differences from TES3MP 0.8.1](docs/TES3MP_DIFFERENCES.md)
- [Отличия от TES3MP 0.8.1](docs/TES3MP_DIFFERENCES_RU.md)
- [Building ArenaMP](docs/BUILDING.md)
- [Сборка ArenaMP](docs/BUILDING_RU.md)
- [Changelog](CHANGELOG.md)
- [Contributing](CONTRIBUTING.md)
- [EncoreMP companion content](extras/encoremp/README.md)

## Repository layout

| Path | Purpose |
|---|---|
| `apps/` | Client, server, launcher, browser, and tools |
| `components/` | Shared engine and multiplayer libraries |
| `files/` | Runtime configuration, shaders, layouts, localization, and assets |
| `server/` | Bundled ArenaMP CoreScripts and seed data |
| `CI/` | Platform build and validation scripts |
| `extras/` | Optional content not required to compile the engine |
| `docs/` | Project and retained upstream documentation |

## Current status

ArenaMP Y012 is a development snapshot, not a stable semantic release. Tactical combat, clustered lighting, ragdolls, and some advanced render paths remain experimental. Public servers should be tested with several clients and backed up before upgrading.

Android/ng-gl4es support code exists, but Android is not currently produced by the release workflow and should be treated as an unsupported development target.

## License and credits

ArenaMP retains the OpenMW and TES3MP license and contributor history. See [LICENSE](LICENSE), [AUTHORS.md](AUTHORS.md), [CREDITS.md](CREDITS.md), and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

OpenMW, TES3MP, OpenMoji, and Bethesda Softworks do not endorse this fork.

- **Y007 HUD event feed:** a six-slot icon feed above the stamina/combat stack shows gained items, aggregated gold and newly applied lasting effects.
- **Y009 HUD stability:** live exact-instance effect timers, exact MP inventory-SET reseeding, pooled combat-bar parity, and actor-batch receive isolation.

### Y012 — water/XP penalty repair
- Shader water no longer falls back to legacy particle movement rings when shader-ripple simulation is disabled.
- XP cards are iconless and localized; positive/negative/neutral events use green/red/black semantics.
- Death is server-authoritative: all current-level XP is wiped, but earned level, Skill Points and skills are preserved. At exactly 0 XP the server adds `level × 5s` to respawn time.
- Jail wipes current-level XP and disables the old random skill changes while XP Leveling is enabled. Network protocol remains 806.

### Y011 unified HUD notifications
The right-side event feed now uses one uniform medium-opacity backing instead of three shade bands. Arena XP rewards and gameplay XP status messages use the same lane; repeated rewards from the same source coalesce. GUI-triggered XP warnings keep their MessageBox fallback. ArenaMP chat input is 14 px and white while chat history colours stay unchanged.

### Y010 HUD feed polish
Event cards now hug the right edge with a 2 px margin, use a borderless soft shadow background, and show existing inventory totals as `+5 (10)` after pickups.

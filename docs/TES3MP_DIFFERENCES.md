# ArenaMP compared with TES3MP 0.8.1

ArenaMP inherits its multiplayer foundation from TES3MP 0.8.1 and its engine foundation from OpenMW 0.47.0. It is not a drop-in replacement for a stock TES3MP client or server.

## Compatibility first

Official TES3MP 0.8.1 identifies its protocol as `10`. ArenaMP Y001 identifies its custom protocol as `806` and extends packet layouts and native script APIs. The shared `0.8.1` version string records ancestry; it does not make the binaries network-compatible.

Use the same ArenaMP source revision for the client and server. A server administrator should treat a client/server mix as unsupported even if both display `0.8.1`.

## Major differences

| Area | TES3MP 0.8.1 | ArenaMP Y001 |
|---|---|---|
| Network identity | Protocol 10 | Custom protocol 806; matching build required |
| Server scripts | CoreScripts normally selected/deployed separately | ArenaMP core is bundled and packaged with the engine |
| Localization | Primarily server-script dependent | Built-in EN/RU client UI and per-client server localization |
| Chat | TES3MP chat window and script commands | Player Menu, structured RP/OOC channels, roster, colors, emoji, UTF-8 safety |
| Groups | Script-specific implementations | Persistent integrated groups, invitations, leader actions, group UI, XP sharing |
| Friendly fire | Standard ally mechanics and server scripts | Native disabled/enabled/group policy synchronized with the integrated party roster |
| Quests | TES3MP world/journal sync controlled by scripts | Server-authored quests, Quest Studio, localized stages, per-player quest-item phasing |
| Progression | Morrowind skill/level model unless scripts replace it | Native server-authoritative XP model and optional death loss |
| Character creation | TES3MP/OpenMW 0.47 flow | Modern staged flow with full preview and multiplayer-safe review/finalization |
| Interface | Stock OpenMW/TES3MP windows | Quick Loot, inventory/container improvements, target panel, combat bars, Player Menu |
| NPC AI | Known synchronization and authority limitations | Multi-target combat sync, stable authority, tactical movement, door pursuit, home recovery |
| Rendering | OpenMW 0.47 renderer | Added HDR/bloom/SMAA, PBR controls, water work, occlusion culling, optional clustered lighting |
| Server administration | CoreScripts configuration and tools | Launcher-managed server settings, config hot reload, manifests, security checks |
| Position recovery | Stock persistence behavior | Continuous safe-position cache and restart collision guard |
| Release targets | Official 0.8.1 packages | Current workflow: Windows x64, Linux x64/Steam Deck, experimental macOS ARM64 |

## What remains familiar

- The main executables remain `tes3mp`, `tes3mp-server`, and `tes3mp-browser`.
- The project still uses the TES3MP server-side Lua model and much of its API.
- Existing TES3MP scripts can often be ported, but must be audited for ArenaMP's native extensions, packet changes, group rules, localization, and server-authoritative systems.
- Morrowind content files remain the gameplay data source. All clients must use the same content list and order required by the server.

## Upstream references

- [TES3MP repository](https://github.com/TES3MP/TES3MP)
- [TES3MP 0.8.1 release](https://github.com/TES3MP/TES3MP/releases/tag/tes3mp-0.8.1)
- [OpenMW 0.47.0 release](https://openmw.org/2021/openmw-0-47-0-released/)
- Retained upstream history: [TES3MP changelog](upstream/TES3MP_CHANGELOG.md) and [TES3MP credits](upstream/TES3MP_CREDITS.md)


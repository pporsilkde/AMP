# ArenaMP feature overview

This page describes the features present in the X057 source snapshot. Availability can depend on server settings, game data, GPU capabilities, and build options.

## Multiplayer and server core

- TES3MP-derived player, actor, object, cell, world, record, journal, and topic synchronization.
- Bundled ArenaMP CoreScripts installed from the same source tree on every supported desktop platform.
- Launcher-managed server settings with guarded updates of `server/scripts/config.lua`.
- EN/RU server messages selected per client or forced globally by the administrator.
- Hot reload for supported raw server configuration values.
- Stable actor authority rules, invalid cell-transition guards, actor home anchors, and recovery after authority handoff.
- Persistent player-position sampling and restart collision safety.
- Private interior instances and configurable spawn/respawn rules.
- Required-data-file manifest support and additional native server security checks.

## Player Menu, chat, and groups

- Quick chat input plus a resizable Player Menu.
- RP/OOC modes; say, whisper, shout, local OOC, and global OOC channels; `/me`, `/do`, and `/try` styles.
- Player list with details and group invitation shortcuts.
- Persistent groups with leader transfer, kick, leave, and disband actions.
- Optional party journal/topic synchronization.
- Same-cell sharing for validated kill and quest XP.
- Friendly-fire modes: disabled, enabled, or blocked between group allies.
- Nickname color palette and optional full-color OpenMoji atlas.
- UTF-8-safe chat truncation and deduplicated notifications.

The unfinished Home placeholder from earlier internal builds is not exposed in this cleaned snapshot.

## Quests and multiplayer progression

- Server-authoritative JSON quest definitions.
- In-game MyGUI Quest Studio for staff: draft, validate, publish, disable, and edit stages/dialogue/logic.
- Personal, party, and server-wide quest progress modes.
- Localized quest dialogue, journal text, requirements, rewards, and runtime errors.
- Per-player quest-item phasing backed by a server-owned quest index.
- Automatic quest-index bootstrap with fail-closed behavior when no valid index exists.
- Native XP leveling controlled by the server, including group sharing and configurable death loss.
- Server-controlled equipment requirements and related gameplay rules.

## Interface and interaction

- Modern multi-stage character creation with preview and review flow.
- Quick Loot with movement guards, keyboard/mouse navigation, and standard-container fallback.
- Searchable container/inventory presentation and improved workspace behavior.
- Compact target information and separate combat health bars.
- Dynamic camera and full-body first-person options.
- Synchronized interaction/consumption animations and player pose controls.
- Native book/scroll writing tools.

## Gameplay systems

- Refined Alchemy knowledge, revised potion workflow, and server-authoritative alchemy rules.
- Poison coating stored on an exact item instance.
- Configurable arrow-stick behavior.
- Weapon and shield sheathing, graphic herbalism, skill-book limits, and combat/equipment balance switches.
- Optional EncoreMP 0.93 companion ESPs under `extras/encoremp/`.

## NPC synchronization and combat

- Multi-target combat state synchronization with authority validation.
- Engine-side tactical movement: strafing, circling, short retreats, evasive jumps, and stuck recovery.
- Configurable combat magic/healing bias and attack selection.
- Limited pursuit through teleport doors with distance, chance, and actor-count budgets.
- Stable idle AI across authority changes and recovery of original AI packages.
- Return-home behavior for actors that leave their source cell.
- More stable dialogue poses and combat-bar tracking.

## Rendering and performance

- Expanded water presets and PBR water controls.
- HDR, tone mapping, half-resolution bloom, SMAA, and optional native post effects.
- Enhanced PBR material controls and parallax quality tiers.
- Dynamic shadow categories, resolution controls, and distance tuning.
- Groundcover, wind, interaction, and water-caustics integration fixes.
- CPU software occlusion culling with terrain occluders and conservative live tuning.
- Optional Project Magnus clustered lighting. It requires desktop OpenGL 4.3 and falls back to the existing shader-lighting path when unavailable.
- UI update reduction, configurable scene budgets, and release LTO/build-cache support where available.

## Experimental or limited areas

- Tactical combat, ragdolls, clustered lighting, and advanced post-processing need broader hardware and multiplayer testing.
- ArenaMP remains based on OpenMW 0.47. It does not provide the modern OpenMW Lua API.
- Android/ng-gl4es code paths are not part of the current release workflow.
- Client/server interoperability is supported only for matching ArenaMP revisions.


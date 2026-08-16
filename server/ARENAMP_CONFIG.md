# ArenaMP server mechanics

The bundled CoreScripts are the authoritative server core for the Windows,
GNU/Linux and macOS builds. The launcher edits `scripts/config.lua`; the server
then sends the mapped OpenMW `[Game]` settings to every connected player.

## NPC combat and pursuit

- `arenaTacticalCombat` enables ArenaMP tactical combat behaviour.
- `arenaCombatWeaponSheatheDelay` controls the post-combat sheathe delay.
- `arenaCombatPursuitThroughDoors` enables pursuit through teleport doors.
- `arenaCombatPursuitGuaranteedDistance` is the distance where door pursuit is guaranteed.
- `arenaCombatPursuitDoorMaxDistance` is the maximum distance considered for door pursuit.
- `arenaCombatPursuitMinimumChance` is the pursuit chance at the maximum distance.
- `arenaCombatPursuitMaxActors` limits the number of NPCs crossing one door transition.
- `arenaCombatPursuitMaxDistance` is the same-cell pursuit leash; `0` means unlimited.

## Other ArenaMP mechanics

The same section controls follower aggression, collision avoidance, giving way,
following over water, actor processing range, looting during death animations,
weapon/shield sheathing, graphic herbalism, long-blade Agility scaling,
two-handed accuracy, staff accuracy, skill-book level limits, constant-effect
difficulty and the global XP multiplier.

Values are clamped to launcher-supported ranges when the server starts. The
server-side values override corresponding local client settings while connected.

## Friendly fire

`friendlyFireMode` controls player-versus-player harmful effects and accepts
three canonical values:

- `disabled` - players cannot damage or apply harmful magic effects to other players;
- `enabled` - unrestricted player-versus-player damage;
- `group` - damage is allowed against other players, but blocked between allies
  created with `/invite` and `/join`.

The default is `group`. Weapon attacks, hand-to-hand, arrows, on-strike
enchantments, touch/target/area spells, reflected effects and lasting harmful
effects use the same rule. Beneficial magic is not blocked.

### Lua interface

CoreScripts expose both native `tes3mp` functions and the higher-level
`friendlyFire` module:

```lua
local mode = friendlyFire.GetMode()
local grouped = friendlyFire.ArePlayersGrouped(firstPid, secondPid)
local canDamage = friendlyFire.CanDamage(attackerPid, targetPid)

-- Change the live rule and resend settings to connected players.
friendlyFire.SetMode("disabled")
```

Native equivalents are:

```lua
tes3mp.SetFriendlyFireMode("group")
tes3mp.GetFriendlyFireMode()
tes3mp.ArePlayersAllied(firstPid, secondPid)
tes3mp.IsFriendlyFireAllowed(attackerPid, targetPid)
```

Custom scripted damage should call `friendlyFire.CanDamage()` before changing
a player's health or applying a harmful scripted effect.

## Client language and server localization

The first editable setting in `server/scripts/config.lua` is:

```lua
config.serverLanguage = "AUTO"
```

Supported values:

- `AUTO` - use the normalized language reported by each client;
- `RU` - force Russian server messages for every player;
- `EN` - force English server messages for every player.

The launcher exposes the same choice as **Server language** in the General
server-settings form. Saving the form updates `config.serverLanguage`.

Clients report a normalized language flag in `PlayerBaseInfo`. Russian clients
use `RU`; English and every other language use `EN`. The raw client flag remains
available as `tes3mp.GetLanguage(pid)`, `localization.GetClientLanguage(pid)` and
`Players[pid].language`. `localization.GetLanguage(pid)` returns the effective
language after applying `config.serverLanguage`.

`config.defaultLanguage` is the fallback used only in `AUTO` mode when no valid
client flag is available.


Runtime Lua helpers are also available:

```lua
localization.GetServerLanguage()       -- AUTO, RU or EN
localization.SetServerLanguage("RU")   -- changes the live rule until restart
localization.GetClientLanguage(pid)    -- raw RU/EN client flag
localization.GetLanguage(pid)          -- effective RU/EN language
```

CoreScripts load `server/scripts/localization.lua` and the bundled
`server/scripts/locales/core.lua` dictionary. External scripts can register a
namespace with `localization.RegisterDictionary()` or load a Lua dictionary
module with `localization.LoadDictionary()`.

```lua
localization.RegisterDictionary("example", {
    EN = { greeting = "Hello, {name}!" },
    RU = { greeting = "Привет, {name}!" }
})

localization.Message(pid, "example", "greeting", { name = Players[pid].name })
```

The multiplayer protocol is `806`; client and server must be updated together.

## CoreArenaMP persistence and native security (v18)

The bundled server loads `CoreArenaMP_DataManager.lua` and
`CoreArenaMP_BaseScript.lua` as internal CoreScripts. Player, cell, world and
record-store JSON persistence goes through path validation and protected
read/write wrappers. The launcher recreates required empty `server/data`
subdirectories before starting the server so first-time profile creation does
not depend on archive tools preserving empty folders.

The launcher no longer overwrites a newly bundled `server/scripts/config.lua`
with an old full copy from `userdata/server-config.lua`. The bundled CoreScript
is the template; scalar user values are migrated on top of it. Before a changed
legacy configuration is migrated, the launcher saves a timestamped
`server-config.before-core-update-*.lua` backup in `userdata`.

Native C++ validation is the authoritative boundary for untrusted multiplayer
input. Version 18 adds:

- finite-number (`NaN`/`Inf`) checks and semantic bounds for player, actor,
  object and worldstate packet data;
- semantic negative-value checks: fields such as base/max stats, item counts,
  durations, costs and damage cannot be negative, while legitimate signed
  coordinates, modifiers and protocol sentinel values remain supported;
- validation for attributes, skills, dynamic stats, level/progress, bounty,
  reputation, attacks, casts, spell lists, active spell effects, inventory,
  equipment, factions, journal/quick-key data, object state, actor state,
  client globals and dynamic records;
- per-packet-type transactional rollback: if a player packet is malformed or
  rejected, only the fields that packet attempted to modify are restored to the
  previously authoritative server values before any Lua callback or broadcast;
- server-authoritative movement validation and position rollback for impossible
  speed/teleport samples;
- actor authority enforcement for actor AI/equipment/stats/position/attack/cast
  and other actor-state updates;
- cross-player leases for NPC/container interaction, including atomic checking
  of every object in a multi-container packet;
- packet/list allocation caps, string size caps, malformed/truncated primitive
  read handling and global packet-rate limits to reduce packet/OOM abuse;
- unknown-connection and handshake/replay guards before player state is
  dereferenced or initialized.

The client contains matching preflight guards for malformed outgoing data and
interaction spam. These checks improve robustness but are not trusted as the
anti-cheat boundary: a modified client can remove them, so the server repeats
validation independently.

ArenaMP does **not** scan, terminate or hook ArtMoney/Cheat Engine processes.
Instead, it rejects or rolls back invalid network state that memory editing
tries to make the client submit. This avoids invasive process inspection and
keeps the important checks on the server.

See `COREARENAMP_SECURITY.md` for the exact negative-value policy, coverage and
known limits.

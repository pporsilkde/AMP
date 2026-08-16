# CoreArenaMP native security v18

CoreArenaMP treats every client packet as untrusted. The server is the security
boundary; client-side checks are only an early filter and robustness layer.

## Numeric validation

Incoming player, actor, object and worldstate packets are checked for finite
floating-point values and field-specific bounds before Lua callbacks or relay.
This includes attributes, skills, dynamic Health/Magicka/Fatigue data, level,
progress, bounty, reputation, inventory/equipment values, attack/cast data,
spell effects and durations, object/actor state, client globals and supported
dynamic records.

Negative numbers are rejected only where the field semantics make them invalid.
Examples include base/max stat values, item counts, durations, prices/costs,
damage and most indices/counts. Negative values remain allowed where Morrowind
or the TES3MP protocol intentionally uses them: world coordinates and rotation,
stat modifiers/current values that can cross zero, `-1` sentinel values such as
an absent faction rank/weather/charge, unlocked lock levels, and signed spell
effect magnitudes where the engine can legitimately use the sign.

The validator also rejects `NaN`, positive/negative infinity, impossible enum
values, excessive strings and oversized lists before those values reach normal
gameplay processing.

## Transactional player state

Before an incoming player packet is read, ArenaMP serializes the server's
current values for that packet type into a temporary BitStream. If the packet is
malformed or fails validation, those exact fields are deserialized back into the
Player object. Rejected position packets also resend the authoritative position
to the client.

This avoids a subtle class of failures where a packet could be rejected after
its values had already overwritten in-memory server state.

## Movement and interaction authority

Movement samples are validated server-side for finite values, packet rate,
large teleports and impossible speed. The baseline is reset during legitimate
cell changes.

NPC/container interactions use server-side cross-player leases. A second player
cannot mutate the same leased target concurrently. For container packets that
contain multiple object targets, every lock is checked first and leases are
acquired only if the complete set is available, avoiding partial lock state.

Actor-state packets additionally require the sender to hold actor authority for
the cell, with only the existing dedicated follower cell-change path keeping its
protocol exception.

## Packet hardening

Deserializer list sizes are capped before allocation for player, actor, object
and worldstate packets. Primitive/string reads mark a packet invalid on failure;
failed arithmetic/enum reads are reset to a defined zero value so a truncated
packet cannot leave an uninitialized `count` that later reaches `resize()` or a
loop. Unknown GUIDs are rejected before player dereference, handshake data is
read into a local structure, and replayed loaded/session initialization packets
are ignored.

## Client preflight

The ArenaMP client checks outgoing values for malformed numerics and invalid
counts/stats and rate-limits suspicious local interaction patterns. These checks
are deliberately secondary: a modified executable can bypass local checks, so
no server decision depends on their presence.

## ArtMoney / Cheat Engine model

CoreArenaMP does not scan process names, inspect another process's memory, kill
memory editors or install anti-debugging hooks. Instead it protects the network
trust boundary: invalid or impossible values produced by memory editing are
rejected and, for player packets, rolled back to the server snapshot.

This is materially stronger than relying only on client-side detection, but it
is not a claim that every plausible in-range cheat is mathematically
undetectable. For example, a forged inventory operation that is numerically
valid may require gameplay provenance/transaction checks to prove where the item
came from, and a spell magnitude that is within structural limits cannot always
be validated against plugin records if the headless server lacks the relevant
record context. Those cases should be extended with authoritative gameplay
transaction checks rather than by trusting the client.

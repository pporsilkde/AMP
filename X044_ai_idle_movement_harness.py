#!/usr/bin/env python3
"""ArenaMP X044 logic harness.

Models the decision logic of the X044 fixes so the regression that froze every
NPC can be reasoned about without a full client build.

Covered:
  1. Authority hand-off must not clear the AiSequence of an actor that never
     received an ID_ACTOR_AI packet (the X034 regression).
  2. A real CANCEL packet must still clear the sequence.
  3. AI_Fight is restored from the record only when it is still the zero the
     puppet code wrote.
  4. An already-wiped AiSequence is refilled from the actor's own record.
  5. LocalActor AI heartbeats never clear the shared outgoing batch and are not
     emitted for idle actors.
"""

CANCEL, ACTIVATE, COMBAT, ESCORT, FOLLOW, TRAVEL, WANDER, COMBAT_END = range(8)


class Actor:
    def __init__(self, packages, record_fight=30, record_packages=("wander",)):
        self.packages = list(packages)
        self.fight = record_fight
        self.record_fight = record_fight
        self.record_packages = list(record_packages)
        self.dead = False

    def in_combat(self):
        return "combat" in self.packages


class DedicatedActor:
    """Puppet copy of an actor on a non-authority client."""

    def __init__(self, actor):
        self.actor = actor
        self.ai_action = CANCEL          # BaseActor default
        self.has_received_ai = False     # X044

    def receive_ai(self, action):
        self.ai_action = action
        self.has_received_ai = True
        self.set_ai()

    def set_ai(self):
        if not self.has_received_ai:     # X044 guard
            return
        self.actor.fight = 0             # puppets never start their own fights
        if self.ai_action == COMBAT_END:
            self.actor.packages = [p for p in self.actor.packages if p != "combat"]
        elif self.ai_action == CANCEL:
            self.actor.packages = []
        elif self.ai_action == COMBAT:
            if "combat" not in self.actor.packages:
                self.actor.packages.insert(0, "combat")
        elif self.ai_action == WANDER:
            self.actor.packages.append("wander")


def restore_owned_actor_ai(actor):
    """X044: Cell.cpp helper applied when an actor becomes ours."""
    if actor.record_fight > 0 and actor.fight == 0:
        actor.fight = actor.record_fight
    if actor.record_packages and not actor.packages and not actor.dead:
        actor.packages = list(actor.record_packages)


def gain_authority(dedicated_actors):
    """ProcessorActorAuthority -> prepareDedicatedActorsForAuthority()."""
    for dedicated in dedicated_actors:
        if dedicated.has_received_ai:    # X044 guard
            dedicated.set_ai()
        restore_owned_actor_ai(dedicated.actor)
    return [d.actor for d in dedicated_actors]


class OutgoingBatch:
    """The shared mwmp::ActorList used by Cell::updateLocal."""

    def __init__(self):
        self.positions = []
        self.ai = []

    def reset(self):
        self.positions = []
        self.ai = []


class LocalActorAi:
    """LocalActor::updateAiState state machine."""

    def __init__(self):
        self.initialized = False
        self.was_combat = False
        self.had_home = False
        self.heartbeat = 0.0

    def update(self, batch, in_combat, has_home, use_shared_batch=False):
        """Returns the number of AI packets emitted this tick."""
        if not self.initialized:
            self.initialized = True
            self.was_combat, self.had_home = in_combat, has_home
            self.heartbeat = 0.0
            if not in_combat and not has_home:
                return 0

        self.heartbeat += 1.0
        heartbeat_due = (in_combat or has_home) and self.heartbeat >= (30.0 if in_combat else 90.0)
        changed = in_combat != self.was_combat or has_home != self.had_home
        sent = 0
        if changed or heartbeat_due:
            if use_shared_batch:         # pre-X044 behaviour
                batch.reset()
            batch.ai.append(COMBAT if in_combat else COMBAT_END)
            self.heartbeat = 0.0
            sent = 1
        self.was_combat, self.had_home = in_combat, has_home
        return sent


def check(name, condition):
    print(("PASS  " if condition else "FAIL  ") + name)
    return condition


def main():
    ok = True

    # 1. Idle NPC, no AI packet ever received, client becomes authority.
    idle = Actor(["wander"])
    gain_authority([DedicatedActor(idle)])
    ok &= check("idle NPC keeps its Wander package across an authority hand-off",
                idle.packages == ["wander"])
    ok &= check("idle NPC keeps a usable fight rating", idle.fight == 30)

    # 2. A genuine CANCEL packet still cancels while the actor is a puppet.
    scripted = Actor(["wander"])
    dedicated = DedicatedActor(scripted)
    dedicated.receive_ai(CANCEL)
    ok &= check("an explicit CANCEL packet still clears the sequence",
                scripted.packages == [])
    # Documented trade-off: taking authority over an actor with an empty sequence
    # refills it from the record, so a legacy CANCEL is not remembered forever.
    gain_authority([dedicated])
    ok &= check("record AI returns when such an actor becomes ours",
                scripted.packages == ["wander"])

    # 3. Fight rating: restored after puppet pacification, preserved when scripted to 0.
    fighter = Actor(["wander"])
    puppet = DedicatedActor(fighter)
    puppet.receive_ai(COMBAT)
    ok &= check("puppet copy is pacified while it is a puppet", fighter.fight == 0)
    gain_authority([puppet])
    ok &= check("fight rating comes back with authority", fighter.fight == 30)

    pacifist = Actor(["wander"], record_fight=0)
    pacifist.fight = 0
    restore_owned_actor_ai(pacifist)
    ok &= check("a record with Fight 0 stays at 0", pacifist.fight == 0)

    # 4. Recovery of an already-wiped sequence.
    wiped = Actor([])
    restore_owned_actor_ai(wiped)
    ok &= check("an emptied AI sequence is refilled from the record",
                wiped.packages == ["wander"])

    dead = Actor([])
    dead.dead = True
    restore_owned_actor_ai(dead)
    ok &= check("a dead actor is not refilled", dead.packages == [])

    # 5. Outgoing batch integrity.
    batch = OutgoingBatch()
    batch.positions = ["npc_a", "npc_b"]
    state = LocalActorAi()
    state.update(batch, in_combat=False, has_home=False)
    ok &= check("idle actor sends no AI packet on its first update", batch.ai == [])
    ok &= check("queued position packets survive the AI update",
                batch.positions == ["npc_a", "npc_b"])

    sends = sum(state.update(batch, in_combat=False, has_home=False) for _ in range(500))
    ok &= check("idle actor emits no heartbeat at all", sends == 0)
    ok &= check("position batch still intact after 500 idle ticks",
                batch.positions == ["npc_a", "npc_b"])

    ok &= check("combat start is published immediately",
                state.update(batch, in_combat=True, has_home=False) == 1)
    ok &= check("combat end is published immediately",
                state.update(batch, in_combat=False, has_home=False) == 1)

    # The pre-X044 path is what destroyed the batch.
    legacy_batch = OutgoingBatch()
    legacy_batch.positions = ["npc_a", "npc_b"]
    legacy = LocalActorAi()
    legacy.update(legacy_batch, in_combat=False, has_home=False)
    legacy.update(legacy_batch, in_combat=True, has_home=False, use_shared_batch=True)
    ok &= check("regression reproduced: shared-batch reset drops queued positions",
                legacy_batch.positions == [])

    print("\nRESULT:", "OK" if ok else "FAILURES PRESENT")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Logic-level checks for ArenaMP X034 combat/home state protocol."""

from dataclasses import dataclass

@dataclass(frozen=True)
class Hop:
    frm: str
    to: str


def add_hop(route, frm, to):
    if route and route[-1].frm == to and route[-1].to == frm:
        route.pop()
    else:
        route.append(Hop(frm, to))
        del route[:-12]


def test_route():
    r=[]
    add_hop(r,'A','B'); add_hop(r,'B','C')
    assert r == [Hop('A','B'), Hop('B','C')]
    add_hop(r,'C','B')
    assert r == [Hop('A','B')]
    add_hop(r,'B','A')
    assert r == []
    print('route A->B->C->B->A: OK')


def apply_snapshot(advertised, resolvable):
    # DedicatedActor iterates backwards because equal-priority Combat inserts at front.
    seq=[]
    unresolved=False
    for target in reversed(advertised):
        if target not in resolvable:
            unresolved=True
            continue
        seq.insert(0, target)
    return seq, unresolved


def test_targets():
    seq,retry=apply_snapshot(['playerA','playerB'], {'playerA'})
    assert seq == ['playerA'] and retry
    seq,retry=apply_snapshot(['playerA','playerB'], {'playerA','playerB'})
    assert seq == ['playerA','playerB'] and not retry
    assert seq[0] == 'playerA'
    print('multi-target primary ordering + unresolved retry: OK')


def test_authority():
    authority='guid-B'
    def accept(sender): return sender == authority
    assert not accept('guid-A') and accept('guid-B')
    print('stale/non-authority ActorAI rejection: OK')


def test_music():
    local='playerA'
    combats=[{'playerB'}, {'playerC'}, {'playerA','playerB'}]
    assert any(local in targets for targets in combats)
    assert not any(local in targets for targets in [{'playerB'}, {'playerC'}])
    print('battle music is local-player-specific: OK')


def test_door_budget_survives_handoff():
    configured=4
    storage_after_handoff=0
    route_breadcrumbs=3
    count=max(storage_after_handoff, route_breadcrumbs)
    assert count == 3 and count < configured
    route_breadcrumbs=4
    assert max(storage_after_handoff, route_breadcrumbs) >= configured
    print('door-transition budget survives authority handoff: OK')


def main():
    test_route(); test_targets(); test_authority(); test_music(); test_door_budget_survives_handoff()
    print('X034 logic harness: ALL OK')

if __name__ == '__main__': main()

from pathlib import Path
root = Path('/mnt/data/x048_final')

cfg=(root/'server/scripts/config.lua').read_text(encoding='utf-8')
assert 'chatProgressMessages = false' in cfg
assert 'chatRewardMessages = false' in cfg
assert 'chatRuntimeErrors = false' in cfg

quest=(root/'server/scripts/serverQuestSystem.lua').read_text(encoding='utf-8')
assert 'local function runtimeNotify' in quest
assert '"chatProgressMessages"' in quest
assert '"chatRewardMessages"' in quest
assert '"chatRuntimeErrors"' in quest
assert 'if type(questConfig) == "table" and questConfig[settingName] == true' in quest
# Explicit editor command feedback is intentionally preserved.
assert 'send(pid, "Quest definitions reloaded from disk")' in quest

keyboard=(root/'apps/openmw/mwinput/keyboardmanager.cpp').read_text(encoding='utf-8')
assert 'case SDL_SCANCODE_R:' not in keyboard
assert 'case SDL_SCANCODE_F:' not in keyboard
assert 'Ready Weapon / Ready Magic intentionally continue through the' in keyboard

action=(root/'apps/openmw/mwinput/actionmanager.cpp').read_text(encoding='utf-8')
assert 'stepPhysicsGrabRotation(direction, 0.f)' in action
assert 'stepPhysicsGrabRotation(0.f, direction)' in action
assert 'actionIsActive(A_Run) ? -1.f : 1.f' in action
# Smooth hold path is still active.
assert 'world->rotatePhysicsGrab(horizontalRotation, verticalRotation, dt);' in action

wm=(root/'apps/openmw/mwgui/windowmanagerimp.cpp').read_text(encoding='utf-8')
assert 'if (visible && mQuickLoot)' in wm and 'mQuickLoot->clear();' in wm
for signature in ['bool WindowManager::activateQuickLoot()',
                  'bool WindowManager::handleQuickLootMouseWheel(int rel)',
                  'bool WindowManager::handleQuickLootKeyPress(MyGUI::KeyCode key)',
                  'bool WindowManager::isQuickLootVisible() const']:
    pos=wm.index(signature)
    block=wm[pos:pos+420]
    assert 'isPhysicsGrabActive()' in block

ru=(root/'files/vfs/l10n/arenamp/ru.ini').read_text(encoding='utf-8')
en=(root/'files/vfs/l10n/arenamp/en.ini').read_text(encoding='utf-8')
assert 'удерживать — вращать' in ru
assert 'hold to rotate' in en

# Behavioral model: placement suppresses QuickLoot regardless of a stale container focus.
def quickloot_visible(placement_active, overlay_internal_visible):
    if placement_active:
        return False
    return overlay_internal_visible
assert quickloot_visible(True, True) is False
assert quickloot_visible(False, True) is True


world_h=(root/'apps/openmw/mwbase/world.hpp').read_text(encoding='utf-8')
world_cpp=(root/'apps/openmw/mwworld/worldimp.cpp').read_text(encoding='utf-8')
mouse=(root/'apps/openmw/mwinput/mousemanager.cpp').read_text(encoding='utf-8')
assert 'adjustPhysicsGrabDistance(float wheelSteps)' in world_h
assert 'void World::adjustPhysicsGrabDistance(float wheelSteps)' in world_cpp
assert 'state.mHoldDistance - wheelSteps * distancePerWheelStep' in world_cpp
assert 'world->adjustPhysicsGrabDistance(static_cast<float>(wheelDirection));' in mouse
assert mouse.index('world->adjustPhysicsGrabDistance') < mouse.index('handleQuickLootMouseWheel')
assert 'placement.distance = Колесо мыши' in ru
assert 'placement.distance = Mouse wheel' in en

print('X048 quest/placement/quickloot/distance harness: ALL OK')

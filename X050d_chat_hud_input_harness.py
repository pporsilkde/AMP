from pathlib import Path
from collections import defaultdict
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parent
cpp = (root/'apps/openmw/mwmp/GUI/GUIChat.cpp').read_text()
hpp = (root/'apps/openmw/mwmp/GUI/GUIChat.hpp').read_text()
guihpp = (root/'apps/openmw/mwmp/GUIController.hpp').read_text()
guicpp = (root/'apps/openmw/mwmp/GUIController.cpp').read_text()
layout = (root/'files/mygui/tes3mp_chat.layout').read_text()
skin = (root/'files/mygui/tes3mp_chat.skin.xml').read_text()
defaults = (root/'files/settings-default.cfg').read_text()
tes3 = (root/'files/tes3mp/tes3mp-client-default.cfg').read_text()

ET.parse(root/'files/mygui/tes3mp_chat.layout')
ET.parse(root/'files/mygui/tes3mp_chat.skin.xml')

for token in [
    'constexpr int sHudX = 1;', 'constexpr int sHudY = 25;',
    'constexpr int sHudWidth = 260;', 'constexpr int sHudHeight = 400;',
    'constexpr const char* sHudFont = "Russo";',
    'constexpr const char* sMenuFont = "DejaVuLGCSansMono";',
    'applyPanelGeometry(view.width, view.height);',
    'applyHudGeometry(view.width, view.height);',
    'GUIController::GM_ARENAMP_PlayerMenu',
    'windowManager->pushGuiMode(playerMenuMode)',
    'windowManager->removeGuiMode(playerMenuMode)',
    'mPanelBackground->setAlpha(sMenuBackgroundAlpha)',
]:
    assert token in cpp, token

assert 'MyGUI::IntCoord panelCoord;' in hpp
assert 'GM_ARENAMP_PlayerMenu' in guihpp and 'case GM_ARENAMP_PlayerMenu:' in guicpp
assert 'skin="FullBlackBG"' in layout
assert 'name="PanelBorder"' in layout and 'skin="MW_Box"' in layout
assert '<Property key="MinSize" value="40 40"/>' in layout
assert '<Property key="FontName" value="Russo"/>' in layout
assert 'FontName" value="Russo"' in skin

# The user-supplied tes3mp client cfg must not receive X049 state keys.
for forbidden in ['send channel =', 'send style =', 'rp mode =', 'stay after send =', 'player menu tab =', 'panel layout version =', 'font =']:
    assert forbidden not in tes3, forbidden
for required in ['x = 20', 'y = 40', 'w = 620', 'h = 420']:
    assert required in tes3, required

# Player-menu keys live in one default source only.
for required in ['send channel = default', 'send style = plain', 'rp mode = false', 'stay after send = false', 'player menu tab = 0']:
    assert required in defaults, required

# Check duplicate keys inside each cfg and duplicate Chat keys across both cfgs.
def parse_cfg(text):
    sec = ''
    out = []
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        if line.startswith('[') and line.endswith(']'):
            sec = line[1:-1]
        elif '=' in line:
            out.append((sec, line.split('=', 1)[0].strip()))
    return out

for name, text in [('settings-default.cfg', defaults), ('tes3mp-client-default.cfg', tes3)]:
    keys = parse_cfg(text)
    assert len(keys) == len(set(keys)), f'duplicate key in {name}'

left = set(parse_cfg(defaults))
right = set(parse_cfg(tes3))
assert not {item for item in left & right if item[0] == 'Chat'}, 'cross-file duplicate [Chat] keys'

print('X050d chat HUD/input harness: OK')

from pathlib import Path
from PIL import Image
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
failures = []

def check(name, cond):
    print(('ok   ' if cond else 'FAIL ') + name)
    if not cond:
        failures.append(name)

# StopCombat periodic spam is opt-in only.
group = (ROOT / 'server/scripts/groupHelper.lua').read_text(encoding='utf-8')
cfg = (ROOT / 'server/scripts/config.lua').read_text(encoding='utf-8')
check('legacy summon sweep defaults off', '["summon legacy stopcombat tick"] = false' in cfg)
check('post-init starts sweep only when opted in', 'cfg.protectSummons and cfg.summonLegacyStopCombatTick' in group)
check('reactive OnObjectHit backstop remains', 'registerValidator("OnObjectHit"' in group and 'stopSummonCombat' in group)

# Emoji atlas is now the default and has every glyph used by GUIChat.
settings = (ROOT / 'files/settings-default.cfg').read_text(encoding='utf-8')
check('menu color font default', 'menu font = ArenaMPChatColor' in settings)
check('emoji color font default', 'emoji font = ArenaMPChatColor' in settings)
check('blank legacy settings upgraded in C++', 'emojiFontName = sColorChatFont' in (ROOT / 'apps/openmw/mwmp/GUI/GUIChat.cpp').read_text(encoding='utf-8'))

atlas = ROOT / 'files/mygui/ArenaMPChatColor.png'
try:
    im = Image.open(atlas)
    im.verify()
    png_ok = True
except Exception:
    png_ok = False
check('emoji atlas png valid', png_ok)

tree = ET.parse(ROOT / 'files/mygui/ArenaMPChatColor.xml')
indices = set()
for code in tree.findall('.//Code'):
    try:
        indices.add(int(code.attrib['index']))
    except (ValueError, KeyError):
        pass
palette = [0x263A,0x2639,0x1F60A,0x1F609,0x1F60B,0x1F602,0x1F622,0x1F621,
           0x2665,0x1F494,0x1F44D,0x1F44E,0x2714,0x274C,0x2B50,0x2694,
           0x1F37A,0x1F525,0x1F3B5,0x1F4A4]
check('all 20 emoji glyphs present', all(cp in indices for cp in palette))

# Theme resources.
qrc = ET.parse(ROOT / 'files/launcher/launcher.qrc')
theme_files = {f.text for f in qrc.findall('.//qresource[@prefix="theme"]/file')}
expected = {'theme/arenamp-launcher.qss','theme/arenamp-bg.png','theme/arenamp-panel.png',
            'theme/arenamp-button-up.png','theme/arenamp-button-down.png','theme/arenamp-divider.png'}
check('launcher theme resources registered', expected.issubset(theme_files))
main = (ROOT / 'apps/launcher/main.cpp').read_text(encoding='utf-8')
check('launcher forces deterministic Fusion base', 'QStyleFactory::create(QStringLiteral("Fusion"))' in main)
check('launcher applies global QApplication stylesheet', 'app.setStyleSheet(QString::fromUtf8(arenaTheme.readAll()))' in main)

print(f'\n{len(failures)} failures')
raise SystemExit(1 if failures else 0)

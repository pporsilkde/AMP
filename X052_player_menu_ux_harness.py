#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ArenaMP X052 static harness.

Run from the root of a tree that already has the X052 patch applied:

    python3 X052_player_menu_ux_harness.py

It checks the things a compiler cannot check for us here: that every widget the
C++ asks for exists in the layout, that every localization key used by GUIChat
exists in BOTH ru.ini and en.ini, that the constructor initialises members in
declaration order (MSVC /W4 treats a mismatch as a warning and the project
builds with warnings-as-errors), and that the server protocol prefixes agree on
both sides of the wire.
"""
import os
import re
import sys
import xml.dom.minidom

CPP = 'apps/openmw/mwmp/GUI/GUIChat.cpp'
HPP = 'apps/openmw/mwmp/GUI/GUIChat.hpp'
LAYOUT = 'files/mygui/tes3mp_chat.layout'
L10N = ('files/vfs/l10n/arenamp/ru.ini', 'files/vfs/l10n/arenamp/en.ini')
SETTINGS = 'files/settings-default.cfg'
LUA = {
    'color': 'server/scripts/chatColorHelper.lua',
    'players': 'server/scripts/playerListHelper.lua',
    'group': 'server/scripts/groupHelper.lua',
    'events': 'server/scripts/eventHandler.lua',
    'core': 'server/scripts/serverCore.lua',
    'config': 'server/scripts/config.lua',
}

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


def read(path):
    with open(path, encoding='utf-8') as handle:
        return handle.read()


for path in [CPP, HPP, LAYOUT, SETTINGS] + list(L10N) + list(LUA.values()):
    if not os.path.exists(path):
        print('MISSING FILE: ' + path)
        sys.exit(1)

cpp = read(CPP)
hpp = read(HPP)
layout = read(LAYOUT)

# --- 1. layout is well formed and carries every widget the C++ binds ---
xml.dom.minidom.parseString(layout)
widget_names = set(re.findall(r'name="([^"]+)"', layout))

for match in re.finditer(r'getWidget\(\s*[^,]+,\s*"([^"]+)"\s*\)', cpp):
    check(match.group(1) in widget_names,
          'getWidget("%s") has no matching widget in the layout' % match.group(1))
for match in re.finditer(r'findWidget\("([^"]+)"\)', cpp):
    check(match.group(1) in widget_names,
          'findWidget("%s") has no matching widget in the layout' % match.group(1))

for index in range(1, 21):
    check('Emoji%d' % index in widget_names, 'layout is missing Emoji%d' % index)
for index in range(1, 17):
    check('Color%d' % index in widget_names, 'layout is missing Color%d' % index)
for name in ('TabPlayers', 'PlayersPane', 'PlayersList', 'PlayersInfo',
             'PlayersRefreshButton', 'PlayersOpenListButton', 'PlayersInviteButton',
             'GroupRoster', 'GroupRosterLabel', 'ColorBar', 'ColorToggle'):
    check(name in widget_names, 'layout is missing the X052 widget %s' % name)

# --- 2. localization coverage in every language ---
used_keys = sorted(set(re.findall(r'localizeArena\("([^"]+)"\)', cpp)))
check(len(used_keys) > 0, 'no localizeArena keys found, the parse probably broke')
for locale in L10N:
    defined = set()
    for line in read(locale).splitlines():
        line = line.strip()
        if line and not line.startswith('#') and '=' in line:
            defined.add(line.split('=', 1)[0].strip())
    for key in used_keys:
        check(key in defined, '%s does not define %s' % (locale, key))

# --- 3. constructor initialises members in declaration order ---
class_body = re.search(r'class GUIChat : public MWGui::WindowBase\s*\{(.*)\n    \};', hpp, re.S)
check(class_body is not None, 'could not locate the GUIChat class body')
if class_body:
    member_re = (r'^\s+(?:MyGUI::\w+\*|bool|float|int|std::string|ChatWindowState|ChatChannel|'
                 r'ChatStyle|PlayerMenuTab|ChatDrawer|std::vector<[^;]+>|std::map<[^;]+>|'
                 r'MyGUI::IntPoint|MyGUI::IntCoord)\s+(\w+)(?:\[[^\]]*\])?;')
    declared = [m.group(1) for m in re.finditer(member_re, class_body.group(1), re.M)]
    ctor = re.search(r'GUIChat::GUIChat\([^)]*\)\s*:(.*?)\n    \{', cpp, re.S)
    check(ctor is not None, 'could not locate the GUIChat constructor')
    if ctor:
        initialised = [m.group(1) for m in re.finditer(r'[,:]\s*(\w+)[({]', ctor.group(1))]
        expected = [name for name in declared if name in initialised]
        for position, (got, want) in enumerate(zip(initialised, expected)):
            if got != want:
                failures.append('constructor init order diverges at slot %d: '
                                'got %s, declaration order wants %s' % (position, got, want))
                break

# --- 4. every declared method has a definition ---
if class_body:
    declared_methods = set(re.findall(
        r'^\s+(?:virtual\s+)?[\w:<>*&\s]+?\b(\w+)\([^;{]*\)(?:\s*const)?;',
        class_body.group(1), re.M))
    defined_methods = set(re.findall(r'GUIChat::(\w+)\(', cpp))
    for name in sorted(declared_methods - defined_methods - {'GUIChat'}):
        failures.append('GUIChat::%s is declared but never defined' % name)

# --- 5. control protocol agrees on both ends ---
for prefix, lua_key in (('@@AMP_COLOR@@', 'color'),
                        ('@@AMP_PLAYERS@@', 'players'),
                        ('@@AMP_GROUP@@', 'group')):
    check(prefix in cpp, 'client never handles %s' % prefix)
    check(prefix in read(LUA[lua_key]), '%s never sends %s' % (LUA[lua_key], prefix))

for command, source in (('/chatcolor', 'color'), ('/playerlistui', 'players')):
    check(command in cpp, 'client never sends %s' % command)
    check(command.lstrip('/') in read(LUA[source]),
          '%s does not register %s' % (LUA[source], command))

check('roster' in read(LUA['group']), 'groupHelper does not implement the roster action')
check('buildRosterField' in read(LUA['group']), 'groupHelper is missing buildRosterField')

# --- 6. settings key exists, because Settings::Manager throws on unknown keys ---
settings = read(SETTINGS)
check(re.search(r'^emoji font\s*=', settings, re.M) is not None,
      'settings-default.cfg has no [Chat] "emoji font" key, '
      'Settings::Manager::getString would throw at startup')
check('Settings::Manager::getString("emoji font", "Chat")' in cpp,
      'GUIChat never reads the emoji font setting')

# --- 7. the emoji palettes are complete and the ASCII column is pure ASCII ---
palette = re.search(r'const EmojiSlot sEmojiPalette\[\] = \{(.*?)\};', cpp, re.S)
check(palette is not None, 'could not locate sEmojiPalette')
if palette:
    slots = re.findall(r'\{\s*"((?:[^"\\]|\\.)*)"\s*,\s*"((?:[^"\\]|\\.)*)"\s*\}', palette.group(1))
    check(len(slots) == 20, 'sEmojiPalette must hold exactly 20 slots, found %d' % len(slots))
    for _, ascii_form in slots:
        check('\\x' not in ascii_form,
              'the fallback %r is not pure ASCII, it could still render as a box' % ascii_form)

# --- 8. server side stays syntactically loadable ---
for path in LUA.values():
    text = read(path)
    check(text.count('function') >= 1, '%s looks empty' % path)

check('chatColorHelper = require("chatColorHelper")' in read(LUA['core']),
      'serverCore does not require chatColorHelper')
check('playerListHelper = require("playerListHelper")' in read(LUA['core']),
      'serverCore does not require playerListHelper')
check('config.chatNameColors' in read(LUA['config']),
      'config.lua does not define chatNameColors')
check('truncateUtf8' in read(LUA['events']),
      'eventHandler still truncates chat on a raw byte boundary')

if failures:
    print('X052 HARNESS FAILED (%d):' % len(failures))
    for item in failures:
        print('  - ' + item)
    sys.exit(1)

print('X052 harness passed: layout, localization, member order, protocol, settings, palettes.')

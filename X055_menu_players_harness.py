from pathlib import Path

# Geometry copied from GUIChat.cpp X055.
main_width = 800
side_margin = 10
inner = main_width - side_margin * 2
grid_width = inner - 12
columns = 20
gap = 4
minimum_button_width = 34
cell_width = max(minimum_button_width, (grid_width - (columns - 1) * gap) // columns)
last_right = 6 + (columns - 1) * (cell_width + gap) + cell_width
assert last_right <= inner, (last_right, inner)

core = Path('server/scripts/coreChat.lua').read_text(encoding='utf-8')
assert 'message:sub(1, 6) == "@@AMP_"' in core
players = Path('server/scripts/playerListHelper.lua').read_text(encoding='utf-8')
assert 'OnPlayerAuthentified' in players and 'playerListHelper.SendState(pid)' in players
handler = Path('server/scripts/eventHandler.lua').read_text(encoding='utf-8')
assert 'getArenaChatInstructions(pid)' in handler
assert 'chat_instructions' not in handler

cfg = Path('files/tes3mp/tes3mp-client-default.cfg').read_text(encoding='utf-8').splitlines()
section = None
seen = set()
for line in cfg:
    s = line.strip()
    if s.startswith('[') and s.endswith(']'):
        section = s[1:-1]
    elif section == 'Chat' and '=' in s and not s.startswith('#'):
        key = s.split('=', 1)[0].strip().lower()
        assert key not in seen, key
        seen.add(key)
assert 'w' in seen and 'h' in seen
print('X055 harness: OK')

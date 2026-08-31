#!/usr/bin/env python3
"""Static/regression checks for ArenaMP X049 player-menu chat patch."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent


def require(path, needle):
    text = (ROOT / path).read_text(encoding="utf-8")
    assert needle in text, f"missing {needle!r} in {path}"
    return text


def main():
    layout_path = ROOT / "files/mygui/tes3mp_chat.layout"
    skin_path = ROOT / "files/mygui/tes3mp_chat.skin.xml"
    ET.parse(layout_path)
    ET.parse(skin_path)

    cpp = require("apps/openmw/mwmp/GUI/GUIChat.cpp", "changeInputMode(true)")
    require("apps/openmw/mwmp/GUI/GUIChat.cpp", "persistGeometry()")
    require("apps/openmw/mwmp/GUI/GUIChat.cpp", 'return "/ampchat "')
    require("apps/openmw/mwmp/GUI/GUIChat.cpp", "setStayOpenAfterSend")
    require("apps/openmw/mwmp/GUI/GUIChat.cpp", "onDrag")

    layout = layout_path.read_text(encoding="utf-8")
    widget_names = re.findall(r'getWidget\([^,]+, "([^"]+)"\)', cpp)
    missing = [name for name in widget_names if f'name="{name}"' not in layout]
    assert not missing, f"C++ requests widgets absent from layout: {missing}"

    server = require("server/scripts/eventHandler.lua", "processArenaStructuredChat")
    for token in ('scope ~= "default"', 'scope ~= "local"', 'scope ~= "global"',
                  'style ~= "plain"', 'style ~= "me"', 'style ~= "do"'):
        assert token in server
    assert "config.localChatRadius" in server
    assert "tes3mp.GetCell(originPid) ~= tes3mp.GetCell(targetPid)" in server
    assert "dx * dx + dy * dy + dz * dz <= radius * radius" in server

    cfg = require("server/scripts/config.lua", "config.localChatRadius = 5000")
    assert "localChatRadius" in cfg

    for lang in ("en.ini", "ru.ini"):
        text = (ROOT / "files/vfs/l10n/arenamp" / lang).read_text(encoding="utf-8")
        for key in (
            "chat.menu.title", "chat.tab.chat", "chat.tab.group", "chat.tab.home",
            "chat.return_game", "chat.channel.default", "chat.channel.local",
            "chat.channel.global", "chat.style.plain", "chat.emoji",
            "chat.stay_after_send", "chat.send", "chat.group.placeholder",
            "chat.home.placeholder",
        ):
            assert f"{key} =" in text, f"missing {key} in {lang}"

    settings = require("files/settings-default.cfg", "send channel = default")
    for needle in (
        "send style = plain", "rp mode = false", "stay after send = false",
        "panel layout version = 1", "font = DejaVuLGCSansMono",
    ):
        assert needle in settings

    # Important compatibility property: ordinary text is still sent raw and user
    # slash commands bypass /ampchat, so existing servers/commands keep semantics.
    assert "if (!text.empty() && text[0] == '/')" in cpp
    assert "chatChannel == CHANNEL_DEFAULT && chatStyle == STYLE_PLAIN && !rpMode" in cpp

    print("X049 player-menu chat harness: OK")


if __name__ == "__main__":
    main()

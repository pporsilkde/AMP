// ArenaMP X054 - client chat logic harness.
//
// Reproduces three pieces of GUIChat/GUIController logic with no MyGUI, no SDL
// and no OpenMW, so the rules can be checked without a full client build:
//
//   [A] Say-key tap/hold arbitration       (task 1)
//   [B] control-field split/unescape order (task 6, the "no separator" bug)
//   [C] outgoing message routing           (task 3, /me /do // ///)
//
// Build: g++ -std=c++17 -O0 -o x054 X054_chat_logic_harness.cpp && ./x054

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
    int gChecks = 0;
    int gFailures = 0;

    void check(const char* name, bool condition, const std::string& detail = std::string())
    {
        ++gChecks;
        if (condition)
            std::printf("  ok    %s\n", name);
        else
        {
            ++gFailures;
            std::printf("  FAIL  %s%s%s\n", name,
                detail.empty() ? "" : "  <- ", detail.c_str());
        }
    }
}

// ---------------------------------------------------------------------------
// [A] Say-key tap/hold arbitration.
//
// Mirrors GUIController::pressedKey + updateSayKeyHold and the two GUIChat
// state flags they drive. A tap must leave the chat in the compact HUD state
// (editState, no menuState, no GUI mode); a hold must promote it.
// ---------------------------------------------------------------------------
namespace tapHold
{
    struct Chat
    {
        bool editState = false;
        bool menuState = false;
        bool sayKeyHeld = false;
        std::string commandLine;

        // GUIChat::setEditState - leaving input always drops the menu tier too.
        void setEditState(bool state)
        {
            editState = state;
            if (!editState)
            {
                menuState = false;
                commandLine.clear();
            }
        }

        void setMenuState(bool state)
        {
            if (!editState)
                state = false;
            menuState = state;
        }

        void pressedSay() { setEditState(true); }

        void openPlayerMenu()
        {
            if (!editState)
                setEditState(true);
            setMenuState(true);
        }

        // GUIChat::commandTextChanged - drop anything the held key typed.
        void typeText(const std::string& text)
        {
            commandLine += text;
            if (sayKeyHeld && !commandLine.empty())
                commandLine.clear();
        }

        // GUIChat::syncInteractiveInputMode - only the expanded menu takes the
        // cursor and releases mouse-look.
        bool guiModePushed() const { return editState && menuState; }
    };

    struct Controller
    {
        Chat chat;
        bool sayHoldArmed = false;
        bool sayHoldTriggered = false;
        float sayHoldTime = 0.f;
        float sayHoldThreshold = 0.35f;
        bool keyDown = false;

        // Returns true when the press was consumed by the chat.
        bool pressSayKey()
        {
            keyDown = true;
            if (chat.editState)
                return false; // the caret owns the key now: it types a letter

            if (sayHoldThreshold <= 0.f)
            {
                chat.openPlayerMenu();
                return true;
            }

            chat.pressedSay();
            chat.sayKeyHeld = true;
            sayHoldArmed = true;
            sayHoldTriggered = false;
            sayHoldTime = 0.f;
            return true;
        }

        void releaseSayKey() { keyDown = false; }

        void update(float dt)
        {
            const bool down = sayHoldArmed && keyDown;
            if (!down)
            {
                if (sayHoldArmed)
                    chat.sayKeyHeld = false;
                sayHoldArmed = false;
                sayHoldTriggered = false;
                sayHoldTime = 0.f;
                return;
            }

            if (sayHoldTriggered)
                return;

            sayHoldTime += std::max(0.f, dt);
            if (sayHoldTime >= sayHoldThreshold)
            {
                sayHoldTriggered = true;
                chat.openPlayerMenu();
            }
        }
    };

    void run()
    {
        std::printf("\n[A] Say-key tap/hold arbitration\n");

        {
            // A short tap: pressed and released inside one frame.
            Controller c;
            check("tap: press is consumed", c.pressSayKey());
            c.releaseSayKey();
            c.update(1.f / 60.f);
            check("tap: caret is in the chat", c.chat.editState);
            check("tap: player menu stays closed", !c.chat.menuState);
            check("tap: no GUI mode, mouse-look is untouched", !c.chat.guiModePushed());
            check("tap: hold timer disarmed", !c.sayHoldArmed);
        }

        {
            // Held past the threshold.
            Controller c;
            c.pressSayKey();
            for (int frame = 0; frame < 10; ++frame)
                c.update(1.f / 60.f); // 166 ms - still under 350 ms
            check("hold: menu still closed before the threshold", !c.chat.menuState);
            for (int frame = 0; frame < 20; ++frame)
                c.update(1.f / 60.f); // now well past it
            check("hold: player menu opened", c.chat.menuState);
            check("hold: GUI mode is pushed", c.chat.guiModePushed());
            check("hold: chat is still in input state", c.chat.editState);
        }

        {
            // The menu must open exactly once, no matter how long the key is held.
            Controller c;
            c.pressSayKey();
            int openings = 0;
            for (int frame = 0; frame < 120; ++frame)
            {
                const bool before = c.chat.menuState;
                c.update(1.f / 60.f);
                if (!before && c.chat.menuState)
                    ++openings;
            }
            check("hold: menu opens exactly once", openings == 1,
                "openings=" + std::to_string(openings));
        }

        {
            // SDL auto-repeat while the key is down must not leak characters.
            Controller c;
            c.pressSayKey();
            c.chat.typeText("y");
            c.update(1.f / 60.f);
            c.chat.typeText("y");
            check("hold: auto-repeat characters are discarded", c.chat.commandLine.empty(),
                "line='" + c.chat.commandLine + "'");
            c.releaseSayKey();
            c.update(1.f / 60.f);
            c.chat.typeText("y");
            check("release: normal typing resumes", c.chat.commandLine == "y",
                "line='" + c.chat.commandLine + "'");
        }

        {
            // Once the caret is in the chat, the Say key is an ordinary letter.
            Controller c;
            c.pressSayKey();
            c.releaseSayKey();
            c.update(1.f / 60.f);
            check("typing: a later press is not consumed", !c.pressSayKey());
            check("typing: the menu did not open", !c.chat.menuState);
        }

        {
            // Threshold 0 restores the X049 behaviour for anyone who preferred it.
            Controller c;
            c.sayHoldThreshold = 0.f;
            c.pressSayKey();
            check("threshold 0: a tap opens the full menu", c.chat.menuState);
        }

        {
            // Closing the chat must drop both tiers.
            Controller c;
            c.pressSayKey();
            for (int frame = 0; frame < 40; ++frame)
                c.update(1.f / 60.f);
            c.releaseSayKey();
            c.update(1.f / 60.f);
            c.chat.setEditState(false);
            check("close: menu tier dropped", !c.chat.menuState);
            check("close: GUI mode released", !c.chat.guiModePushed());
            c.pressSayKey();
            check("reopen: next tap is the compact HUD again", c.chat.editState && !c.chat.menuState);
        }
    }
}

// ---------------------------------------------------------------------------
// [B] Control-field split/unescape order.
//
// The X052 bug: handleServerControlMessage unescaped every tab-separated field
// before the record was split on ';' and '^'. The player-detail field is itself
// a list, so "\n" had already become a real newline by the time the splitter
// ran - and the splitter drops newlines, gluing "PID: 0" straight onto the next
// line. X054 unescapes on the leaves instead.
// ---------------------------------------------------------------------------
namespace controlFields
{
    std::vector<std::string> splitControlFields(const std::string& value, char delimiter)
    {
        std::vector<std::string> result;
        std::string current;
        for (char c : value)
        {
            if (c == delimiter)
            {
                result.push_back(current);
                current.clear();
            }
            else if (c != '\r' && c != '\n')
                current.push_back(c);
        }
        result.push_back(current);
        return result;
    }

    std::string unescapeControlField(const std::string& value)
    {
        std::string result;
        result.reserve(value.size());
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (value[i] != '\\' || i + 1 >= value.size())
            {
                result.push_back(value[i]);
                continue;
            }
            const char next = value[++i];
            if (next == 'n') result.push_back('\n');
            else if (next == 't') result.push_back('\t');
            else result.push_back(next);
        }
        return result;
    }

    // What the server sends: "\n" inside a record is escaped as a literal
    // backslash-n, records are joined with ';' and fields with '^'.
    const std::string sMessage =
        "STATE\tPlayers online: 2\t"
        "Aldar^[A][RP] PID 0 [ Aldar, Dunmer, 5 lvl ]\\nLocation: [ Balmora ]\\nRegion: [ West Gash ]^[A][RP] PID 0;"
        "Beris^[P][G] PID 1 [ Beris, Nord, 3 lvl ]\\nLocation: [ Seyda Neen ]^[P][G] PID 1";

    struct Parsed
    {
        std::string header;
        std::vector<std::string> names;
        std::vector<std::string> details;
    };

    // X052: unescape first, then split. This is the bug.
    Parsed parseOldWay(const std::string& message)
    {
        Parsed out;
        std::vector<std::string> fields = splitControlFields(message, '\t');
        for (std::string& field : fields)
            field = unescapeControlField(field);

        out.header = fields.size() > 1 ? fields[1] : std::string();
        const std::string body = fields.size() > 2 ? fields[2] : std::string();
        for (const std::string& entry : splitControlFields(body, ';'))
        {
            if (entry.empty())
                continue;
            const std::vector<std::string> parts = splitControlFields(entry, '^');
            if (parts.empty() || parts[0].empty())
                continue;
            out.names.push_back(parts[0]);
            out.details.push_back(parts.size() > 1 ? parts[1] : std::string());
        }
        return out;
    }

    // X054: split first, unescape the leaves.
    Parsed parseNewWay(const std::string& message)
    {
        Parsed out;
        const std::vector<std::string> fields = splitControlFields(message, '\t');

        out.header = fields.size() > 1 ? unescapeControlField(fields[1]) : std::string();
        const std::string body = fields.size() > 2 ? fields[2] : std::string();
        for (const std::string& entry : splitControlFields(body, ';'))
        {
            if (entry.empty())
                continue;
            const std::vector<std::string> parts = splitControlFields(entry, '^');
            if (parts.empty() || parts[0].empty())
                continue;
            out.names.push_back(unescapeControlField(parts[0]));
            out.details.push_back(parts.size() > 1 ? unescapeControlField(parts[1]) : std::string());
        }
        return out;
    }

    std::size_t countLines(const std::string& text)
    {
        return 1 + static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n'));
    }

    void run()
    {
        std::printf("\n[B] Control-field split/unescape order\n");

        const Parsed oldWay = parseOldWay(sMessage);
        const Parsed newWay = parseNewWay(sMessage);

        check("both parsers find the same two players",
            oldWay.names.size() == 2 && newWay.names.size() == 2);
        check("both parsers agree on the names",
            newWay.names[0] == "Aldar" && newWay.names[1] == "Beris");

        // The regression the user reported.
        check("X052: detail lines were glued together", countLines(oldWay.details[0]) == 1,
            "lines=" + std::to_string(countLines(oldWay.details[0])));
        check("X052: the glue is visible in the text",
            oldWay.details[0].find("]Location:") != std::string::npos, oldWay.details[0]);

        check("X054: the first card has three lines", countLines(newWay.details[0]) == 3,
            "lines=" + std::to_string(countLines(newWay.details[0])));
        check("X054: the second card has two lines", countLines(newWay.details[1]) == 2,
            "lines=" + std::to_string(countLines(newWay.details[1])));
        check("X054: no glued separator remains",
            newWay.details[0].find("]Location:") == std::string::npos, newWay.details[0]);
        check("X054: the header still unescapes", newWay.header == "Players online: 2", newWay.header);
        check("X054: line content is intact",
            newWay.details[0].find("Location: [ Balmora ]") != std::string::npos, newWay.details[0]);
        check("X054: region line survives",
            newWay.details[0].find("Region: [ West Gash ]") != std::string::npos, newWay.details[0]);

        // The same ordering rule applies to the group roster, where the bug was
        // latent because roster names rarely contain an escape.
        const std::string roster = "Aldar\\ncontinued^1^0^1;Beris^0^0^0";
        const std::vector<std::string> entries = splitControlFields(roster, ';');
        const std::vector<std::string> first = splitControlFields(entries[0], '^');
        check("roster: leaf unescape restores the newline",
            countLines(unescapeControlField(first[0])) == 2);
    }
}

// ---------------------------------------------------------------------------
// [C] Outgoing message routing.
//
// GUIChat::buildOutgoingMessage. The menu no longer speaks its own /ampchat
// dialect: it emits the command a player would type, so coreChat stays the one
// and only formatter.
// ---------------------------------------------------------------------------
namespace routing
{
    enum ChatChannel
    {
        CHANNEL_SAY = 0,
        CHANNEL_WHISPER,
        CHANNEL_SHOUT,
        CHANNEL_LOCAL_OOC,
        CHANNEL_GLOBAL_OOC
    };

    enum ChatStyle
    {
        STYLE_PLAIN = 0,
        STYLE_ME,
        STYLE_DO,
        STYLE_TRY
    };

    std::string buildOutgoingMessage(ChatChannel channel, ChatStyle style, const std::string& text)
    {
        if (!text.empty() && text[0] == '/')
            return text;

        switch (style)
        {
            case STYLE_ME: return "/me " + text;
            case STYLE_DO: return "/do " + text;
            case STYLE_TRY: return "/try " + text;
            default: break;
        }

        switch (channel)
        {
            case CHANNEL_WHISPER: return "/w " + text;
            case CHANNEL_SHOUT: return "/sh " + text;
            case CHANNEL_LOCAL_OOC: return "// " + text;
            case CHANNEL_GLOBAL_OOC: return "/// " + text;
            case CHANNEL_SAY:
            default: return "/s " + text;
        }
    }

    void run()
    {
        std::printf("\n[C] Outgoing message routing\n");

        check("plain + say -> /s",
            buildOutgoingMessage(CHANNEL_SAY, STYLE_PLAIN, "hello") == "/s hello");
        check("plain + whisper -> /w",
            buildOutgoingMessage(CHANNEL_WHISPER, STYLE_PLAIN, "hello") == "/w hello");
        check("plain + shout -> /sh",
            buildOutgoingMessage(CHANNEL_SHOUT, STYLE_PLAIN, "hello") == "/sh hello");
        check("plain + local OOC -> //",
            buildOutgoingMessage(CHANNEL_LOCAL_OOC, STYLE_PLAIN, "hello") == "// hello");
        check("plain + global OOC -> ///",
            buildOutgoingMessage(CHANNEL_GLOBAL_OOC, STYLE_PLAIN, "hello") == "/// hello");

        check("/me wins over the channel",
            buildOutgoingMessage(CHANNEL_GLOBAL_OOC, STYLE_ME, "waves") == "/me waves");
        check("/do wins over the channel",
            buildOutgoingMessage(CHANNEL_WHISPER, STYLE_DO, "the door creaks") == "/do the door creaks");
        check("/try wins over the channel",
            buildOutgoingMessage(CHANNEL_SAY, STYLE_TRY, "picks the lock") == "/try picks the lock");

        check("a typed slash command is passed through untouched",
            buildOutgoingMessage(CHANNEL_LOCAL_OOC, STYLE_ME, "/list") == "/list");
        check("a typed // is passed through untouched",
            buildOutgoingMessage(CHANNEL_SAY, STYLE_PLAIN, "//x") == "//x");

        check("no /ampchat envelope is produced anywhere",
            buildOutgoingMessage(CHANNEL_GLOBAL_OOC, STYLE_ME, "x").find("/ampchat") == std::string::npos);

        // Multi-line messages keep working: the command prefix is applied once.
        const std::string multi = buildOutgoingMessage(CHANNEL_SAY, STYLE_PLAIN, "line one\nline two");
        check("multi-line text keeps a single prefix", multi == "/s line one\nline two", multi);
    }
}

int main()
{
    std::printf("ArenaMP X054 client chat logic harness\n");
    tapHold::run();
    controlFields::run();
    routing::run();
    std::printf("\n%d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}

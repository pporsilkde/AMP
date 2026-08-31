-- ArenaMP X054: the /list roster, rendered inside the Player Menu.
--
-- X052 built its own reduced entry here ("PID: 0", "Cell: ...", raw race id).
-- X054 drops that and calls guiHelper.GetPlayerListEntry, which is the exact
-- formatter /list uses: rank + RP tag, translated race name, level, location,
-- region, ping, days on server, session length and the ghost marker. The two
-- views cannot disagree any more, because there is only one formatter.
--
-- Ghost privacy, bounty override and the admin-only ghost marker all come from
-- that shared function as well, so the tab inherits them for free.

local playerListHelper = {}

local STATE_PREFIX = "@@AMP_PLAYERS@@"

local function isValidPid(pid)
    return Players[pid] ~= nil and Players[pid]:IsLoggedIn() and Players[pid].data ~= nil
end

-- Backslash, tab and newline survive the trip as escape sequences; the client
-- resolves them only after it has split the record apart.
local function escapeField(value)
    value = tostring(value or "")
    value = value:gsub("\\", "\\\\")
    value = value:gsub("\t", "\\t")
    value = value:gsub("\r", "")
    value = value:gsub("\n", "\\n")
    return value
end

-- ';' separates records and '^' separates the fields inside one, and the split
-- happens before unescaping, so those two characters must never appear inside a
-- payload. Cell and player names in practice never contain them.
local function stripSeparators(value)
    return (tostring(value or ""):gsub("[;%^]", " "))
end

-- MyGUI ListBox items are plain text and do not resolve "#RRGGBB" tags, so the
-- left-hand column gets a colourless label while the detail card keeps the full
-- coloured block.
local function stripColorCodes(value)
    return (tostring(value or ""):gsub("#%x%x%x%x%x%x", ""))
end

function playerListHelper.SendState(pid)
    if not isValidPid(pid) then return end
    if type(guiHelper) ~= "table" or type(guiHelper.GetPlayerListEntry) ~= "function" then return end

    local entries = {}
    local count = 0
    local lastPid = tes3mp.GetLastPlayerId()
    for otherPid = 0, lastPid do
        local entry = guiHelper.GetPlayerListEntry(pid, otherPid)
        if entry ~= nil then
            count = count + 1

            local sameCell = tes3mp.GetCell(pid) == tes3mp.GetCell(otherPid)
            local label = stripColorCodes(entry.label)
            if sameCell and otherPid ~= pid then label = label .. " *" end

            entries[#entries + 1] = table.concat({
                escapeField(stripSeparators(entry.name)),
                escapeField(stripSeparators(entry.block)),
                escapeField(stripSeparators(label))
            }, "^")
        end
    end

    local header = localization.Get(pid, "coreChat", "players_online", {count = count})
    local fields = { "STATE", escapeField(stripSeparators(header)), table.concat(entries, ";") }
    tes3mp.SendMessage(pid, STATE_PREFIX .. table.concat(fields, "\t") .. "\n", false)
end

local function processCommand(pid, cmd)
    if not isValidPid(pid) then return end
    playerListHelper.SendState(pid)
end

customCommandHooks.registerCommand("playerlistui", processCommand)

return playerListHelper

-- ArenaMP X052: the /list roster, rendered inside the Player Menu.
--
-- The menu's "Players" tab asks for /playerlistui state and gets the same
-- information /list reports, encoded as a hidden control message. The tab also
-- carries a button that simply sends /list, so the original server dialog stays
-- reachable and the two views can never disagree about who is online.

local playerListHelper = {}

local STATE_PREFIX = "@@AMP_PLAYERS@@"

local function isValidPid(pid)
    return Players[pid] ~= nil and Players[pid]:IsLoggedIn() and Players[pid].data ~= nil
end

local function escapeField(value)
    value = tostring(value or "")
    value = value:gsub("\\", "\\\\")
    value = value:gsub("\t", "\\t")
    value = value:gsub("\r", "")
    value = value:gsub("\n", "\\n")
    return value
end

local function safeToken(value, maxLength)
    value = tostring(value or "")
    value = value:gsub("[%c\t]", " ")
    value = value:gsub("[;%^]", " ")
    value = value:gsub("%s+", " ")
    value = value:match("^%s*(.-)%s*$") or ""
    if maxLength ~= nil and #value > maxLength then value = value:sub(1, maxLength) end
    return value
end

local function rankTag(pid)
    if Players[pid] == nil or not Players[pid]:IsServerStaff() then return "" end
    if Players[pid]:IsServerOwner() then return "[Owner] " end
    if Players[pid]:IsAdmin() then return "[Admin] " end
    if Players[pid]:IsModerator() then return "[Mod] " end
    return ""
end

local function characterLine(pid)
    local character = Players[pid].data.character or {}
    local stats = Players[pid].data.stats or {}
    local parts = {}
    if character.race ~= nil then table.insert(parts, tostring(character.race)) end
    if character.class ~= nil then table.insert(parts, tostring(character.class)) end
    if stats.level ~= nil then table.insert(parts, tostring(stats.level) .. " lvl") end
    return table.concat(parts, ", ")
end

local function locationLines(viewerPid, pid)
    local location = Players[pid].data.location or {}
    local lines = {}

    -- Ghost mode hides a player's whereabouts from everyone but staff, which is
    -- the same courtesy the original /list handler extends.
    local vars = Players[pid].data.customVariables or {}
    local hidden = vars.Ghost == 1 or vars.Ghost == true
    local viewerIsStaff = Players[viewerPid] ~= nil and Players[viewerPid]:IsServerStaff()

    if hidden and not viewerIsStaff and viewerPid ~= pid then
        return lines
    end

    if location.cell ~= nil then table.insert(lines, "Cell: " .. tostring(location.cell)) end
    if location.regionName ~= nil then table.insert(lines, "Region: " .. tostring(location.regionName)) end
    return lines
end

local function buildEntry(viewerPid, pid)
    local name = safeToken(Players[pid].name or Players[pid].accountName or ("PID " .. tostring(pid)), 48)

    local label = safeToken(rankTag(pid) .. name, 64)
    local sameCell = tes3mp.GetCell(viewerPid) == tes3mp.GetCell(pid)
    if sameCell and viewerPid ~= pid then label = label .. " *" end

    local details = { rankTag(pid) .. name, "PID: " .. tostring(pid) }

    local character = characterLine(pid)
    if character ~= "" then table.insert(details, character) end

    for _, line in ipairs(locationLines(viewerPid, pid)) do table.insert(details, line) end

    if groupHelper ~= nil and type(groupHelper.GetPlayerGroup) == "function" then
        local group = groupHelper.GetPlayerGroup(pid)
        if group ~= nil then table.insert(details, "Group: " .. tostring(group.name)) end
    end

    return name .. "^" .. escapeField(table.concat(details, "\n")) .. "^" .. label
end

function playerListHelper.SendState(pid)
    if not isValidPid(pid) then return end

    local entries = {}
    local count = 0
    local lastPid = tes3mp.GetLastPlayerId()
    for otherPid = 0, lastPid do
        if isValidPid(otherPid) then
            count = count + 1
            table.insert(entries, buildEntry(pid, otherPid))
        end
    end

    local header = "Players online: " .. tostring(count)
    local fields = { "STATE", escapeField(header), table.concat(entries, ";") }
    tes3mp.SendMessage(pid, STATE_PREFIX .. table.concat(fields, "\t") .. "\n", false)
end

local function processCommand(pid, cmd)
    if not isValidPid(pid) then return end
    playerListHelper.SendState(pid)
end

customCommandHooks.registerCommand("playerlistui", processCommand)

return playerListHelper

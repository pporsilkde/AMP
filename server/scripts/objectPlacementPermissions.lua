require("config")

-- Arena Y013-fix-01
--
-- Server-authoritative RP placement policy for ObjectMove / ObjectRotate.
--
-- Changes over the original Y013 module:
--   * RP mode is resolved through a configurable list of custom-variable names
--     instead of a single hard-coded RPStatus that nothing ever wrote.
--   * privateCellInstances.IsOwnPrivateCell is probed instead of assumed. A
--     missing helper degrades to "not my private cell" and logs once, instead of
--     throwing inside a packet validator on every grab.
--   * The faction-by-cell-name heuristic is off by default. Explicit
--     factionCellOverrides are always honoured.
--   * Deny is reported once per grab attempt, not once per second. The rollback
--     control message is still sent every time so the client never keeps an
--     illegal transform.

local placement = {}

local privateCellInstances = nil
do
    local ok, module = pcall(require, "privateCellInstances")
    if ok and type(module) == "table" then privateCellInstances = module end
end

local houseHelper = nil
do
    local ok, helper = pcall(require, "houseHelper")
    if ok and type(helper) == "table" then houseHelper = helper end
end

-- pid -> { last = os.time(), notified = bool }
local denyState = {}

local warned = {}
local function warnOnce(key, message)
    if warned[key] then return end
    warned[key] = true
    tes3mp.LogMessage(enumerations.log.WARN, "[objectPlacementPermissions] " .. message)
end

local function lower(value)
    return string.lower(tostring(value or ""))
end

local function callModule(module, methods, ...)
    if module == nil then return nil, false end
    for _, name in ipairs(methods) do
        local fn = module[name]
        if type(fn) == "function" then
            local ok, value = pcall(fn, ...)
            if not ok then ok, value = pcall(fn, module, ...) end
            if ok then return value, true end
        end
    end
    return nil, false
end

-------------------------------------------------------------------------------
-- RP mode
-------------------------------------------------------------------------------

-- The original module tested player.data.customVariables.RPStatus, which no
-- other script in the tree ever assigns, so every non-moderator was denied
-- everywhere. Resolve the flag from whatever the server actually uses and let
-- the admin name it in config.
local function getRpMode(player, cfg)
    local custom = player.data and player.data.customVariables or {}

    local names = cfg.rpModeVariables
    if type(names) ~= "table" or #names == 0 then
        names = { "RPStatus", "rpMode", "isRolePlaying" }
    end

    for _, name in ipairs(names) do
        local value = custom[name]
        if value == true then return true end
        if value == false then return false end
        if value == 1 or value == "1" or lower(value) == "true" then return true end
        if value == 0 or value == "0" or lower(value) == "false" then return false end
    end

    -- Never set for this character yet.
    return nil
end

-- Public setter so chat / commands / other modules can drive the same flag.
function placement.SetRpMode(pid, enabled)
    local player = Players[pid]
    if player == nil or player.data == nil then return end

    player.data.customVariables = player.data.customVariables or {}

    local cfg = config.rpObjectPlacement or {}
    local names = cfg.rpModeVariables
    local primary = (type(names) == "table" and names[1]) or "RPStatus"

    player.data.customVariables[primary] = enabled and true or false
end

-------------------------------------------------------------------------------
-- Ownership
-------------------------------------------------------------------------------

local function isOwnPrivateCell(player, cellDescription)
    if privateCellInstances == nil then
        warnOnce("privateCellInstances", "privateCellInstances module is unavailable; " ..
            "own-private-cell placement is disabled")
        return false
    end

    local value, called = callModule(privateCellInstances,
        { "IsOwnPrivateCell", "isOwnPrivateCell" }, player, cellDescription)

    if not called then
        warnOnce("IsOwnPrivateCell", "privateCellInstances.IsOwnPrivateCell is missing; " ..
            "own-private-cell placement is disabled")
        return false
    end

    return value == true
end

local function isOwnHouse(player, cellDescription)
    local data, called = callModule(houseHelper, { "GetCellData", "getCellData" }, cellDescription)
    if not called or type(data) ~= "table" or not data.house then return false end

    local houseName = data.house
    if houseName == true then houseName = cellDescription end

    local owner, gotOwner = callModule(houseHelper,
        { "GetHouseOwnerName", "getHouseOwnerName" }, houseName)
    if not gotOwner or owner == nil then return false end

    local accountName = player.accountName or player.name or ""
    return lower(owner) == lower(accountName)
end

-------------------------------------------------------------------------------
-- Faction premises
-------------------------------------------------------------------------------

local stopWords = {
    ["the"] = true, ["of"] = true, ["guild"] = true, ["house"] = true,
    ["faction"] = true, ["clan"] = true, ["order"] = true
}

local function normalizedWords(value)
    local result = {}
    value = lower(value):gsub("[^%w]+", " ")
    for word in value:gmatch("%S+") do
        if #word >= 4 and not stopWords[word] then result[#result + 1] = word end
    end
    return result
end

local function cellMatchesFactionId(cellDescription, factionId)
    local cell = lower(cellDescription)
    local words = normalizedWords(factionId)
    if #words == 0 then return false end
    for _, word in ipairs(words) do
        if not cell:find(word, 1, true) then return false end
    end
    return true
end

local function isJoinedFaction(ranks, factionId)
    for joinedId, rank in pairs(ranks) do
        if lower(joinedId) == lower(factionId) and tonumber(rank or -1) >= 0 then
            return true
        end
    end
    return false
end

local function hasFactionAccess(player, cellDescription, cfg)
    if cfg.allowFactionCells == false then return false end

    local ranks = player.data and player.data.factionRanks or {}

    -- Explicit mapping always wins and is the only reliable source.
    local overrides = cfg.factionCellOverrides or {}
    for configuredCell, factionIds in pairs(overrides) do
        if lower(configuredCell) == lower(cellDescription) and type(factionIds) == "table" then
            for _, factionId in pairs(factionIds) do
                if isJoinedFaction(ranks, factionId) then return true end
            end
        end
    end

    -- Name matching is a guess: "Mages Guild" matches every mages guild on the
    -- continent, not just the local one. Opt-in only.
    if cfg.factionNameHeuristic ~= true then return false end

    for factionId, rank in pairs(ranks) do
        if tonumber(rank or -1) >= 0 and cellMatchesFactionId(cellDescription, factionId) then
            return true
        end
    end

    return false
end

-------------------------------------------------------------------------------
-- Policy
-------------------------------------------------------------------------------

function placement.IsAllowed(pid, cellDescription)
    local player = Players[pid]
    if player == nil or not player:IsLoggedIn() then return false end

    local cfg = config.rpObjectPlacement or {}
    if cfg.enabled == false then return true end
    if cfg.moderatorBypass ~= false and player:IsModerator() then return true end

    local loadedCell = LoadedCells[cellDescription]
    if loadedCell ~= nil and loadedCell.isExterior == true then return false end

    -- Description-level fallback for a cell that is not in LoadedCells yet.
    -- Exterior descriptions are "x, y" pairs.
    if loadedCell == nil and string.match(cellDescription, "^%-?%d+, *%-?%d+$") ~= nil then
        return false
    end

    if cfg.requireRpMode ~= false then
        local rpMode = getRpMode(player, cfg)
        if rpMode == false then return false end
        if rpMode == nil and cfg.allowUnknownRpMode ~= true then return false end
    end

    if cfg.allowOwnPrivateCell ~= false and isOwnPrivateCell(player, cellDescription) then
        return true
    end
    if cfg.allowOwnHouse ~= false and isOwnHouse(player, cellDescription) then return true end
    if hasFactionAccess(player, cellDescription, cfg) then return true end

    return false
end

-------------------------------------------------------------------------------
-- Denial
-------------------------------------------------------------------------------

-- A held object is resynced roughly every 50 ms and both ObjectMove and
-- ObjectRotate are validated, so a naive per-second message produced a wall of
-- centered MessageBoxes. Send the rollback control every time, but ask the
-- client to show the explanation only when this is a new grab attempt: a gap
-- longer than denyQuietSeconds means the player let go and tried again.
function placement.Deny(pid, cellDescription)
    local cfg = config.rpObjectPlacement or {}
    local quiet = tonumber(cfg.denyQuietSeconds) or 4

    local now = os.time()
    local state = denyState[pid]

    local showMessage = false
    if state == nil or (now - state.last) >= quiet then
        showMessage = true
        tes3mp.LogMessage(enumerations.log.WARN, "Rejected RP object transform from " ..
            logicHandler.GetChatName(pid) .. " in " .. tostring(cellDescription))
    end

    denyState[pid] = { last = now }

    -- Hidden client-control message. Field 1 is the rollback flag (always 1),
    -- field 2 asks for the localized explanation. No leading tab: the client
    -- splits on tabs and a leading one would shift every field by an index.
    -- It never appears in chat.
    tes3mp.SendMessage(pid, "@@AMP_PLACE_DENY@@1\t" .. (showMessage and "1" or "0") .. "\n", false)
end

function placement.Clear(pid)
    denyState[pid] = nil
end

return placement

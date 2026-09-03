require("config")
local tableHelper = require("tableHelper")

-- Arena Y020
--
-- Server-authoritative protection for the native physics grab (hold Activate/E).
-- This is intentionally NOT an RP/home/faction permission system.
--
-- Ordinary players may move only objects that were actually dropped by a player
-- and therefore entered the cell through ObjectPlace with droppedByPlayer=true.
-- References authored in ESM/ESP/OMWAddon data, script-spawned props and other
-- original cell objects are protected from client-gameplay ObjectMove/Rotate.
-- Moderators and higher may move any object.
--
-- Non-gameplay origins (server scripts/client scripts/console) are not filtered
-- here, so quest scripts, doors, AI and authoritative server mechanics are not
-- accidentally blocked by an anti-grief grab rule.

local placement = {}
local denyState = {}

local function getConfig()
    return config.objectGrabPermissions or {}
end

local function wasDroppedByPlayer(cell, uniqueIndex)
    if cell == nil or type(cell.data) ~= "table" then return false end
    if type(cell.data.objectData) ~= "table" or type(cell.data.packets) ~= "table" then return false end

    local stored = cell.data.objectData[uniqueIndex]
    if type(stored) ~= "table" or stored.droppedByPlayer ~= true then return false end

    -- Require the provenance marker AND an ObjectPlace entry. This avoids
    -- trusting a stray/stale boolean in hand-edited JSON data.
    local placed = cell.data.packets.place
    return type(placed) == "table" and tableHelper.containsValue(placed, uniqueIndex)
end

function placement.IsAllowed(pid, cellDescription, objects)
    local player = Players[pid]
    if player == nil or not player:IsLoggedIn() then return false end

    local cfg = getConfig()
    if cfg.enabled == false then return true end
    if cfg.moderatorBypass ~= false and player:IsModerator() then return true end

    -- The native E/Activate physics placement path publishes transforms as
    -- CLIENT_GAMEPLAY. Do not apply this anti-grief rule to script/server moves.
    if tes3mp.GetObjectListOrigin() ~= enumerations.packetOrigin.CLIENT_GAMEPLAY then
        return true
    end

    local cell = LoadedCells[cellDescription]
    if cell == nil or type(objects) ~= "table" then return false end

    -- A transform packet can contain more than one object. Every object in it
    -- must be safe; otherwise reject the packet atomically.
    for uniqueIndex, _ in pairs(objects) do
        if cfg.allowPlayerDropped == false or not wasDroppedByPlayer(cell, uniqueIndex) then
            return false
        end
    end

    return true
end

function placement.Deny(pid, cellDescription)
    local cfg = getConfig()
    local quiet = tonumber(cfg.denyQuietSeconds) or 4
    local now = os.time()
    local state = denyState[pid]

    local showMessage = false
    if state == nil or (now - state.last) >= quiet then
        showMessage = true
        tes3mp.LogMessage(enumerations.log.WARN, "Rejected protected world-object grab from " ..
            logicHandler.GetChatName(pid) .. " in " .. tostring(cellDescription))
    end

    denyState[pid] = { last = now }

    -- Roll back every rejected transform, but only show the explanation once per
    -- grab attempt. The client-side Y013-fix-01 rollback also works after release.
    tes3mp.SendMessage(pid, "@@AMP_PLACE_DENY@@1\t" .. (showMessage and "1" or "0") .. "\n", false)
end

function placement.Clear(pid)
    denyState[pid] = nil
end

return placement

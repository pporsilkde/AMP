-- ArenaMP X052: per-player nickname colour for the chat.
--
-- Design notes:
--   * the palette is stored here as literal #RRGGBB values instead of names
--     from color.lua, so it does not depend on a server having the extended
--     colour table an older Nirn/jrp setup used to ship;
--   * the client receives only hex codes and the selected index; the human
--     readable colour names come from the client's own localization, so RU and
--     EN players each see their own language without a server round trip;
--   * the choice lives in customVariables and therefore survives a relog.

local chatColorHelper = {}

local STATE_PREFIX = "@@AMP_COLOR@@"

chatColorHelper.palette = {
    "#87CEEB", -- sky blue
    "#4682B4", -- steel blue
    "#3CB371", -- medium sea green
    "#8FBC8F", -- dark sea green
    "#FFC0CB", -- pink
    "#FF69B4", -- hot pink
    "#F0E68C", -- khaki
    "#DAA520", -- goldenrod
    "#FA8072", -- salmon
    "#E9967A", -- dark salmon
    "#40E0D0", -- turquoise
    "#20B2AA", -- light sea green
    "#BC8F8F", -- rosy brown
    "#DDA0DD", -- plum
    "#DEB887", -- burly wood
    "#C0C0C0"  -- silver
}

local DEFAULT_COLOR = "#FFFFFF"

local function isValidPid(pid)
    return Players[pid] ~= nil and Players[pid]:IsLoggedIn() and Players[pid].data ~= nil
end

local function enabled()
    return type(config.chatNameColors) ~= "table" or config.chatNameColors.enabled ~= false
end

local function persist(pid)
    if not isValidPid(pid) then return end
    if type(Players[pid].QuicksaveToDrive) == "function" then
        Players[pid]:QuicksaveToDrive()
    elseif type(Players[pid].Save) == "function" then
        Players[pid]:Save()
    end
end

local function ensureVars(pid)
    if not isValidPid(pid) then return nil end
    Players[pid].data.customVariables = Players[pid].data.customVariables or {}
    return Players[pid].data.customVariables
end

-- Index of the colour the player picked, or nil when they use the default.
function chatColorHelper.GetIndex(pid)
    local vars = ensureVars(pid)
    if vars == nil then return nil end
    local index = tonumber(vars.arenampChatColor)
    if index == nil or index < 1 or index > #chatColorHelper.palette then
        return nil
    end
    return math.floor(index)
end

-- The colour code to prefix a player's name with. Always safe to concatenate.
function chatColorHelper.GetColor(pid)
    if not enabled() then return DEFAULT_COLOR end
    local index = chatColorHelper.GetIndex(pid)
    if index == nil then return DEFAULT_COLOR end
    return chatColorHelper.palette[index]
end

-- Assign a random unused colour on first login so a fresh server already looks
-- readable without anyone touching the menu. Disabled via config.
function chatColorHelper.EnsureColor(pid)
    if not enabled() then return end
    if type(config.chatNameColors) == "table" and config.chatNameColors.assignOnFirstLogin == false then
        return
    end
    if chatColorHelper.GetIndex(pid) ~= nil then return end

    local taken = {}
    for otherPid, player in pairs(Players) do
        if player ~= nil and otherPid ~= pid then
            local index = chatColorHelper.GetIndex(otherPid)
            if index ~= nil then taken[index] = true end
        end
    end

    local free = {}
    for i = 1, #chatColorHelper.palette do
        if not taken[i] then table.insert(free, i) end
    end
    if #free == 0 then
        for i = 1, #chatColorHelper.palette do table.insert(free, i) end
    end

    chatColorHelper.SetIndex(pid, free[math.random(1, #free)], true)
end

function chatColorHelper.SetIndex(pid, index, quiet)
    local vars = ensureVars(pid)
    if vars == nil then return false, "Player is not logged in" end

    index = tonumber(index)
    if index == nil or index < 1 or index > #chatColorHelper.palette then
        return false, "Colour index must be between 1 and " .. tostring(#chatColorHelper.palette)
    end

    vars.arenampChatColor = math.floor(index)
    persist(pid)
    if not quiet then
        chatColorHelper.SendState(pid)
    end
    return true
end

function chatColorHelper.ResetIndex(pid)
    local vars = ensureVars(pid)
    if vars == nil then return false, "Player is not logged in" end
    vars.arenampChatColor = nil
    persist(pid)
    return true
end

function chatColorHelper.SendState(pid)
    if not isValidPid(pid) then return end
    local index = chatColorHelper.GetIndex(pid)
    local fields = {
        "STATE",
        index ~= nil and tostring(index) or "0",
        table.concat(chatColorHelper.palette, ";")
    }
    tes3mp.SendMessage(pid, STATE_PREFIX .. table.concat(fields, "\t") .. "\n", false)
end

local function processCommand(pid, cmd)
    if not isValidPid(pid) then return end

    local sub = tostring(cmd[2] or "state"):lower()
    if sub == "state" or sub == "" then
        chatColorHelper.SendState(pid)
        return
    end

    if sub == "default" or sub == "reset" then
        chatColorHelper.ResetIndex(pid)
        chatColorHelper.SendState(pid)
        tes3mp.SendMessage(pid, color.Turquoise .. "[Chat] " .. color.White ..
            "Nickname colour reset.\n", false)
        return
    end

    local ok, message = chatColorHelper.SetIndex(pid, sub)
    if not ok then
        tes3mp.SendMessage(pid, color.Red .. "[Chat] " .. color.White .. tostring(message) .. "\n", false)
        return
    end

    local applied = chatColorHelper.GetColor(pid)
    tes3mp.SendMessage(pid, color.Turquoise .. "[Chat] " .. color.White .. "Nickname colour: " ..
        applied .. logicHandler.GetChatName(pid) .. color.Default .. "\n", false)
end

customCommandHooks.registerCommand("chatcolor", processCommand)
customCommandHooks.registerCommand("nickcolor", processCommand)

customEventHooks.registerHandler("OnPlayerAuthentified", function(eventStatus, pid)
    if isValidPid(pid) then
        chatColorHelper.EnsureColor(pid)
        chatColorHelper.SendState(pid)
    end
end)

return chatColorHelper

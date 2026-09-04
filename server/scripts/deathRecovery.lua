local deathRecovery = {}

local CONTROL_PREFIX = "@@AMP_REVIVE@@"

local function clamp(value, low, high)
    value = tonumber(value) or low
    if value < low then return low end
    if value > high then return high end
    return value
end

local function cfgNumber(key, fallback)
    if type(config.deathRecovery) == "table" and config.deathRecovery[key] ~= nil then
        return tonumber(config.deathRecovery[key]) or fallback
    end
    return fallback
end

local function now()
    -- Y039: use the server monotonic clock so a rescue at e.g. 7.6 seconds
    -- settles the exact XP fraction instead of rounding to whole wall-clock seconds.
    if type(tes3mp.GetMillisecondsSinceServerStart) == "function" then
        return tes3mp.GetMillisecondsSinceServerStart() / 1000
    end
    return os.time()
end

local function stopTimer(player, field)
    if player ~= nil and player[field] ~= nil then
        tes3mp.StopTimer(player[field])
        player[field] = nil
    end
end

local function sendLevelSilently(player)
    player.data.stats.experience = math.max(0, tonumber(player.data.stats.experience) or 0)

    -- Y040: rebuild the complete PlayerLevel packet. Setting only the experience
    -- field left level/levelProgress/skillPoints/xpAttributeProgress at whatever
    -- the server had last written for this pid, and LocalPlayer::setLevel()
    -- applies every field of the packet it receives.
    if type(player.LoadLevel) == "function" then
        player:LoadLevel()
        return
    end

    tes3mp.SetExperience(player.pid, player.data.stats.experience)
    tes3mp.SendLevel(player.pid)
end

local function exactRemainingXp(player, atTime)
    local initial = math.max(0, tonumber(player.deathRecoveryInitialXp) or 0)
    local duration = math.max(0.1, tonumber(player.deathRecoveryDuration) or 10)
    local started = tonumber(player.deathRecoveryStartedAt) or atTime
    local elapsed = math.max(0, atTime - started)
    local fraction = math.max(0, 1 - elapsed / duration)
    return initial * fraction
end

local function applyXpAtTime(player, atTime, persist)
    if player == nil or player.deathRecoveryActive ~= true then return 0 end
    local nextXp = exactRemainingXp(player, atTime)
    if nextXp < 0.001 then nextXp = 0 end
    local oldXp = math.max(0, tonumber(player.data.stats.experience) or 0)
    if math.abs(oldXp - nextXp) >= 0.001 then
        player.data.stats.experience = nextXp
        sendLevelSilently(player)
        if persist and type(player.PersistXpProgress) == "function" then
            player:PersistXpProgress(true)
        end
    end
    return nextXp
end

local function scheduleTick(player)
    if player == nil or player.deathRecoveryActive ~= true then return end
    stopTimer(player, "deathRecoveryXpTimerId")
    local tickSeconds = clamp(cfgNumber("xp tick seconds", 1), 0.25, 2)
    player.deathRecoveryXpTimerId = tes3mp.CreateTimerEx(
        "OnDeathXpDecayTick", time.seconds(tickSeconds), "is", player.pid, player.accountName)
    tes3mp.StartTimer(player.deathRecoveryXpTimerId)
end

function deathRecovery.Begin(player)
    if player == nil then return 10 end

    deathRecovery.Cancel(player, false)

    local duration = clamp(cfgNumber("xp decay seconds", 10), 3, 30)
    local currentXp = math.max(0, tonumber(player.data.stats.experience) or 0)

    player.deathRecoveryActive = true
    player.deathRecoveryStartedAt = now()
    player.deathRecoveryDuration = duration
    player.deathRecoveryInitialXp = currentXp

    -- The first tick is delayed; the client renders a smooth interpolation while
    -- these authoritative one-second checkpoints prevent reconnect/lag exploits.
    scheduleTick(player)

    -- Never auto-respawn before the XP decay window can finish.
    local minimumRespawn = duration
    if currentXp <= 0 and type(config.xpLeveling) == "table" then
        local perLevel = tonumber(config.xpLeveling["zero xp death cooldown per level"]) or 0
        local currentLevel = math.max(1, tonumber(player.data.stats.level) or 1)
        minimumRespawn = math.max(minimumRespawn, duration + math.max(0, perLevel) * currentLevel)
    end
    return minimumRespawn
end

function deathRecovery.Tick(pid, accountName)
    local player = Players and Players[pid] or nil
    if player == nil or not player:IsLoggedIn() or player.accountName ~= accountName
        or player.deathRecoveryActive ~= true then
        return
    end

    player.deathRecoveryXpTimerId = nil
    local remaining = applyXpAtTime(player, now(), true)
    if remaining > 0 then
        scheduleTick(player)
    else
        player.data.stats.experience = 0
        sendLevelSilently(player)
        if type(player.PersistXpProgress) == "function" then
            player:PersistXpProgress(true)
        end
        -- Keep deathRecoveryActive true until normal respawn or a recovery action;
        -- the recovery window is over, but the old zero-XP extra cooldown may remain.
    end
end

function deathRecovery.Cancel(player, settleXp)
    if player == nil then return end
    if settleXp == true and player.deathRecoveryActive == true then
        applyXpAtTime(player, now(), true)
    end
    stopTimer(player, "deathRecoveryXpTimerId")
    player.deathRecoveryActive = false
    player.deathRecoveryStartedAt = nil
    player.deathRecoveryDuration = nil
    player.deathRecoveryInitialXp = nil
end

local function inventoryCount(player, refId)
    if player == nil or type(player.data.inventory) ~= "table" or type(refId) ~= "string" or refId == "" then
        return 0
    end
    local count = 0
    for _, item in pairs(player.data.inventory) do
        if type(item) == "table" and item.refId == refId then
            count = count + math.max(0, tonumber(item.count) or 0)
        end
    end
    return count
end

local RESTORE_HEALTH_EFFECT_ID = 75

local function cfgList(key)
    if type(config.deathRecovery) == "table" and type(config.deathRecovery[key]) == "table" then
        return config.deathRecovery[key]
    end
    return {}
end

local function cfgBoolean(key, fallback)
    if type(config.deathRecovery) == "table" and type(config.deathRecovery[key]) == "boolean" then
        return config.deathRecovery[key]
    end
    return fallback
end

local function recordRestoresHealth(refId)
    -- Server-generated and permanently stored potion records carry their effect
    -- list, so custom alchemy is validated exactly instead of by name.
    if RecordStores == nil or RecordStores["potion"] == nil then return false end
    local data = RecordStores["potion"].data
    if type(data) ~= "table" then return false end

    local record = nil
    if type(data.permanentRecords) == "table" then record = data.permanentRecords[refId] end
    if record == nil and type(data.generatedRecords) == "table" then record = data.generatedRecords[refId] end
    if type(record) ~= "table" or type(record.effects) ~= "table" then return false end

    for _, effect in pairs(record.effects) do
        if type(effect) == "table" and tonumber(effect.id) == RESTORE_HEALTH_EFFECT_ID then
            local magnitude = math.max(tonumber(effect.magnitudeMax) or 0, tonumber(effect.magnitudeMin) or 0)
            if magnitude > 0 then return true end
        end
    end
    return false
end

local function isRestoreHealthPotion(refId)
    -- Y040: the client checks the ESM record before offering the prompt, but the
    -- request itself arrives as plain text. Without this a modified client could
    -- spend a worthless item - or an empty string - and still stand back up.
    if type(refId) ~= "string" or refId == "" then return false end
    if not cfgBoolean("validate potion refIds", true) then return true end

    local lowered = string.lower(refId)
    for _, pattern in pairs(cfgList("restore health refId patterns")) do
        if type(pattern) == "string" and pattern ~= "" and string.match(lowered, pattern) ~= nil then
            return true
        end
    end

    return recordRestoresHealth(refId)
end

local function takeOne(player, refId)
    if not isRestoreHealthPotion(refId) then
        tes3mp.LogMessage(enumerations.log.WARN, "deathRecovery: refused unrecognised revive item " ..
            tostring(refId) .. " from " .. tostring(player.accountName))
        return false
    end
    if inventoryCount(player, refId) < 1 then return false end
    inventoryHelper.removeClosestItem(player.data.inventory, refId, 1)
    tableHelper.cleanNils(player.data.inventory)
    player:LoadItemChanges({ { refId = refId, count = 1 } }, enumerations.inventory.REMOVE)
    player:Save()
    return true
end

local function findPidByCharacterName(name)
    if type(name) ~= "string" or name == "" or Players == nil then return nil end
    for pid, player in pairs(Players) do
        if player ~= nil and player:IsLoggedIn() and tes3mp.GetName(pid) == name then
            return pid
        end
    end
    return nil
end

local function nearby(pidA, pidB)
    if pidA == nil or pidB == nil then return false end

    -- Y040: interior and exterior cells share the same local coordinate ranges, so
    -- a pure distance test let a player in one interior revive a party member lying
    -- in a completely different cell with similar coordinates.
    local cellA = tes3mp.GetCell(pidA)
    local cellB = tes3mp.GetCell(pidB)
    if type(cellA) ~= "string" or type(cellB) ~= "string" or cellA ~= cellB then
        return false
    end

    local maxDistance = clamp(cfgNumber("ally revive distance", 256), 96, 768)
    local dx = tes3mp.GetPosX(pidA) - tes3mp.GetPosX(pidB)
    local dy = tes3mp.GetPosY(pidA) - tes3mp.GetPosY(pidB)
    local dz = tes3mp.GetPosZ(pidA) - tes3mp.GetPosZ(pidB)
    return dx * dx + dy * dy + dz * dz <= maxDistance * maxDistance
end

local function recoverTarget(target, sourcePid, kind)
    if target == nil or target.deathRecoveryActive ~= true then return false end

    -- Settle the exact XP at the instant of rescue before stopping decay.
    applyXpAtTime(target, now(), true)
    stopTimer(target, "deathRecoveryXpTimerId")

    local fractionKey = kind == "touch" and "touch revive health fraction" or "potion revive health fraction"
    local defaultFraction = kind == "touch" and 0.10 or 0.25
    local fraction = clamp(cfgNumber(fractionKey, defaultFraction), 0.01, 1)
    local healthBase = math.max(1, tonumber(target.data.stats.healthBase) or 1)
    local restoredHealth = math.max(1, healthBase * fraction)

    target.deathRecoveryActive = false
    target.deathRecoveryStartedAt = nil
    target.deathRecoveryDuration = nil
    target.deathRecoveryInitialXp = nil

    if target.resurrectTimerId ~= nil then
        tes3mp.StopTimer(target.resurrectTimerId)
        target.resurrectTimerId = nil
    end

    -- Recovery is intentionally not BasePlayer:Resurrect(): no shrine teleport and
    -- no death-jail penalty. The player gets back up exactly where they fell.
    if target.data.shapeshift ~= nil and target.data.shapeshift.isWerewolf == true then
        target:SetWerewolfState(false)
    end
    contentFixer.UnequipDeadlyItems(target.pid)
    tes3mp.Resurrect(target.pid, enumerations.resurrect.REGULAR)
    target.data.stats.healthCurrent = restoredHealth
    target:LoadStatsDynamic()
    target:Save()

    local helperName = sourcePid ~= nil and logicHandler.GetChatName(sourcePid) or ""
    local source = kind == "touch" and "healing touch" or "Restore Health potion"
    if helperName ~= "" and sourcePid ~= target.pid then
        tes3mp.SendMessage(target.pid, helperName .. " revived you with " .. source .. ".\n", false)
        tes3mp.SendMessage(sourcePid, "You revived " .. logicHandler.GetChatName(target.pid) .. ".\n", false)
    else
        -- Y040: the wording used to claim a potion even on the touch path.
        tes3mp.SendMessage(target.pid, "You recovered with a " .. source .. ".\n", false)
    end
    return true
end

local function parseControl(message)
    if type(message) ~= "string" or message:sub(1, #CONTROL_PREFIX) ~= CONTROL_PREFIX then
        return nil
    end
    local payload = message:sub(#CONTROL_PREFIX + 1)
    local fields = {}
    for part in string.gmatch(payload .. "\t", "([^\t]*)\t") do
        table.insert(fields, part)
    end
    return fields
end

local function controlValidator(eventStatus, pid, message)
    local fields = parseControl(message)
    if fields == nil then return nil end

    local action = fields[1] or ""
    local player = Players and Players[pid] or nil
    if player == nil or not player:IsLoggedIn() then
        return customEventHooks.makeEventStatus(false, false)
    end

    if action == "SELF_POTION" then
        local refId = fields[2] or ""
        if player.deathRecoveryActive == true and refId ~= "" and takeOne(player, refId) then
            recoverTarget(player, pid, "potion")
        end

    elseif action == "ALLY_POTION" or action == "TOUCH" then
        local targetName = fields[2] or ""
        local targetPid = findPidByCharacterName(targetName)
        local target = targetPid ~= nil and Players[targetPid] or nil
        if target ~= nil and target.deathRecoveryActive == true
            and groupHelper ~= nil and groupHelper.ArePlayersInSameGroup(pid, targetPid)
            and nearby(pid, targetPid) then
            if action == "ALLY_POTION" then
                local refId = fields[3] or ""
                if refId ~= "" and takeOne(player, refId) then
                    recoverTarget(target, pid, "potion")
                end
            else
                -- The client only emits TOUCH after a successful Restore Health
                -- touch effect actually resolves against the dead party member.
                recoverTarget(target, pid, "touch")
            end
        end
    end

    -- Recovery controls are transport, never visible player chat.
    return customEventHooks.makeEventStatus(false, false)
end

local function levelValidator(eventStatus, pid, playerPacket)
    local player = Players and Players[pid] or nil
    if player == nil or player.deathRecoveryActive ~= true
        or type(playerPacket) ~= "table" or type(playerPacket.stats) ~= "table" then
        return nil
    end

    -- While incapacitated, the server owns the XP ceiling. A stale or modified
    -- client may submit a PlayerLevel packet between decay ticks, but it can
    -- never restore XP that has already faded. Other level/SP fields retain the
    -- normal ArenaMP handling.
    local ceiling = exactRemainingXp(player, now())
    playerPacket.stats.experience = math.min(
        math.max(0, tonumber(playerPacket.stats.experience) or 0), ceiling)
    return nil
end

customEventHooks.registerValidator("OnPlayerLevel", levelValidator)
customEventHooks.registerValidator("OnPlayerSendMessage", controlValidator)
customEventHooks.registerHandler("OnPlayerDisconnect", function(_, pid)
    local player = Players and Players[pid] or nil
    if player ~= nil then deathRecovery.Cancel(player, true) end
end)

return deathRecovery

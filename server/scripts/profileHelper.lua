-- ArenaMP Y049 persistent profile/statistics/return-point helper.
local profileHelper = {}
local cfg = {
    unlockLevel = 25,
    returnCooldown = 20,
    maxSlots = 3
}
if type(config.profileSystem) == "table" then
    cfg.unlockLevel = tonumber(config.profileSystem["return unlock level"]) or cfg.unlockLevel
    cfg.returnCooldown = tonumber(config.profileSystem["return cooldown seconds"]) or cfg.returnCooldown
end

local function valid(pid)
    return Players[pid] ~= nil and Players[pid]:IsLoggedIn() and type(Players[pid].data) == "table"
end

local function isRu(pid)
    return valid(pid) and tostring(Players[pid].language or "EN"):upper() == "RU"
end

local function tr(pid, ru, en)
    return isRu(pid) and ru or en
end

local function ensure(pid)
    if not valid(pid) then return nil end
    local p = Players[pid]
    p.data.profile = type(p.data.profile) == "table" and p.data.profile or {}
    local d = p.data.profile
    d.playSeconds = tonumber(d.playSeconds) or 0
    d.deaths = tonumber(d.deaths) or 0
    d.arrests = tonumber(d.arrests) or 0
    d.returnSlots = type(d.returnSlots) == "table" and d.returnSlots or {}
    d.lastReturn = tonumber(d.lastReturn) or 0
    if d.returnUnlocked == nil then d.returnUnlocked = false end
    return d
end

local function save(pid)
    if valid(pid) and type(Players[pid].QuicksaveToDrive) == "function" then
        Players[pid]:QuicksaveToDrive()
    end
end

local function liveSeconds(pid)
    local d = ensure(pid)
    if not d then return 0 end
    local extra = 0
    local lastLogin = tonumber(Players[pid].data.timestamps and Players[pid].data.timestamps.lastLogin) or 0
    if lastLogin > 0 then extra = math.max(0, os.time() - lastLogin) end
    return math.max(0, math.floor(d.playSeconds + extra))
end

local function formatTime(seconds)
    seconds = math.max(0, math.floor(tonumber(seconds) or 0))
    local days = math.floor(seconds / 86400); seconds = seconds % 86400
    local hours = math.floor(seconds / 3600); seconds = seconds % 3600
    local mins = math.floor(seconds / 60)
    if days > 0 then return string.format("%dd %02dh %02dm", days, hours, mins) end
    return string.format("%02dh %02dm", hours, mins)
end

local function slotName(index)
    return ({"A", "B", "C"})[index]
end

local function slotDescription(slot, pid)
    if type(slot) ~= "table" or type(slot.cell) ~= "string" or slot.cell == "" then
        return tr(pid, "пусто", "empty")
    end
    return slot.cell
end

local function showMain(pid)
    local d = ensure(pid); if not d then return end
    local level = tonumber(Players[pid].data.stats and Players[pid].data.stats.level) or 1
    local text = tr(pid, "ПРОФИЛЬ", "PROFILE") .. "\n\n" ..
        tr(pid, "В игре: ", "Time played: ") .. formatTime(liveSeconds(pid)) .. "\n" ..
        tr(pid, "Смертей: ", "Deaths: ") .. tostring(d.deaths) .. "\n" ..
        tr(pid, "Арестов: ", "Arrests: ") .. tostring(d.arrests) .. "\n" ..
        tr(pid, "Уровень: ", "Level: ") .. tostring(level) .. "\n\n" ..
        tr(pid, "Точки возврата", "Return points") .. " (" .. tr(pid, "ур. ", "lvl ") .. tostring(cfg.unlockLevel) .. "):\n"
    for i = 1, cfg.maxSlots do
        local name = slotName(i)
        text = text .. name .. ": " .. slotDescription(d.returnSlots[name], pid) .. "\n"
    end
    if not d.returnUnlocked then
        text = text .. "\n" .. tr(pid, "Откроются автоматически на 25 уровне.", "They unlock automatically at level 25.")
    end
    local buttons = tr(pid, "Точки возврата;Обновить;Удалить профиль;Закрыть",
        "Return points;Refresh;Delete profile;Close")
    tes3mp.CustomMessageBox(pid, config.customMenuIds.profileMain, text, buttons)
end

local function showReturns(pid)
    local d = ensure(pid); if not d then return end
    if not d.returnUnlocked then
        tes3mp.MessageBox(pid, -1, tr(pid, "Точки возврата откроются на 25 уровне.", "Return points unlock at level 25."))
        return
    end
    local text = tr(pid, "ТОЧКИ ВОЗВРАТА\nВыберите слот. «Записать» сохраняет текущую позицию.",
        "RETURN POINTS\nChoose a slot. Save stores your current position.") .. "\n\n"
    for i=1,cfg.maxSlots do
        local n=slotName(i); text=text..n..": "..slotDescription(d.returnSlots[n],pid).."\n"
    end
    tes3mp.CustomMessageBox(pid, config.customMenuIds.profileReturns, text,
        tr(pid, "A: записать;A: вернуться;B: записать;B: вернуться;C: записать;C: вернуться;Назад",
            "A: save;A: return;B: save;B: return;C: save;C: return;Back"))
end

local function setSlot(pid, name)
    local d=ensure(pid); if not d or not d.returnUnlocked then return end
    local cell=tostring(tes3mp.GetCell(pid) or "")
    if cell=="" then return end
    d.returnSlots[name]={cell=cell,posX=tes3mp.GetPosX(pid),posY=tes3mp.GetPosY(pid),posZ=tes3mp.GetPosZ(pid),rotX=tes3mp.GetRotX(pid),rotZ=tes3mp.GetRotZ(pid)}
    save(pid)
    tes3mp.MessageBox(pid,-1,tr(pid,"Точка "..name.." сохранена.","Return point "..name.." saved."))
end

local function goSlot(pid, name)
    local d=ensure(pid); if not d or not d.returnUnlocked then return end
    local slot=d.returnSlots[name]
    if type(slot)~="table" or not slot.cell or slot.cell=="" then
        tes3mp.MessageBox(pid,-1,tr(pid,"Этот слот пуст.","This slot is empty.")); return
    end
    local now=os.time()
    if now < (tonumber(d.lastReturn) or 0) + cfg.returnCooldown then
        local left=(tonumber(d.lastReturn) or 0)+cfg.returnCooldown-now
        tes3mp.MessageBox(pid,-1,tr(pid,"Возврат будет доступен через ","Return available in ")..tostring(left)..tr(pid," сек."," sec.")); return
    end
    d.lastReturn=now
    Players[pid].data.location = {
        cell=slot.cell, regionName="", posX=slot.posX, posY=slot.posY, posZ=slot.posZ, rotX=slot.rotX, rotZ=slot.rotZ
    }
    tes3mp.SetCell(pid, slot.cell)
    tes3mp.SetPos(pid, slot.posX, slot.posY, slot.posZ)
    tes3mp.SetRot(pid, slot.rotX, slot.rotZ)
    tes3mp.SendCell(pid); tes3mp.SendPos(pid)
    save(pid)
end

local function requestDelete(pid)
    tes3mp.PasswordDialog(pid, config.customMenuIds.profileDeletePassword,
        tr(pid,"Удаление профиля необратимо. Введите текущий пароль. После повторного входа потребуется создать пароль и персонажа заново.",
            "Profile deletion is irreversible. Enter your current password. On the next login you will create a new password and character."), "")
end

local function deleteProfile(pid, inputHash)
    if not valid(pid) or inputHash == nil then return end
    local p=Players[pid]
    local login=p.data.login or {}
    if tostring(login.passwordHash or "") ~= tes3mp.GetSHA256Hash(tostring(inputHash) .. tostring(login.passwordSalt or "")) then
        tes3mp.MessageBox(pid,-1,tr(pid,"Неверный пароль. Профиль не удалён.","Incorrect password. Profile was not deleted.")); return
    end
    if config.databaseType ~= "json" then
        tes3mp.MessageBox(pid,-1,tr(pid,"Удаление профиля через интерфейс сейчас поддерживается только для JSON-профилей.",
            "In-game profile deletion currently supports JSON profiles only.")); return
    end
    local path=config.dataPath.."/player/"..tostring(p.accountFile or "")
    local ok, err=os.remove(path)
    if not ok then
        tes3mp.MessageBox(pid,-1,tr(pid,"Не удалось удалить файл профиля: ","Could not delete profile file: ")..tostring(err or "unknown")); return
    end
    p.hasAccount=false
    -- Clear the client's remembered password before disconnecting. The next
    -- registration therefore starts with an empty password field.
    tes3mp.SendMessage(pid,"@@AMP_PROFILE@@DELETED\n",false)
    tes3mp.SendMessage(pid,tr(pid,"Профиль удалён. Подключитесь снова, чтобы создать новый пароль и персонажа.\n",
        "Profile deleted. Reconnect to create a new password and character.\n"),false)
    p:Kick()
end

function profileHelper.AccumulateSession(player)
    if type(player)~="table" or type(player.data)~="table" then return end
    player.data.profile=type(player.data.profile)=="table" and player.data.profile or {}
    local d=player.data.profile
    d.playSeconds=tonumber(d.playSeconds) or 0
    local t=tonumber(player.data.timestamps and player.data.timestamps.lastSessionDuration) or 0
    d.playSeconds=d.playSeconds+math.max(0,t)
end

function profileHelper.RecordDeath(pid)
    local d=ensure(pid); if d then d.deaths=d.deaths+1; save(pid) end
end

function profileHelper.RecordArrest(pid)
    local d=ensure(pid); if not d then return end
    local now=os.time()
    if now-(tonumber(d.lastArrestSignal) or 0) < 10 then return end
    d.lastArrestSignal=now; d.arrests=d.arrests+1; save(pid)
end

function profileHelper.CheckUnlock(pid, announce)
    local d=ensure(pid); if not d then return end
    local level=tonumber(Players[pid].data.stats and Players[pid].data.stats.level) or 1
    if level>=cfg.unlockLevel and not d.returnUnlocked then
        d.returnUnlocked=true; save(pid)
        if announce then tes3mp.MessageBox(pid,-1,tr(pid,"25 уровень! Открыты три точки возврата: A, B и C.",
            "Level 25! Three return-point slots are now unlocked: A, B and C.")) end
    end
end

local function command(pid, cmd)
    if not valid(pid) then return end
    local sub=tostring(cmd[2] or ""):lower()
    if sub=="" or sub=="show" then showMain(pid); return end
    if sub=="returns" then showReturns(pid); return end
end

customCommandHooks.registerCommand("profile", command)
customEventHooks.registerHandler("OnPlayerLevel", function(eventStatus,pid)
    if eventStatus.validDefaultHandler then profileHelper.CheckUnlock(pid,true) end
end)
customEventHooks.registerHandler("OnPlayerAuthentified", function(eventStatus,pid)
    if eventStatus.validDefaultHandler then profileHelper.CheckUnlock(pid,false) end
end)
customEventHooks.registerHandler("OnGUIAction", function(eventStatus,pid,idGui,data)
    if not valid(pid) then return end
    if idGui==config.customMenuIds.profileMain then
        local v=tonumber(data) or -1
        if v==0 then showReturns(pid) elseif v==1 then showMain(pid) elseif v==2 then requestDelete(pid) end
    elseif idGui==config.customMenuIds.profileReturns then
        local v=tonumber(data) or -1
        if v>=0 and v<=5 then
            local slot=slotName(math.floor(v/2)+1)
            if v%2==0 then setSlot(pid,slot) else goSlot(pid,slot) end
            showReturns(pid)
        elseif v==6 then showMain(pid) end
    elseif idGui==config.customMenuIds.profileDeletePassword then
        deleteProfile(pid,data)
    end
end)

return profileHelper

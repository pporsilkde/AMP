-- ArenaMP X056 group invite popup harness.
config = { groupSystem = { ["invite lifetime seconds"] = 120, ["invite popups"] = true, ["summon protection"] = false } }
enumerations = { log = { INFO = 1, ERROR = 3 } }
color = { Red = "", Turquoise = "", White = "" }
jsonInterface = { load = function() return { version=1, nextId=1, groups={} } end, save = function() end }
local commands, handlers = {}, {}
customCommandHooks = { registerCommand = function(name, fn) commands[name] = fn end }
customEventHooks = {
    registerHandler = function(name, fn) handlers[name] = handlers[name] or {}; table.insert(handlers[name], fn) end,
    registerValidator = function(name, fn) end
}
tableHelper = { concatenateFromIndex = function(t, n) local r={} for i=n,#t do r[#r+1]=t[i] end return table.concat(r, " ") end }
local popups, boxes, sent = {}, {}, {}
tes3mp = {
    SendMessage = function(pid, msg) sent[#sent+1] = {pid=pid,msg=msg} end,
    CustomMessageBox = function(pid, id, msg, buttons) popups[#popups+1] = {pid=pid,id=id,msg=msg,buttons=buttons} end,
    MessageBox = function(pid, id, msg) boxes[#boxes+1] = {pid=pid,id=id,msg=msg} end,
    LogMessage = function() end,
    GetCell = function() return "Balmora" end
}
local function mk(pid, name, account)
    local p = { pid=pid, name=name, accountName=account, data={ customVariables={} } }
    function p:IsLoggedIn() return true end
    function p:QuicksaveToDrive() end
    return p
end
Players = { [0]=mk(0,"Leader","leader_acc"), [1]=mk(1,"Guest","guest_acc") }
localization = { GetLanguage = function(pid) return pid == 1 and "RU" or "RU" end }

local groupHelper = dofile("server/scripts/groupHelper.lua")
local function cmd(pid, ...) local a={...}; commands.groupui(pid, a) end
local function gui(pid, choice)
    for _,fn in ipairs(handlers.OnGUIAction or {}) do fn({}, pid, 20560001, tostring(choice)) end
end
local failures=0
local function check(name, value)
    if value then print("ok   "..name) else failures=failures+1; print("FAIL "..name) end
end

cmd(0, "groupui", "create", "Test Party")
cmd(0, "groupui", "invite", "Guest")
check("invite creates immediate popup", #popups == 1 and popups[1].pid == 1)
check("popup has Yes/No", popups[1] and popups[1].buttons == "Да;Нет")
check("popup identifies leader and group", popups[1] and popups[1].msg:find("Leader",1,true) and popups[1].msg:find("Test Party",1,true))

gui(1, 1)
check("decline notifies leader by MessageBox", boxes[#boxes] and boxes[#boxes].pid == 0 and boxes[#boxes].msg:find("отказался",1,true))
check("decline leaves guest outside group", groupHelper.GetPlayerGroup(1) == nil)

cmd(0, "groupui", "invite", "Guest")
check("second invite can be sent after decline", #popups == 2)
gui(1, 0)
check("accept adds guest to group", groupHelper.ArePlayersInSameGroup(0,1))
check("accept notifies leader by MessageBox", boxes[#boxes] and boxes[#boxes].pid == 0 and boxes[#boxes].msg:find("принял",1,true))

print(string.format("X056 group invite harness: %d failure(s)", failures))
os.exit(failures == 0 and 0 or 1)

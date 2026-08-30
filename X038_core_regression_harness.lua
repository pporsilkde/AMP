-- X035 server quest core logic harness. Runs under texlua without TES3MP.
local saved = {}
local sample = {
    schemaVersion = 1, id = "test_ring", version = 1, name = "Test ring", author = "mod",
    status = "published", progressMode = "personal",
    giver = { refId = "caius cosades", cell = "Balmora" }, initialStage = 10,
    topics = { { id = "test_ring_topic", text = "missing ring", enabled = true, green = true } },
    stages = {
        { index = 10, journal = "Find it", dialogue = "Please find it", requirements = {}, rewards = {}, next = {20}, complete = false, fail = false },
        { index = 20, journal = "Returned", dialogue = "Thanks", requirements = {}, rewards = {
            {type="gold", amount=250}, {type="xp", amount=125}}, next = {}, complete = true, fail = false }
    }, audit = {}
}

config = {
    serverQuests = { enabled = true, moderatorsCanPublish = true },
    xpLeveling = { ["xp gain multiplier"] = 2.0 },
    customMenuIds = { questEditorMain=9010,questEditorList=9011,questEditorDetail=9012,questEditorInput=9013,questPlayerList=9014,questPlayerJournal=9015 }
}
color = { SkyBlue="", Default="" }
enumerations = { log={INFO=1,WARN=2,ERROR=3}, inventory={ADD=1,REMOVE=2} }

local function deepcopy(v)
    if type(v) ~= "table" then return v end
    local r={}; for k,x in pairs(v) do r[k]=deepcopy(x) end; return r
end

tableHelper = {
    deepCopy=deepcopy,
    cleanNils=function(t) local r={} for _,v in pairs(t) do if v~=nil then table.insert(r,v) end end; for k in pairs(t) do t[k]=nil end; for i,v in ipairs(r) do t[i]=v end end,
    containsValue=function(t,v) for _,x in pairs(t or {}) do if x==v then return true end end return false end,
    concatenateFromIndex=function(t,i) local r={} for n=i,#t do table.insert(r,t[n]) end return table.concat(r," ") end
}

jsonInterface = {
    load=function(path)
        if path == "custom/quests/index.json" then return {schemaVersion=1, quests={{id="test_ring"}}} end
        if path == "custom/quests/test_ring.json" then return deepcopy(sample) end
        error("missing "..path)
    end,
    save=function(path,value) saved[path]=deepcopy(value) end
}

inventoryHelper = {}
function inventoryHelper.addItem(inv, refId, count)
    for _,x in ipairs(inv) do if x.refId==refId then x.count=x.count+count return end end
    table.insert(inv,{refId=refId,count=count})
end
function inventoryHelper.removeClosestItem(inv, refId, count)
    for i,x in ipairs(inv) do if x.refId==refId then x.count=x.count-count; if x.count<=0 then table.remove(inv,i) end; return end end
end

local registeredCommand
customCommandHooks = { registerCommand=function(name,cb) if name=="quest" then registeredCommand=cb end end }
customEventHooks = { registerHandler=function() end, registerValidator=function() end }
logicHandler = { GetChatName=function(pid) return "Player("..pid..")" end }
WorldInstance = { data={customVariables={worldFlag=1}} }

tes3mp = {
    LogMessage=function() end,
    CustomMessageBox=function() end,
    ListBox=function() end,
    InputDialog=function() end
}

local P = { accountName="mod", data={
    settings={staffRank=2}, stats={level=10,experience=0}, inventory={}, customVariables={}
}}
function P:IsServerStaff() return true end
function P:IsModerator() return true end
function P:IsAdmin() return true end
function P:IsServerOwner() return false end
function P:IsLoggedIn() return true end
function P:Message(m) self.lastMessage=m end
function P:QuicksaveToDrive() self.saves=(self.saves or 0)+1 end
function P:LoadItemChanges(items,action) self.lastItemAction=action end
function P:LoadLevel() self.levelLoads=(self.levelLoads or 0)+1 end
Players = {[1]=P}

local qsys = dofile("/mnt/data/x038_work/server/scripts/serverQuestSystem.lua")

local pass,total=0,0
local function check(name, cond)
    total=total+1
    if cond then pass=pass+1; print("OK  "..name) else print("FAIL "..name) end
end

check("quest command registered", type(registeredCommand)=="function")
check("definition loaded", qsys.quests.test_ring ~= nil)
local topics=qsys.GetAvailableTopics(1,"caius cosades","Balmora")
check("green topic available", #topics==1 and topics[1].green==true and topics[1].topicId=="test_ring_topic")
local d=qsys.GetCurrentDialogue(1,"test_ring")
check("pre-start dialogue resolves initial stage", d and d.stage==10 and d.text=="Please find it")
local ok=qsys.StartQuest(1,"test_ring",false)
check("published quest starts", ok==true and qsys.GetPlayerState(1,"test_ring").stage==10)
local ok2=qsys.AdvanceQuest(1,"test_ring",20,false)
local st=qsys.GetPlayerState(1,"test_ring")
local gold=0; for _,x in ipairs(P.data.inventory) do if x.refId=="gold_001" then gold=x.count end end
check("stage advance completes", ok2==true and st.state=="completed" and st.stage==20)
check("gold reward applied once", gold==250)
check("xp reward respects global XP speed", P.data.stats.experience==250 and (P.levelLoads or 0)==1)
check("journal appended", #st.journal==2 and st.journal[2].text=="Returned")
local ok3=qsys.AdvanceQuest(1,"test_ring",20,false)
check("completed quest cannot advance normally", ok3==false)

local broken=deepcopy(sample); broken.id="broken"; broken.stages[1].rewards={{type="mystery",amount=1}}
local errors=qsys.ValidateQuest(broken)
check("unknown reward rejected by validation", #errors>0)

local P2 = { accountName="player", data={settings={staffRank=0},stats={level=1,experience=0},inventory={},customVariables={}} }
function P2:IsServerStaff() return false end
function P2:IsModerator() return false end
function P2:IsAdmin() return false end
function P2:IsServerOwner() return false end
function P2:IsLoggedIn() return true end
function P2:Message(m) self.lastMessage=m end
function P2:QuicksaveToDrive() end
Players[2]=P2
registeredCommand(2,{"quest","new","illegal","Should","Not","Exist"})
check("ordinary player cannot create quest", qsys.quests.illegal==nil)
registeredCommand(1,{"quest","new","staff_created","Staff","Quest"})
check("moderator can create draft", qsys.quests.staff_created~=nil and qsys.quests.staff_created.status=="draft")

print(string.format("RESULT %d/%d",pass,total))
if pass~=total then os.exit(1) end

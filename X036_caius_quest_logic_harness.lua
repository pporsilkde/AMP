-- X036 built-in Caius drink quest + hidden dialogue transport harness.
local saved, transports = {}, {}
local quest = dofile == nil and nil or nil

config = {
    serverQuests = { enabled = true, moderatorsCanPublish = true },
    xpLeveling = { ["xp gain multiplier"] = 2.0 },
    customMenuIds = { questEditorMain=9010,questEditorList=9011,questEditorDetail=9012,questEditorInput=9013,questPlayerList=9014,questPlayerJournal=9015 }
}
color = { SkyBlue="", Default="" }
enumerations = { log={INFO=1,WARN=2,ERROR=3}, inventory={ADD=1,REMOVE=2}, dialogueChoice={TOPIC=0} }

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

local caius = {
  schemaVersion=1,id="arena_caius_drink",version=1,name="Выпивка для Кая",author="X036",status="published",progressMode="personal",
  giver={refId="caius cosades",cell="Balmora, Caius Cosades' House"},initialStage=10,
  topics={{id="arena_caius_drink_topic",text="немного выпивки",enabled=true,green=true}},
  stages={
    {index=10,journal="Кай просит мацт",dialogue="Принеси мацт",requirements={},rewards={},next={20},complete=false,fail=false},
    {index=20,journal="Я принёс мацт",dialogue="Вот это другое дело",requirements={{type="item",refId="potion_mazte_01",operator=">=",count=1,value=1}},
     rewards={{type="takeItem",refId="potion_mazte_01",count=1},{type="gold",amount=75},{type="xp",amount=40}},next={},complete=true,fail=false}
  },audit={}
}

jsonInterface = {
    load=function(path)
        if path == "custom/quests/index.json" then return {schemaVersion=1,quests={{id="arena_caius_drink"}}} end
        if path == "custom/quests/arena_caius_drink.json" then return deepcopy(caius) end
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

customCommandHooks = { registerCommand=function() end }
local validators, handlers = {}, {}
customEventHooks = {
    registerValidator=function(name,cb) validators[name]=cb end,
    registerHandler=function(name,cb) handlers[name]=handlers[name] or {}; table.insert(handlers[name],cb) end,
    makeEventStatus=function(a,b) return {validDefaultHandler=a,validCustomHandlers=b} end
}
logicHandler = { GetChatName=function(pid) return "Player("..pid..")" end }
WorldInstance = { data={customVariables={}} }

tes3mp = {
    LogMessage=function() end,
    CustomMessageBox=function(pid,id,label,buttons) if id==-35036 then table.insert(transports,label) end end,
    ListBox=function() end,
    InputDialog=function() end
}

local P = { accountName="player", data={settings={staffRank=0},stats={level=10,experience=0},inventory={},customVariables={}} }
function P:IsServerStaff() return false end
function P:IsModerator() return false end
function P:IsAdmin() return false end
function P:IsServerOwner() return false end
function P:IsLoggedIn() return true end
function P:Message(m) self.lastMessage=m end
function P:QuicksaveToDrive() self.saves=(self.saves or 0)+1 end
function P:LoadItemChanges(items,action) self.lastItemAction=action end
function P:LoadLevel() self.levelLoads=(self.levelLoads or 0)+1 end
Players = {[1]=P}

local qsys = dofile("/mnt/data/x036_work/server/scripts/serverQuestSystem.lua")
local pass,total=0,0
local function check(name, cond) total=total+1; if cond then pass=pass+1; print("OK  "..name) else print("FAIL "..name) end end
local function countItem(id)
    for _,x in ipairs(P.data.inventory) do if x.refId==id then return x.count end end
    return 0
end
local function click()
    local objects={ ["1-0"]={refId="caius cosades",dialogueChoiceType=enumerations.dialogueChoice.TOPIC,dialogueTopic="@ArenaQuest:arena_caius_drink:arena_caius_drink_topic"} }
    local st={validDefaultHandler=true,validCustomHandlers=true}
    local v=validators.OnObjectDialogueChoice(st,1,"Balmora, Caius Cosades' House",objects)
    check("custom topic suppresses vanilla echo", v and v.validDefaultHandler==false and v.validCustomHandlers==true)
    for _,h in ipairs(handlers.OnObjectDialogueChoice or {}) do h(v or st,1,"Balmora, Caius Cosades' House",objects) end
end

check("published Caius quest loaded", qsys.quests.arena_caius_drink and qsys.quests.arena_caius_drink.status=="published")
local errors=qsys.ValidateQuest(qsys.quests.arena_caius_drink)
check("Caius quest validates", #errors==0)
local topics=qsys.GetAvailableTopics(1,"caius cosades","Balmora, Caius Cosades' House")
check("green topic available", #topics==1 and topics[1].text=="немного выпивки" and topics[1].green==true)
click()
local state=qsys.GetPlayerState(1,"arena_caius_drink")
check("first click starts stage 10", state and state.stage==10 and state.state=="active")
local sawStage10=false
for _,x in ipairs(transports) do if x:match("^RESPONSE") and x:find("10",1,true) then sawStage10=true end end
check("first response transported", sawStage10)

inventoryHelper.addItem(P.data.inventory,"potion_mazte_01",1)
click()
state=qsys.GetPlayerState(1,"arena_caius_drink")
check("second click with Mazte completes", state and state.stage==20 and state.state=="completed")
check("Mazte consumed", countItem("potion_mazte_01")==0)
check("75 gold rewarded", countItem("gold_001")==75)
check("40 XP respects 2x server multiplier", P.data.stats.experience==80)
check("completed green topic disappears", #qsys.GetAvailableTopics(1,"caius cosades","Balmora, Caius Cosades' House")==0)
local sawResponse=false
for _,x in ipairs(transports) do if x:match("^RESPONSE") and x:find("20",1,true) then sawResponse=true end end
check("completion response sent through hidden transport", sawResponse)

print(string.format("RESULT %d/%d",pass,total))
if pass~=total then os.exit(1) end

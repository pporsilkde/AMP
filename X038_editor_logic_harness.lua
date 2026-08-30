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
    customMenuIds = { questEditorMain=9010,questEditorList=9011,questEditorDetail=9012,questEditorInput=9013,questPlayerList=9014,questPlayerJournal=9015,
        questEditorTopics=9016,questEditorTopicList=9017,questEditorTopicDetail=9018,questEditorOffer=9019,questEditorChoices=9020,questEditorChoiceList=9021,questEditorChoiceDetail=9022,questEditorStages=9023,questEditorStageList=9024,questEditorStageDetail=9025,questEditorRequirements=9026,questEditorRequirementList=9027,questEditorRewards=9028,questEditorRewardList=9029,questEditorTransitions=9030,questEditorTransitionList=9031,questEditorConfirm=9032,questEditorStageFlags=9033,questEditorGiver=9034 }
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
local handlers={}
customEventHooks = { registerHandler=function(name,cb) handlers[name]=cb end, registerValidator=function() end }
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
 total=total+1; if cond then pass=pass+1; print("OK  "..name) else print("FAIL "..name) end
end
check("OnGUIAction registered", type(handlers.OnGUIAction)=="function")
check("OnObjectActivate picker registered", type(handlers.OnObjectActivate)=="function")

qsys.editor[1]={questId="test_ring",pickGiver=true}
handlers.OnObjectActivate({},1,"Balmora, Guild of Mages",{["42-7"]={refId="ranis athrys",activatingPid=1}}, {})
local q=qsys.quests.test_ring
check("in-game giver picker captures refId", q.giver.refId=="ranis athrys")
check("in-game giver picker captures cell", q.giver.cell=="Balmora, Guild of Mages")
check("in-game giver picker captures uniqueIndex", q.giver.uniqueIndex=="42-7")
check("picker auto-disarms", qsys.editor[1].pickGiver==false)
check("editing published quest returns to draft", q.status=="draft")

qsys.editor[1].input="stage_add"
handlers.OnGUIAction({},1,config.customMenuIds.questEditorInput,"30 | Editor journal | Editor dialogue")
local st30=nil; for _,s in ipairs(q.stages) do if tonumber(s.index)==30 then st30=s end end
check("visual editor adds stage", st30~=nil and st30.journal=="Editor journal")

qsys.editor[1].stageIndex=30
qsys.editor[1].choiceSource=30
qsys.editor[1].input="choice_add"
handlers.OnGUIAction({},1,config.customMenuIds.questEditorInput,"finish | advance | 20 | Finish it")
check("visual editor adds choice", st30 and #st30.choices==1 and st30.choices[1].id=="finish" and st30.choices[1].targetStage==20)

qsys.editor[1].choiceIndex=1
qsys.editor[1].requirementOwner="choice"
qsys.editor[1].input="requirement_add"
handlers.OnGUIAction({},1,config.customMenuIds.questEditorInput,"item | potion_mazte_01 | >= | 1")
check("choice requirement added", #st30.choices[1].requirements==1 and st30.choices[1].requirements[1].refId=="potion_mazte_01")

qsys.editor[1].requirementOwner="stage"
qsys.editor[1].input="reward_add"
handlers.OnGUIAction({},1,config.customMenuIds.questEditorInput,"gold | 99")
check("stage reward added", #st30.rewards==1 and st30.rewards[1].amount==99)

handlers.OnGUIAction({},1,config.customMenuIds.questEditorStageFlags,"0")
check("stage can be set initial from UI", tonumber(q.initialStage)==30)
handlers.OnGUIAction({},1,config.customMenuIds.questEditorStageFlags,"1")
check("complete flag toggles", st30.complete==true and st30.fail==false)
handlers.OnGUIAction({},1,config.customMenuIds.questEditorStageFlags,"2")
check("fail flag is exclusive", st30.fail==true and st30.complete==false)

print(string.format("RESULT %d/%d",pass,total))
if pass~=total then os.exit(1) end

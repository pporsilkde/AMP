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


local transport = {}
tes3mp.CustomMessageBox=function(pid,id,label,buttons)
    if id == -35036 then table.insert(transport,label) end
end

local qsys = dofile("/mnt/data/x039_work/server/scripts/serverQuestSystem.lua")
local pass,total=0,0
local function check(name, cond)
 total=total+1; if cond then pass=pass+1; print("OK  "..name) else print("FAIL "..name) end
end

check("OnGUIAction registered", type(handlers.OnGUIAction)=="function")
check("OnObjectActivate picker registered", type(handlers.OnObjectActivate)=="function")
check("SyncEditor API exists", type(qsys.SyncEditor)=="function")

transport={}
registeredCommand(1,{"quest"})
local sawEnd=false
for _,line in ipairs(transport) do if line=="EDITOR_END" then sawEnd=true end end
check("/quest opens hidden MyGUI editor transport", sawEnd)

local function cmd(...)
    local fields={"EDITOR_CMD",...}
    handlers.OnGUIAction({},1,-35036,table.concat(fields,"\t"))
end

cmd("overview","test_ring","Test ring edited","personal","caius cosades","Balmora","42-7","10")
local q=qsys.quests.test_ring
check("overview edit applied", q.name=="Test ring edited" and q.giver.uniqueIndex=="42-7")
check("published edit returns to draft", q.status=="draft")

cmd("topic_upsert","test_ring","","green_help","green help","true")
local found=false
for _,t in ipairs(q.topics) do if t.id=="green_help" and t.text=="green help" then found=true end end
check("MyGUI topic added",found)

cmd("stage_upsert","test_ring","0","30","Editor journal","Editor dialogue","0","0","0")
local st30=nil; for _,st in ipairs(q.stages) do if tonumber(st.index)==30 then st30=st end end
check("MyGUI stage added",st30~=nil and st30.journal=="Editor journal")

cmd("choice_upsert","test_ring","stage","30","","finish","advance","20","Finish it")
check("MyGUI choice added",st30 and #st30.choices==1 and st30.choices[1].id=="finish")

cmd("require_add","test_ring","stageChoice","30","finish","item",">=","1","potion_mazte_01")
check("choice requirement added",#st30.choices[1].requirements==1 and st30.choices[1].requirements[1].refId=="potion_mazte_01")

cmd("reward_add","test_ring","30","gold","99","")
check("stage reward added",#st30.rewards==1 and st30.rewards[1].amount==99)

cmd("next_add","test_ring","30","20")
check("transition added",#st30.next==1 and tonumber(st30.next[1])==20)

cmd("pick_giver","test_ring")
check("picker armed from MyGUI",qsys.editor[1].pickGiver==true)
handlers.OnObjectActivate({},1,"Balmora, Guild of Mages",{["77-9"]={refId="ranis athrys",activatingPid=1}}, {})
check("picker captures and disarms",q.giver.refId=="ranis athrys" and q.giver.uniqueIndex=="77-9" and qsys.editor[1].pickGiver==false)

transport={}
qsys.SyncEditor(1,"test_ring","Ready")
local sawQuest,sawStage,sawReq,sawReward=false,false,false,false
for _,line in ipairs(transport) do
    if line:match("^EDITOR_QUEST") then sawQuest=true end
    if line:match("^EDITOR_STAGE") then sawStage=true end
    if line:match("^EDITOR_REQ") then sawReq=true end
    if line:match("^EDITOR_REWARD") then sawReward=true end
end
check("editor sync contains model",sawQuest and sawStage and sawReq and sawReward)

print(string.format("RESULT %d/%d",pass,total))
if pass~=total then os.exit(1) end

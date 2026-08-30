-- X042 vocabulary + safe merge harness. Runs under texlua without TES3MP.
local saved, transports, calls = {}, {}, {}
config = {
  serverQuests={enabled=true,moderatorsCanPublish=true},
  serverQuestVanillaJournalWhitelist={"A1_1_FindSpymaster"},
  xpLeveling={ ["xp gain multiplier"] = 1.0 },
  customMenuIds={questEditorMain=9010,questEditorList=9011,questEditorDetail=9012,questEditorInput=9013,questPlayerList=9014,questPlayerJournal=9015,
    questEditorTopics=9016,questEditorTopicList=9017,questEditorTopicDetail=9018,questEditorOffer=9019,questEditorChoices=9020,questEditorChoiceList=9021,questEditorChoiceDetail=9022,questEditorStages=9023,questEditorStageList=9024,questEditorStageDetail=9025,questEditorRequirements=9026,questEditorRequirementList=9027,questEditorRewards=9028,questEditorRewardList=9029,questEditorTransitions=9030,questEditorTransitionList=9031,questEditorConfirm=9032,questEditorStageFlags=9033,questEditorGiver=9034}
}
color={SkyBlue="",Default=""}
enumerations={log={INFO=1,WARN=2,ERROR=3},inventory={ADD=1,REMOVE=2},spellbook={SET=0,ADD=1,REMOVE=2}}
local function deepcopy(v) if type(v)~='table' then return v end local r={} for k,x in pairs(v) do r[k]=deepcopy(x) end return r end

tableHelper={
 deepCopy=deepcopy,
 cleanNils=function(t) local r={} for _,v in pairs(t) do if v~=nil then r[#r+1]=v end end for k in pairs(t) do t[k]=nil end for i,v in ipairs(r) do t[i]=v end end,
 containsValue=function(t,v) for _,x in pairs(t or {}) do if x==v then return true end end return false end,
 containsCaseInsensitiveString=function(t,v) local w=tostring(v):lower(); for _,x in pairs(t or {}) do if tostring(x):lower()==w then return true end end return false end,
 removeValue=function(t,v) for i=#t,1,-1 do if t[i]==v then table.remove(t,i) end end end,
 concatenateFromIndex=function(t,i) local r={} for n=i,#t do r[#r+1]=t[n] end return table.concat(r,' ') end
}

local vocab={schemaVersion=1,id='vocab_test',version=1,name='Vocabulary test',author='admin',status='published',progressMode='personal',adminApprovedDangerousRewards=true,
 giver={refId='caius cosades',cell='Balmora'},initialStage=10,topics={{id='vocab_topic',text='vocabulary',enabled=true,green=true}},
 stages={
  {index=10,journal='start',dialogue='start',requirements={},rewards={},next={20},complete=false,fail=false,choices={}},
  {index=20,journal='done',dialogue='done',requirements={},rewards={
    {type='setServerVariable',key='worldQuestFlag',value=7},
    {type='addSpell',refId='firebite'},
    {type='removeSpell',refId='old_spell'},
    {type='setReputation',value=23},
    {type='setBounty',value=17},
    {type='teleport',cell='Balmora, Guild of Mages',posX=1,posY=2,posZ=3},
    {type='playSound',refId='Fx\\envrn\\water.wav'},
    {type='messageBox',text='Quest done'},
    {type='setVanillaJournal',questId='a1_1_findspymaster',index=15}
  },next={},complete=true,fail=false,choices={}}
 },audit={}}
local gate={schemaVersion=1,id='admin_gate',version=1,name='Admin gate',author='mod',status='draft',progressMode='personal',
 giver={refId='caius cosades',cell='Balmora'},initialStage=10,topics={{id='gate_topic',text='gate',enabled=true,green=true}},
 stages={{index=10,journal='x',dialogue='x',requirements={},rewards={{type='setServerVariable',key='danger',value=1}},next={},complete=true,fail=false,choices={}}},audit={}}

jsonInterface={
 load=function(path)
  if path=='custom/quests/index.json' then return {schemaVersion=1,quests={{id='vocab_test'},{id='admin_gate'}}} end
  if path=='custom/quests/vocab_test.json' then return deepcopy(vocab) end
  if path=='custom/quests/admin_gate.json' then return deepcopy(gate) end
  error('missing '..path)
 end,
 save=function(path,value) saved[path]=deepcopy(value) end
}
inventoryHelper={}
function inventoryHelper.addItem(inv,refId,count) for _,x in ipairs(inv) do if x.refId==refId then x.count=x.count+count return end end inv[#inv+1]={refId=refId,count=count} end
function inventoryHelper.removeClosestItem(inv,refId,count) for i,x in ipairs(inv) do if x.refId==refId then x.count=x.count-count if x.count<=0 then table.remove(inv,i) end return end end end
local registeredCommand
customCommandHooks={registerCommand=function(name,cb) if name=='quest' then registeredCommand=cb end end}
local handlers={}
customEventHooks={registerHandler=function(n,cb) handlers[n]=cb end,registerValidator=function() end}
logicHandler={GetChatName=function(pid) return 'Player'..pid end,TeleportToCell=function(pid,cell,x,y,z) calls.teleport={pid,cell,x,y,z} end}
WorldInstance={data={customVariables={},clientVariables={globals={Day={variableType=1,intValue=33}}}},saves=0}
function WorldInstance:QuicksaveToDrive() self.saves=self.saves+1 end

tes3mp={LogMessage=function() end,ListBox=function() end,InputDialog=function() end,
 CustomMessageBox=function(pid,id,label,buttons) if id==-35036 then transports[#transports+1]=label end calls.message=label end,
 ClearSpellbookChanges=function(pid) calls.clearSpell=true end,
 SetSpellbookChangesAction=function(pid,a) calls.spellActions=calls.spellActions or {}; calls.spellActions[#calls.spellActions+1]=a end,
 AddSpell=function(pid,id) calls.spells=calls.spells or {}; calls.spells[#calls.spells+1]=id end,
 SendSpellbookChanges=function(pid) calls.sendSpell=true end,
 SetReputation=function(pid,v) calls.rep=v end,SendReputation=function(pid) calls.sendRep=true end,
 SetBounty=function(pid,v) calls.bounty=v end,SendBounty=function(pid) calls.sendBounty=true end,
 PlaySpeech=function(pid,s) calls.sound=s end,
 ClearJournalChanges=function(pid) calls.clearJournal=true end,
 AddJournalEntry=function(pid,q,i,a) calls.journal={q,i,a} end,
 SendJournalChanges=function(pid) calls.sendJournal=true end
}
local admin=false
local P={accountName='mod',data={settings={staffRank=2},stats={level=12,experience=0},fame={bounty=9,reputation=21},inventory={},customVariables={},
 skills={['Long Blade']={base=45}},attributes={Strength={base=60}},factionRanks={['Fighters Guild']=3},character={race='Dark Elf',class='Warrior'},location={cell='Balmora'},
 journal={{quest='A1_1_FindSpymaster',index=14}},spellbook={'old_spell'}}}
function P:IsServerStaff() return true end; function P:IsModerator() return true end; function P:IsAdmin() return admin end; function P:IsServerOwner() return false end
function P:IsLoggedIn() return true end; function P:Message(m) self.lastMessage=m end; function P:QuicksaveToDrive() self.saves=(self.saves or 0)+1 end
function P:LoadItemChanges() end; function P:LoadLevel() end
Players={[1]=P}

local qsys=dofile('/mnt/data/x042_final/server/scripts/serverQuestSystem.lua')
local pass,total=0,0
local function check(n,c) total=total+1 if c then pass=pass+1 print('OK  '..n) else print('FAIL '..n) end end
local function req(r) local ok=qsys.CheckRequirement(1,nil,r); return ok end
check('skill case-insensitive',req{type='skill',key='long blade',operator='>=',value=40})
check('attribute case-insensitive',req{type='attribute',key='strength',operator='>=',value=50})
check('faction rank case-insensitive',req{type='factionRank',key='fighters guild',operator='>=',value=3})
check('reputation uses fame',req{type='reputation',operator='>=',value=20})
check('bounty uses fame',req{type='bounty',operator='==',value=9})
check('race case-insensitive',req{type='race',value='dark elf'})
check('cell case-insensitive',req{type='cell',value='balmora'})
check('vanilla journal read',req{type='vanillaJournal',questId='a1_1_findspymaster',operator='>=',value=14})
check('global read case-insensitive',req{type='global',key='day',operator='==',value=33})
local errors=qsys.ValidateQuest(qsys.quests.vocab_test)
check('approved vocabulary quest validates',#errors==0)
local ok=qsys.StartQuest(1,'vocab_test',true); check('start vocabulary quest',ok)
ok=qsys.AdvanceQuest(1,'vocab_test',20,true); check('advance vocabulary quest',ok)
check('server variable persisted',WorldInstance.data.customVariables.worldQuestFlag==7 and WorldInstance.saves>0)
check('spellbook action API correct',calls.spellActions and calls.spellActions[1]==enumerations.spellbook.ADD and calls.spellActions[2]==enumerations.spellbook.REMOVE)
check('spellbook state updated',P.data.spellbook[1]=='firebite' and #P.data.spellbook==1)
check('reputation reward uses fame',P.data.fame.reputation==23 and calls.rep==23)
check('bounty reward uses fame',P.data.fame.bounty==17 and calls.bounty==17)
check('teleport reward',calls.teleport and calls.teleport[2]=='Balmora, Guild of Mages')
check('vanilla journal whitelisted action',calls.journal and calls.journal[1]=='a1_1_findspymaster' and calls.journal[2]==15)
admin=false; registeredCommand(1,{'quest','publish','admin_gate'})
check('moderator cannot publish dangerous reward',qsys.quests.admin_gate.status~='published' and qsys.quests.admin_gate.adminApprovedDangerousRewards~=true)
admin=true; registeredCommand(1,{'quest','publish','admin_gate'})
check('admin can approve and publish dangerous reward',qsys.quests.admin_gate.status=='published' and qsys.quests.admin_gate.adminApprovedDangerousRewards==true)
print(string.format('RESULT %d/%d',pass,total))
if pass~=total then os.exit(1) end

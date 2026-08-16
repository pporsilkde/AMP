require("config")
fileHelper = require("fileHelper")
tableHelper = require("tableHelper")
local CoreArenaMP_DataManager = require("CoreArenaMP_DataManager")
local BaseRecordStore = require("recordstore.base")

local RecordStore = class("RecordStore", BaseRecordStore)

function RecordStore:__init(storeType)
    BaseRecordStore.__init(self, storeType)

    -- Ensure filename is valid
    self.recordstoreFile = storeType .. ".json"

    if self.hasEntry == nil then
        self.hasEntry = CoreArenaMP_DataManager.Exists("recordstore/" .. self.recordstoreFile)
    end
end

function RecordStore:CreateEntry()
    CoreArenaMP_DataManager.Save("recordstore/" .. self.recordstoreFile, self.data)
    self.hasEntry = true
end

function RecordStore:SaveToDrive()
    if self.hasEntry then
        CoreArenaMP_DataManager.Save("recordstore/" .. self.recordstoreFile, self.data, config.recordstoreKeyOrder)
    end
end

function RecordStore:QuicksaveToDrive()
    if self.hasEntry then
        CoreArenaMP_DataManager.Quicksave("recordstore/" .. self.recordstoreFile, self.data)
    end
end

function RecordStore:LoadFromDrive()
    self.data = CoreArenaMP_DataManager.Load("recordstore/" .. self.recordstoreFile)

    if self.data == nil then
        tes3mp.LogMessage(enumerations.log.ERROR, "recordstore/" .. self.recordstoreFile .. " cannot be read!")
        tes3mp.StopServer(2)
    else
        -- JSON doesn't allow numerical keys, but we use them, so convert
        -- all string number keys into numerical keys
        tableHelper.fixNumericalKeys(self.data)
    end
end

-- Deprecated functions with confusing names, kept around for backwards compatibility
function RecordStore:Save()
    self:SaveToDrive()
end

function RecordStore:Load()
    self:LoadFromDrive()
end

return RecordStore

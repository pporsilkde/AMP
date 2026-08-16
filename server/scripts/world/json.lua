require("config")
tableHelper = require("tableHelper")
local CoreArenaMP_DataManager = require("CoreArenaMP_DataManager")
local BaseWorld = require("world.base")

local World = class("World", BaseWorld)

function World:__init()
    BaseWorld.__init(self)

    self.coreVariablesFile = "coreVariables.json"
    self.worldFile = "world.json"

    if self.hasEntry == nil then
        self.hasEntry = CoreArenaMP_DataManager.Exists("world/" .. self.worldFile)
    end
end

function World:CreateEntry()
    CoreArenaMP_DataManager.Save("world/" .. self.coreVariablesFile, self.coreVariables)
    CoreArenaMP_DataManager.Save("world/" .. self.worldFile, self.data)
    self.hasEntry = true
end

function World:SaveToDrive()
    if self.hasEntry then
        CoreArenaMP_DataManager.Save("world/" .. self.coreVariablesFile, self.coreVariables)
        CoreArenaMP_DataManager.Save("world/" .. self.worldFile, self.data, config.worldKeyOrder)
    end
end

function World:QuicksaveToDrive()
    if self.hasEntry then
        CoreArenaMP_DataManager.Quicksave("world/" .. self.coreVariablesFile, self.coreVariables)
        CoreArenaMP_DataManager.Quicksave("world/" .. self.worldFile, self.data)
    end
end

function World:QuicksaveCoreVariablesToDrive()
    if self.hasEntry then
        CoreArenaMP_DataManager.Quicksave("world/" .. self.coreVariablesFile, self.coreVariables)
    end
end

function World:LoadFromDrive()
    self.coreVariables = CoreArenaMP_DataManager.Load("world/" .. self.coreVariablesFile)
    self.data = CoreArenaMP_DataManager.Load("world/" .. self.worldFile)

    if self.data == nil then
        tes3mp.LogMessage(enumerations.log.ERROR, "world/" .. self.worldFile .. " cannot be read!")
        tes3mp.StopServer(2)
    else
        -- JSON doesn't allow numerical keys, but we use them, so convert
        -- all string number keys into numerical keys
        tableHelper.fixNumericalKeys(self.data)
    end
end


-- Deprecated functions with confusing names, kept around for backwards compatibility
function World:Save()
    self:SaveToDrive()
end

function World:Load()
    self:LoadFromDrive()
end

return World

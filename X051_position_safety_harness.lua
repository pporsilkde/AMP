package.path = "./server/scripts/?.lua;" .. package.path

config = {
    positionSafety = {
        enabled = true,
        autosaveSeconds = 10,
        loginGuardEnabled = true,
        loginGuardDelaysMs = {400, 1200, 3000},
        loginGuardFallThreshold = 24,
        loginGuardMoveCancelDistance = 96,
        maxAbsCoordinate = 100000000
    }
}

enumerations = { log = { WARN = 2, INFO = 1 } }
local authHandler
customEventHooks = {
    registerHandler = function(name, fn)
        if name == "OnPlayerAuthentified" then authHandler = fn end
    end
}

local now = 100
os.time = function() return now end

local native = {cell="Balmora", x=100, y=200, z=300, rx=0.1, rz=0.2}
local sent = 0
local timers = {}
tes3mp = {
    GetPosX=function() return native.x end, GetPosY=function() return native.y end,
    GetPosZ=function() return native.z end, GetRotX=function() return native.rx end,
    GetRotZ=function() return native.rz end, GetCell=function() return native.cell end,
    SetPos=function(_,x,y,z) native.x,native.y,native.z=x,y,z end,
    SetRot=function(_,rx,rz) native.rx,native.rz=rx,rz end,
    SendPos=function() sent=sent+1 end,
    LogAppend=function() end,
    CreateTimerEx=function(name, delay, types, pid, account)
        timers[#timers+1]={name=name,delay=delay,pid=pid,account=account}
        return #timers
    end,
    StartTimer=function() end
}

local player = {
    accountName="tester", hasAccount=true, loggedIn=true,
    data={location={cell="Balmora", posX=100,posY=200,posZ=300,rotX=0.1,rotZ=0.2}},
    quicksaves=0,
    IsLoggedIn=function(self) return self.loggedIn end,
    QuicksaveToDrive=function(self) self.quicksaves=self.quicksaves+1 end
}
Players={[1]=player}

local h=require("positionSafetyHelper")
assert(authHandler ~= nil)
assert(h.CacheCurrentPosition(1) == true)
assert(player.data.location.posZ == 300)
assert(player.quicksaves == 1)

authHandler({},1)
assert(player.loginPositionGuard.active == true)
assert(#timers == 3)

-- Simulate falling through unloaded collision. Bad Z must not replace saved Z.
native.z=200
assert(h.CacheCurrentPosition(1) == false)
assert(player.data.location.posZ == 300)

-- First guard tick restores the saved transform.
assert(h.ReassertLoginPosition(1,"tester") == true)
assert(native.z == 300 and sent == 1)

-- Horizontal player movement cancels the guard rather than snapping back.
native.x=250
native.z=300
assert(h.CacheCurrentPosition(1) == true)
assert(player.loginPositionGuard.active == false)

-- Disconnect flushing must use cached data and never resample native location.
native.z=-9999
local before=player.data.location.posZ
assert(h.FlushCachedPosition(1) == true)
assert(player.data.location.posZ == before)

print("X051 position safety harness: OK")

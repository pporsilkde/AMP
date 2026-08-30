local modulePath = "/mnt/data/x046_impl/server/scripts/questIndexStore.lua"

local function makeEnv(fileExists, stored, refreshFlag)
    local sent = {}
    local saved = {}
    local env = {
        config = {
            questIndexRefreshOnServerStart = refreshFlag,
            questIndexRequireQuorum = false,
            questIndexVerifyOnLogin = false
        },
        enumerations = { log = { INFO = 1, WARN = 2, ERROR = 3 } },
        Players = {},
        logicHandler = { GetChatName = function(pid) return "Player" .. tostring(pid) end },
        jsonInterface = {
            load = function(path) return stored end,
            save = function(path, value) saved[#saved + 1] = { path = path, value = value } end
        },
        tes3mp = {
            DoesFileExist = function(path) return fileExists end,
            GetModDir = function() return "/server/data" end,
            LogMessage = function(level, text) end,
            SendQuestIndexRequest = function(pid, mode) sent[#sent + 1] = { pid = pid, mode = mode } end
        },
        os = os, math = math, string = string, table = table, tostring = tostring,
        tonumber = tonumber, ipairs = ipairs, pairs = pairs, type = type,
        pcall = pcall, require = require, print = print
    }
    env._G = env
    env.Players[1] = {
        accountName = "tester",
        IsLoggedIn = function() return true end,
        IsServerStaff = function() return false end
    }
    return env, sent, saved
end

local function loadModule(env)
    local chunk, err = loadfile(modulePath, "t", env)
    assert(chunk, err)
    return chunk()
end

-- Existing valid DB: must be reused even if a legacy config still says refresh=true.
do
    local env, sent, saved = makeEnv(true, nil, true)
    local mod = loadModule(env)
    local entries = { "a", "b" }
    env.jsonInterface.load = function(path)
        return {
            contentKey = "0123456789abcdef",
            indexHash = mod.Hash(table.concat(entries, "\n")),
            entries = entries
        }
    end
    mod.OnServerStart()
    local status = mod.GetStatus()
    assert(status.trusted == true, "valid stored DB was not trusted")
    assert(status.entryCount == 2, "stored entry count mismatch")
    assert(#saved == 0, "startup rewrote a valid DB")
    mod.OnPlayerReady(1)
    assert(#sent == 1 and sent[1].mode == 0, "existing DB should send MODE_OFF, not upload")
end

-- Missing DB: first ready client must be asked for exactly one upload.
do
    local env, sent = makeEnv(false, nil, false)
    local mod = loadModule(env)
    mod.OnServerStart()
    assert(mod.GetStatus().trusted == false, "missing DB unexpectedly trusted")
    mod.OnPlayerReady(1)
    assert(#sent == 1 and sent[1].mode == 2, "missing DB should request MODE_UPLOAD")
end

print("X046 quest index persistence harness: ALL OK")

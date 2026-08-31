-- ArenaMP Core: shared logging and JSON bootstrap helper.
--
-- Two jobs:
--
--   1. One log identity. Every ArenaMP-owned script used to stamp its own
--      module tag and, worse, the patch number it happened to ship in
--      ("[groupHelper] X050 loaded", "[X031 ConfigReload] Watching ...").
--      Those tags went stale the moment the next patch landed and made the
--      server log read like a changelog. Everything now logs as
--      "[ArenaMP Core]" and says what happened instead of when it was added.
--
--   2. One place that guarantees a JSON data file exists. jsonInterface.load
--      returns nil for a missing file, and on Windows the underlying io2
--      library prints its own "Cannot open ... in mode r" line to stdout
--      before that nil ever reaches Lua, so a pcall around the load cannot
--      silence it. The only real fix is to make sure the file is on disk, so
--      EnsureJson writes the default the first time and the message never
--      comes back.
--
-- Load order: this module needs jsonInterface to already have its io library
-- selected, so serverCore requires it immediately after that selection.

local ampCore = {}

ampCore.LOG_TAG = "[ArenaMP Core]"

-- Data files ArenaMP owns and wants present before any helper reads them.
-- EnsureRegisteredFiles walks this on OnServerPostInit.
local registeredFiles = {}

--- Strip a leading module tag and any bare patch token from a message.
-- "[groupHelper] X050 loaded: groups" -> "loaded: groups"
-- "[X031 ConfigReload] Watching x"    -> "Watching x"
-- A message that carries no tag is returned untouched, so callers can pass
-- plain text without thinking about it.
function ampCore.StripLegacyTag(message)
    message = tostring(message or "")
    -- Only one leading bracketed tag is removed. Chat-facing tags such as
    -- [Group] or [Local] never reach this function; they are sent through
    -- tes3mp.SendMessage, not through the log.
    message = message:gsub("^%s*%[[%w%s_%-]+%]%s*", "")
    -- A patch token that survived as a bare word at the start of the text.
    message = message:gsub("^[Xx]%d%d%d[a-z]?%s+", "")
    return message
end

--- Write one line to the server log under the shared ArenaMP identity.
function ampCore.Log(level, message)
    tes3mp.LogMessage(level, ampCore.LOG_TAG .. " " .. ampCore.StripLegacyTag(message))
end

--- Convenience wrapper so a helper can keep its existing local log() shape:
---   local log = ampCore.Logger()
function ampCore.Logger()
    return function(level, message)
        ampCore.Log(level, message)
    end
end

function ampCore.LogInfo(message) ampCore.Log(enumerations.log.INFO, message) end
function ampCore.LogWarn(message) ampCore.Log(enumerations.log.WARN, message) end
function ampCore.LogError(message) ampCore.Log(enumerations.log.ERROR, message) end

--- Absolute path of a file inside server/data.
function ampCore.ResolveDataPath(relativePath)
    return tostring(config.dataPath or ".") .. "/" .. tostring(relativePath or "")
end

--- Positive-only existence probe.
--
-- Returns true when the standard Lua io library can open the file. A false
-- result means "unknown", not "missing": on Windows the data path can contain
-- non-ASCII characters (a user profile such as C:\Users\Зэро\...), and plain
-- io cannot open those at all. That is the entire reason TES3MP swaps in io2.
-- Treating a false here as proof of absence would let EnsureJson overwrite a
-- perfectly good file, so callers must fall back to an actual load attempt.
function ampCore.FileDefinitelyExists(relativePath)
    local ok, file = pcall(io.open, ampCore.ResolveDataPath(relativePath), "r")
    if ok and file ~= nil then
        file:close()
        return true
    end
    return false
end

--- Best-effort directory creation, used only after a write has already failed.
-- Never called on the happy path, because spawning a shell on every server
-- start to create a directory that is almost always present is not worth it.
function ampCore.EnsureDirectory(relativeDirectory)
    local path = ampCore.ResolveDataPath(relativeDirectory)
    local command
    if tes3mp.GetOperatingSystemType() == "Windows" then
        command = 'mkdir "' .. path:gsub("/", "\\") .. '" >nul 2>nul'
    else
        command = 'mkdir -p "' .. path .. '" >/dev/null 2>&1'
    end
    pcall(os.execute, command)
end

local function directoryOf(relativePath)
    return tostring(relativePath or ""):match("^(.*)/[^/]*$")
end

--- Load a JSON file, creating it from defaultValue when it is not there yet.
--
-- Returns loadedTable, wasCreated. The returned table is always safe to index:
-- if both the load and the bootstrap write fail, a deep copy of defaultValue
-- comes back so the caller keeps running in memory instead of erroring out.
function ampCore.EnsureJson(relativePath, defaultValue, description)
    defaultValue = defaultValue or {}
    description = description or relativePath

    local definitelyPresent = ampCore.FileDefinitelyExists(relativePath)

    local ok, loaded = pcall(jsonInterface.load, relativePath)
    if ok and type(loaded) == "table" then
        return loaded, false
    end

    if definitelyPresent then
        -- The file is on disk but did not parse. Refusing to overwrite is the
        -- only safe answer here; a corrupt group registry is recoverable by
        -- hand, a silently truncated one is not.
        ampCore.LogError("Could not parse " .. description ..
            "; the existing file was left untouched and defaults are being used for this session")
        return tableHelper.deepCopy(defaultValue), false
    end

    local function tryWrite()
        local writeOk, writeResult = pcall(jsonInterface.save, relativePath, defaultValue)
        -- jsonInterface.save returns the boolean from writeToFile, so a raised
        -- error and a returned false both have to be treated as failure.
        return writeOk and writeResult ~= false
    end

    local written = tryWrite()
    if not written then
        -- The most common cause of a failed write is a data subdirectory that
        -- was never created, so make one attempt at that before giving up.
        local directory = directoryOf(relativePath)
        if directory ~= nil then
            ampCore.EnsureDirectory(directory)
            written = tryWrite()
        end
    end

    if not written then
        ampCore.LogError("Could not create " .. description ..
            "; continuing with in-memory defaults, changes will not persist")
        return tableHelper.deepCopy(defaultValue), false
    end

    ampCore.LogInfo("Created missing data file " .. description)
    return tableHelper.deepCopy(defaultValue), true
end

--- Save a table, reporting failure under the shared log identity.
function ampCore.SaveJson(relativePath, data, description)
    local ok, result = pcall(jsonInterface.save, relativePath, data)
    if not ok then
        ampCore.LogError("Could not save " .. tostring(description or relativePath) .. ": " .. tostring(result))
        return false
    end
    if result == false then
        ampCore.LogError("Could not save " .. tostring(description or relativePath) ..
            "; the data path may be read-only or missing")
        return false
    end
    return true
end

--- Register a data file to be created, if missing, at OnServerPostInit.
-- Helpers that read their own file at load time should call EnsureJson
-- directly instead; this list is for files that are only touched later and
-- would otherwise produce their first "cannot open" line mid-session.
function ampCore.RegisterDataFile(relativePath, defaultValue, description)
    table.insert(registeredFiles, {
        path = relativePath,
        default = defaultValue or {},
        description = description or relativePath
    })
end

function ampCore.EnsureRegisteredFiles()
    for _, entry in ipairs(registeredFiles) do
        ampCore.EnsureJson(entry.path, entry.default, entry.description)
    end
end

customEventHooks.registerHandler("OnServerPostInit", function()
    ampCore.EnsureRegisteredFiles()
end)

return ampCore

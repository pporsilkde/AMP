-- ArenaMP C19 compatibility shim.
-- The reset implementation was moved to server/scripts/resetHelrer.lua.
-- Keep this forwarder so existing customScripts.lua entries such as
-- require("custom.periodicCellResets") continue to work safely.
return require("resetHelrer")

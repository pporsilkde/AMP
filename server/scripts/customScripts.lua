-- Load up your custom scripts here! Ideally, your custom scripts will be placed in the scripts/custom folder and then get loaded like this:
--
-- require("custom/yourScript")
--
-- Refer to the Tutorial.md file for information on how to use various event and command hooks in your scripts.

-- ArenaMP X048: first registered account becomes the server owner (staffRank 3,
-- console enabled); every other account gets the console switched off.
require("custom/arenampOwnerConsole")

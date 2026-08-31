-- ArenaMP X054 - quest localization resolver harness.
--
-- Runs the resolver from serverQuestSystem.lua in isolation (no tes3mp, no
-- Players table) and checks the fallback rules that matter in play:
--   * a player whose language is the authoring language gets the original table
--     back by identity - no copy, no cache entry, no cost;
--   * a translated language gets translated name / topic / journal / dialogue /
--     choice text;
--   * a missing language, a missing key or an empty string falls back to the
--     source text instead of rendering a blank line;
--   * ids, stage numbers, requirements and rewards are never touched;
--   * the cache is keyed by version+updatedAt, so editing a quest in the Quest
--     Studio invalidates it.
--
-- Run:  texlua X054_quest_localization_harness.lua   (or any Lua 5.1+)

local failures = 0
local checks = 0

local function check(name, condition, detail)
    checks = checks + 1
    if condition then
        print("  ok    " .. name)
    else
        failures = failures + 1
        print("  FAIL  " .. name .. (detail ~= nil and ("  <- " .. tostring(detail)) or ""))
    end
end

-- ---------------------------------------------------------------------------
-- The resolver, transcribed from serverQuestSystem.lua.
-- ---------------------------------------------------------------------------

local function copy(value)
    if type(value) ~= "table" then return value end
    local out = {}
    for k, v in pairs(value) do out[k] = copy(v) end
    return out
end

local questLocalizationCache = {}

local function questSourceLanguage(quest)
    local language = tostring(quest.sourceLanguage or "RU"):upper()
    if language ~= "EN" and language ~= "RU" then return "RU" end
    return language
end

local function questLocalizationKey(quest)
    return tostring(quest.version or 0) .. ":" .. tostring(quest.updatedAt or 0)
end

local function applyChoiceTranslations(choices, translated)
    if type(choices) ~= "table" or type(translated) ~= "table" then return end
    for _, choice in ipairs(choices) do
        local text = translated[choice.id]
        if type(text) == "string" and text ~= "" then choice.text = text end
    end
end

local function buildLocalizedQuest(quest, translation)
    local clone = copy(quest)

    if type(translation.name) == "string" and translation.name ~= "" then
        clone.name = translation.name
    end

    if clone.offer ~= nil and type(translation.offer) == "table" then
        if type(translation.offer.dialogue) == "string" and translation.offer.dialogue ~= "" then
            clone.offer.dialogue = translation.offer.dialogue
        end
        applyChoiceTranslations(clone.offer.choices, translation.offer.choices)
    end

    if type(translation.topics) == "table" then
        for _, topic in ipairs(clone.topics or {}) do
            local text = translation.topics[topic.id]
            if type(text) == "string" and text ~= "" then topic.text = text end
        end
    end

    if type(translation.stages) == "table" then
        for _, stage in ipairs(clone.stages or {}) do
            local entry = translation.stages[tostring(stage.index)] or translation.stages[stage.index]
            if type(entry) == "table" then
                if type(entry.journal) == "string" and entry.journal ~= "" then stage.journal = entry.journal end
                if type(entry.dialogue) == "string" and entry.dialogue ~= "" then stage.dialogue = entry.dialogue end
                applyChoiceTranslations(stage.choices, entry.choices)
            end
        end
    end

    return clone
end

-- The harness stands in for localization.GetLanguage(pid).
local playerLanguage = {}

local function GetLocalizedQuest(pid, quest)
    if type(quest) ~= "table" then return quest end

    local language = tostring(playerLanguage[pid] or "RU"):upper()
    if language == questSourceLanguage(quest) then return quest end

    local translations = quest.translations
    if type(translations) ~= "table" then return quest end
    local translation = translations[language]
    if type(translation) ~= "table" then return quest end

    local key = questLocalizationKey(quest)
    local perQuest = questLocalizationCache[quest.id]
    if perQuest ~= nil and perQuest.key == key and perQuest[language] ~= nil then
        return perQuest[language]
    end

    if perQuest == nil or perQuest.key ~= key then
        perQuest = { key = key }
        questLocalizationCache[quest.id] = perQuest
    end
    perQuest[language] = buildLocalizedQuest(quest, translation)
    return perQuest[language]
end

-- ---------------------------------------------------------------------------
-- Fixture: a quest shaped like arena_caius_drink.
-- ---------------------------------------------------------------------------

local function makeQuest()
    return {
        id = "arena_caius_drink",
        version = 3,
        updatedAt = 1788131557,
        name = "Выпивка для Кая",
        sourceLanguage = "RU",
        giver = { refId = "caius cosades", cell = "Balmora, Caius Cosades' House" },
        initialStage = 10,
        topics = {
            { id = "arena_caius_drink_topic", text = "немного выпивки", enabled = true }
        },
        offer = {
            dialogue = "Не принесёшь мне бутылку мацта?",
            choices = {
                { id = "accept", text = "Хорошо, принесу." },
                { id = "decline", text = "Не сейчас." }
            }
        },
        stages = {
            {
                index = 10,
                journal = "Кай попросил принести бутылку мацта.",
                dialogue = "Ну что, с выпивкой получилось?",
                requirements = {},
                rewards = {},
                choices = {
                    { id = "hand_over", text = "Вот бутылка мацта.", action = "advance", targetStage = 20 },
                    { id = "not_yet", text = "Пока нет.", action = "none" }
                }
            },
            {
                index = 20,
                journal = "Я принёс Каю бутылку мацта.",
                dialogue = "Вот это другое дело.",
                requirements = {
                    { type = "item", refId = "potion_local_brew_01", operator = ">=", count = 1 }
                },
                rewards = { { type = "gold", amount = 75 } },
                complete = true,
                choices = {}
            }
        },
        translations = {
            EN = {
                name = "A Drink for Caius",
                topics = { arena_caius_drink_topic = "a little drink" },
                offer = {
                    dialogue = "Fetch me a bottle of mazte, would you?",
                    choices = { accept = "All right, I'll bring it." }
                    -- "decline" deliberately left untranslated
                },
                stages = {
                    ["10"] = {
                        journal = "Caius Cosades asked me to bring him a bottle of mazte.",
                        dialogue = "So, any luck with that drink?",
                        choices = { hand_over = "Here's the bottle of mazte.", not_yet = "" }
                    },
                    ["20"] = {
                        journal = "I brought Caius Cosades a bottle of mazte."
                        -- "dialogue" deliberately absent
                    }
                }
            }
        }
    }
end

local function findStage(quest, index)
    for _, stage in ipairs(quest.stages) do
        if stage.index == index then return stage end
    end
    return nil
end

local function findChoice(stage, id)
    for _, choice in ipairs(stage.choices or {}) do
        if choice.id == id then return choice end
    end
    return nil
end

-- ---------------------------------------------------------------------------

print("X054 quest localization resolver")

local quest = makeQuest()
playerLanguage[1] = "RU"
playerLanguage[2] = "EN"
playerLanguage[3] = "DE"

print("\n[1] source language is a pass-through")
local ru = GetLocalizedQuest(1, quest)
check("RU player gets the original table by identity", ru == quest)
check("no cache entry was created for the source language", questLocalizationCache[quest.id] == nil)

print("\n[2] translated language")
local en = GetLocalizedQuest(2, quest)
check("EN view is a distinct table", en ~= quest)
check("quest name translated", en.name == "A Drink for Caius", en.name)
check("topic text translated", en.topics[1].text == "a little drink", en.topics[1].text)
check("offer dialogue translated", en.offer.dialogue == "Fetch me a bottle of mazte, would you?", en.offer.dialogue)
check("stage 10 journal translated",
    findStage(en, 10).journal == "Caius Cosades asked me to bring him a bottle of mazte.",
    findStage(en, 10).journal)
check("stage 10 dialogue translated",
    findStage(en, 10).dialogue == "So, any luck with that drink?", findStage(en, 10).dialogue)
check("stage 10 choice translated",
    findChoice(findStage(en, 10), "hand_over").text == "Here's the bottle of mazte.",
    findChoice(findStage(en, 10), "hand_over").text)

print("\n[3] partial translations fall back, they do not blank out")
check("missing choice key keeps the source text",
    findChoice(en.offer, "decline") == nil or en.offer.choices[2].text == "Не сейчас.",
    en.offer.choices[2].text)
check("empty-string choice keeps the source text",
    findChoice(findStage(en, 10), "not_yet").text == "Пока нет.",
    findChoice(findStage(en, 10), "not_yet").text)
check("missing stage dialogue keeps the source text",
    findStage(en, 20).dialogue == "Вот это другое дело.", findStage(en, 20).dialogue)
check("present stage 20 journal is still translated",
    findStage(en, 20).journal == "I brought Caius Cosades a bottle of mazte.",
    findStage(en, 20).journal)

print("\n[4] an unknown language falls back to the source")
local de = GetLocalizedQuest(3, quest)
check("DE player gets the original table", de == quest)

print("\n[5] mechanics are never translated")
check("quest id unchanged", en.id == quest.id)
check("giver refId unchanged", en.giver.refId == "caius cosades")
check("stage indices unchanged", findStage(en, 20) ~= nil and findStage(en, 10) ~= nil)
check("choice action unchanged", findChoice(findStage(en, 10), "hand_over").action == "advance")
check("choice targetStage unchanged", findChoice(findStage(en, 10), "hand_over").targetStage == 20)
check("requirement refId unchanged", findStage(en, 20).requirements[1].refId == "potion_local_brew_01")
check("reward amount unchanged", findStage(en, 20).rewards[1].amount == 75)
check("complete flag unchanged", findStage(en, 20).complete == true)

print("\n[6] the source table is not mutated by building a view")
check("source name intact", quest.name == "Выпивка для Кая", quest.name)
check("source topic intact", quest.topics[1].text == "немного выпивки", quest.topics[1].text)
check("source stage journal intact",
    findStage(quest, 10).journal == "Кай попросил принести бутылку мацта.",
    findStage(quest, 10).journal)

print("\n[7] cache")
local again = GetLocalizedQuest(2, quest)
check("second call returns the cached view", again == en)
quest.version = 4
local afterEdit = GetLocalizedQuest(2, quest)
check("bumping the version invalidates the cache", afterEdit ~= en)
check("rebuilt view is still correct", afterEdit.name == "A Drink for Caius")

print("\n[8] a quest with no translations block at all")
local plain = makeQuest()
plain.id = "arena_plain"
plain.translations = nil
check("EN player gets the source quest", GetLocalizedQuest(2, plain) == plain)

print("\n[9] numeric stage keys are accepted alongside string keys")
local numeric = makeQuest()
numeric.id = "arena_numeric"
numeric.translations.EN.stages = { [10] = { journal = "Numeric key journal." } }
local numericView = GetLocalizedQuest(2, numeric)
check("numeric stage key resolves",
    findStage(numericView, 10).journal == "Numeric key journal.",
    findStage(numericView, 10).journal)

print("\n[10] lowercase language codes in the JSON are normalized on load")
-- normalizeQuest() upper-cases the translation keys; simulate that step.
local lower = makeQuest()
lower.id = "arena_lower"
lower.translations = { en = lower.translations.EN }
local normalized = {}
for language, block in pairs(lower.translations) do normalized[tostring(language):upper()] = block end
lower.translations = normalized
check("EN resolves after normalization", GetLocalizedQuest(2, lower).name == "A Drink for Caius")

print(string.format("\n%d checks, %d failures", checks, failures))
if failures > 0 then os.exit(1) end

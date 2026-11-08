function onSay(player, words, param, speechType)
    if speechType ~= TALKTYPE_SAY or param ~= "" then
        player:sendCancelMessage("Use this command without parameters in local chat.")
        return false
    end
    player:useEasyModeItem(words:sub(2):lower())
    return false
end

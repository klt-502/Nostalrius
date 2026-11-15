function onSay(player, words, param, speechType)
    if speechType ~= TALKTYPE_SAY or param ~= "" then
        player:sendCancelMessage("Use !light without parameters in local chat.")
        return false
    end
    player:giveEasyLightWand()
    return false
end

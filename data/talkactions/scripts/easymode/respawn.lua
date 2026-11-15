function onSay(player, words, param, speechType)
    if speechType == TALKTYPE_SAY then player:respawnCommand(param:lower():trim()) end
    return false
end

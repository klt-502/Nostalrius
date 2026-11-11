function onSay(player, words, param, speechType)
    if speechType ~= TALKTYPE_SAY then return false end
    player:brokerCommand(param:lower():trim())
    return false
end

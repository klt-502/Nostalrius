function onSay(player, words, param, speechType)
    if speechType ~= TALKTYPE_SAY then return false end
    param = param:lower():trim()
    if param == "on" or param == "off" then
        if not player:setEasyModeAutoTarget(param == "on") then return false end
    elseif param ~= "" and param ~= "status" then
        player:sendCancelMessage("Use !autotarget on, off, or status.")
        return false
    end
    player:sendTextMessage(MESSAGE_STATUS_SMALL,
        "Auto-target: " .. (player:isEasyModeAutoTargetActive() and "on" or "off") .. ".")
    return false
end

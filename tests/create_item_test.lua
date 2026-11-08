ACCOUNT_TYPE_GOD = 6
ITEM_ATTRIBUTE_KEYNUMBER = 1
CONST_ME_MAGIC_GREEN = 1

function string:split(separator)
    local parts, start = {}, 1
    while true do
        local index = self:find(separator, start, true)
        if not index then parts[#parts + 1] = self:sub(start); return parts end
        parts[#parts + 1] = self:sub(start, index - 1)
        start = index + #separator
    end
end

local key = false
function ItemType()
    return {
        getId = function() return 3160 end,
        isStackable = function() return false end,
        isKey = function() return key end,
        hasSubType = function() return true end
    }
end

local created, assigned, keyNumber = 0
local itemMethods = {
    setActionId = function(_, value) assigned = value end,
    setAttribute = function(_, _, value) keyNumber = value end,
    decay = function() end
}
local item = newproxy(true)
getmetatable(item).__index = itemMethods
local player = {
    getGroup = function() return {getAccess = function() return true end} end,
    getAccountType = function() return ACCOUNT_TYPE_GOD end,
    sendCancelMessage = function() end,
    addItem = function() created = created + 1; return item end,
    getPosition = function() return {sendMagicEffect = function() end} end
}

assert(loadfile(arg[1]))()
for _, invalid in ipairs({"0", "-1", "65536", "65001.5", "abc", "", "nan", "1e309"}) do
    created, assigned = 0, nil
    onSay(player, "/i", "3160,1," .. invalid)
    assert(created == 0 and assigned == nil, "Created item for invalid ID: " .. invalid)
end
for _, valid in ipairs({1, 65001, 65535}) do
    created, assigned = 0, nil
    onSay(player, "/i", "3160,1," .. valid)
    assert(created == 1 and assigned == valid)
end
created, assigned = 0, nil
onSay(player, "/i", "3160,,65001.5")
assert(created == 0 and assigned == nil)
onSay(player, "/i", "3160,1,65001,extra")
assert(created == 0 and assigned == nil)
onSay(player, "/i", "3160,,65001")
assert(created == 1 and assigned == 65001)
created, assigned = 0, nil
onSay(player, "/i", "3160,1")
assert(created == 1 and assigned == nil)
key = true
onSay(player, "/i", "3160,1234,65001")
assert(keyNumber == 1234 and assigned == 65001)
player.addItem = function() created = created + 1; return {item, item} end
onSay(player, "/i", "3160,2,65003")
assert(assigned == 65003)
print("GM Action-ID validation tests passed")

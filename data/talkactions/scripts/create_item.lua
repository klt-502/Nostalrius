function onSay(player, words, param)
	if not player:getGroup():getAccess() then
		return true
	end

	if player:getAccountType() < ACCOUNT_TYPE_GOD then
		return false
	end

	-- The shared string.split drops empty fields, which would shift arguments.
	local split = {}
	for field in (param .. ","):gmatch("(.-),") do
		split[#split + 1] = field
	end
	if #split > 3 then
		player:sendCancelMessage("Use /i item,count,actionId.")
		return false
	end
	-- Validate before addItem: malformed/fractional IDs must never create anything.
	local actionId
	if split[3] ~= nil then
		actionId = tonumber(split[3])
		if not actionId or actionId < 1 or actionId > 65535 or actionId ~= math.floor(actionId) then
			player:sendCancelMessage("Action ID must be an integer between 1 and 65535.")
			return false
		end
	end

	local itemType = ItemType(split[1])
	if itemType:getId() == 0 then
		itemType = ItemType(tonumber(split[1]))
		if itemType:getId() == 0 then
			player:sendCancelMessage("There is no item with that id or name.")
			return false
		end
	end

	local keynumber = 0
	local count = tonumber(split[2])
	if count ~= nil then
		if itemType:isStackable() then
			count = math.min(10000, math.max(1, count))
		elseif itemType:isKey() then
			keynumber = count
			count = 1
		elseif not itemType:hasSubType() then
			count = math.min(100, math.max(1, count))
		else
			count = math.max(1, count)
		end
	else
		count = 1
	end

	local result = player:addItem(itemType:getId(), count)
	if result ~= nil then
		local function configureItem(item)
			if itemType:isKey() then
				item:setAttribute(ITEM_ATTRIBUTE_KEYNUMBER, keynumber)
			end

			if actionId ~= nil then
				item:setActionId(actionId)
			end

			item:decay()
			return true
		end

		if type(result) == "table" then
			for _, item in ipairs(result) do
				configureItem(item)
			end
		else
			configureItem(result)
		end

		player:getPosition():sendMagicEffect(CONST_ME_MAGIC_GREEN)
	end
	return false
end

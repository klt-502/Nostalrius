#include "otpch.h"

#include "easymode.h"
#include "configmanager.h"
#include "item.h"

extern ConfigManager g_config;

bool EasyMode::isEndlessRune(const Item& item)
{
	if (!g_config.getBoolean(ConfigManager::EASY_MODE_ENABLED) ||
	    !g_config.getBoolean(ConfigManager::EASY_MODE_ENDLESS_RUNES)) {
		return false;
	}
	const Consumable kind = recognizeConsumable(item.getID(), item.getActionId(), item.getSubType());
	return kind == Consumable::UltimateHealing || kind == Consumable::GreatFireball || kind == Consumable::Explosion;
}

bool EasyMode::isEndlessManaFluid(const Item& item)
{
	return g_config.getBoolean(ConfigManager::EASY_MODE_ENABLED) &&
	       g_config.getBoolean(ConfigManager::EASY_MODE_ENDLESS_MANA_FLUID) &&
	       recognizeConsumable(item.getID(), item.getActionId(), item.getSubType()) == Consumable::ManaFluid;
}

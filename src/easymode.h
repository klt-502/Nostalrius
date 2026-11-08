#ifndef ZEDAS_EASYMODE_H
#define ZEDAS_EASYMODE_H

#include <cstdint>

class Item;

namespace EasyMode {
enum class Consumable { None, UltimateHealing, GreatFireball, Explosion, ManaFluid };

struct ConsumableDefinition {
	Consumable kind;
	const char* command;
	uint16_t itemId;
	uint16_t actionId;
	uint16_t subtype; // Zero means subtype is not part of rune recognition.
};

static constexpr ConsumableDefinition CONSUMABLES[] = {
	{Consumable::UltimateHealing, "uh", 3160, 65001, 0},
	{Consumable::GreatFireball, "gfb", 3191, 65002, 0},
	{Consumable::Explosion, "explo", 3200, 65003, 0},
	{Consumable::ManaFluid, "mana", 2874, 65004, 10}
};

inline Consumable recognizeConsumable(uint16_t itemId, uint16_t actionId, uint16_t subtype)
{
	for (const auto& definition : CONSUMABLES) {
		if (itemId == definition.itemId && actionId == definition.actionId &&
		    (definition.kind != Consumable::ManaFluid || subtype == definition.subtype)) {
			return definition.kind;
		}
	}
	return Consumable::None;
}

bool isEndlessRune(const Item& item);
bool isEndlessManaFluid(const Item& item);
}

#endif

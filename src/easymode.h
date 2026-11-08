#ifndef ZEDAS_EASYMODE_H
#define ZEDAS_EASYMODE_H

#include <cstdint>
#include <string>

class Item;
class Player;
class Creature;

namespace EasyMode {
static constexpr int64_t AUTO_TARGET_UNREACHABLE_MS = 3000;

struct AutoTargetState {
	bool active = false;
	uint32_t automaticTargetId = 0;
	int64_t unreachableSince = 0;
	void manualInput(bool suspend) {
		automaticTargetId = 0;
		unreachableSince = 0;
		if (suspend) active = false;
	}
};

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
bool useConsumable(Player& player, const std::string& command);
bool isPlayerControlled(const Creature& creature);
bool setAutoTarget(Player& player, bool active);
void updateAutoTarget(Player& player, int64_t now);
}

#endif

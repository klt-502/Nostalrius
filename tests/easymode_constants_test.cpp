#include "easymode.h"
#include <cstdlib>
#include <iostream>

static void require(bool condition)
{
	if (!condition) {
		std::cerr << "Consumable recognition test failed\n";
		std::exit(1);
	}
}

int main()
{
	using namespace EasyMode;
	require(recognizeGoldBag(2856, 65005));
	require(!recognizeGoldBag(2853, 65005));
	require(!recognizeGoldBag(2856, 65004));
	require(!recognizeGoldBag(2856, 0));
	for (const auto& expected : CONSUMABLES) {
		require(recognizeConsumable(expected.itemId, expected.actionId, expected.subtype) == expected.kind);
		require(recognizeConsumable(expected.itemId, 0, expected.subtype) == Consumable::None);
		require(recognizeConsumable(3031, expected.actionId, expected.subtype) == Consumable::None);
		for (const auto& other : CONSUMABLES) {
			if (other.kind != expected.kind) {
				require(recognizeConsumable(expected.itemId, other.actionId, expected.subtype) == Consumable::None);
			}
		}
	}
	require(recognizeConsumable(2874, 65004, 0) == Consumable::None);
	require(recognizeConsumable(2874, 65004, 11) == Consumable::None);
	require(recognizeConsumable(2875, 65004, 10) == Consumable::None);
	return 0;
}

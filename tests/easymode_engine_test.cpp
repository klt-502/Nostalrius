#include "otpch.h"
#include "easymode.h"
#include "actions.h"
#include "chat.h"
#include "configmanager.h"
#include "container.h"
#include "creatureevent.h"
#include "databasetasks.h"
#include "game.h"
#include "monster.h"
#include "movement.h"
#include "player.h"
#include "rsa.h"
#include "scheduler.h"
#include "spells.h"
#include "talkaction.h"
#include "vocation.h"
#include <cstdlib>

DatabaseTasks g_databaseTasks;
Dispatcher g_dispatcher;
Scheduler g_scheduler;
Game g_game;
ConfigManager g_config;
Monsters g_monsters;
Vocations g_vocations;
RSA g_RSA;
extern LuaEnvironment g_luaEnvironment;
extern Actions* g_actions;
extern Spells* g_spells;
extern MoveEvents* g_moveEvents;
extern CreatureEvents* g_creatureEvents;
extern Chat* g_chat;
extern TalkActions* g_talkActions;

static void require(bool condition, const char* message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(1);
	}
}

static Group ordinaryGroup = {"Test", 0, 0, 0, 1, false};

static Player* makePlayer(int x = 100, int y = 100)
{
	Player* player = new Player(nullptr);
	player->setName("Test" + std::to_string(x) + "x" + std::to_string(y));
	player->setGroup(&ordinaryGroup);
	require(player->setVocation(0), "set test vocation");
	require(g_game.internalPlaceCreature(player, Position(x, y, 7)), "place test player");
	return player;
}

static Item* carry(Player& player, uint16_t id, uint16_t aid = 0, uint16_t subtype = 1)
{
	Container* bag = Item::CreateItem(2854)->getContainer();
	Container* nested = Item::CreateItem(2853)->getContainer();
	require(g_game.internalAddItem(&player, bag, CONST_SLOT_BACKPACK) == RETURNVALUE_NOERROR, "equip carried bag");
	bag->internalAddThing(nested);
	Item* item = Item::CreateItem(id, subtype);
	if (aid) item->setActionId(aid);
	nested->internalAddThing(item);
	return item;
}

static void setup()
{
	g_actions = new Actions();
	g_spells = new Spells();
	g_moveEvents = new MoveEvents();
	g_creatureEvents = new CreatureEvents();
	g_chat = new Chat();
	g_talkActions = new TalkActions();
	require(Item::items.loadItems(), "load real item definitions");
	require(g_vocations.loadFromXml(), "load real vocations");
	require(g_luaEnvironment.loadFile("data/global.lua") == 0, "load real Lua helpers");
	require(g_spells->loadFromXml(), "load real rune definitions and scripts");
	require(g_actions->loadFromXml(), "load real fluid action");
	require(g_talkActions->loadFromXml(), "load real Easy Mode talkactions");
	uint16_t ground = 0;
	for (size_t id = 100; id < Item::items.size(); ++id) {
		const ItemType& type = Item::items[id];
		if (type.isGroundTile() && !type.blockSolid && !type.blockPathFind) {
			ground = id;
			break;
		}
	}
	require(ground != 0, "find traversable ground");
	for (int x = 88; x <= 112; ++x) {
		for (int y = 92; y <= 108; ++y) {
			StaticTile* tile = new StaticTile(x, y, 7);
			tile->internalAddThing(Item::CreateItem(ground));
			g_game.map.setTile(Position(x, y, 7), tile);
		}
	}
}

int main(int argc, char** argv)
{
	require(argc == 2, "scenario argument");
	const std::string scenario = argv[1];
	const bool loaded = g_config.load();
	if (scenario == "invalid_config") {
		require(!loaded, "reject nonboolean Easy settings");
		return 0;
	}
	require(loaded, "load test configuration");
	setup();
	const bool enabled = scenario != "disabled" && scenario != "feature_disabled";
	for (const auto& definition : EasyMode::CONSUMABLES) {
		Item* item = Item::CreateItem(definition.itemId, definition.subtype ? definition.subtype : 2);
		item->setActionId(definition.actionId);
		const bool result = definition.kind == EasyMode::Consumable::ManaFluid ?
			EasyMode::isEndlessManaFluid(*item) : EasyMode::isEndlessRune(*item);
		require(result == enabled, "master/feature gates");
		PropWriteStream stream;
		item->serializeAttr(stream);
		size_t length;
		const char* data = stream.getStream(length);
		PropStream input;
		input.init(data, length);
		Item* restored = Item::CreateItem(definition.itemId, 1);
		require(restored->unserializeAttr(input), "deserialize item attributes");
		require(restored->getActionId() == definition.actionId && restored->getSubType() == item->getSubType(), "persist marking/subtype");
		item->decrementReferenceCounter();
		restored->decrementReferenceCounter();
	}
	Player* player = makePlayer();
	if (!enabled) {
		require(!EasyMode::useConsumable(*player, "uh"), "disabled command fails");
		player->setVarStats(STAT_MAGICPOINTS, 10);
		player->changeHealth(-500);
		Item* marked = carry(*player, 3160, 65001, 2);
		require(g_actions->useItemEx(player, Position(0xFFFF, 0, 0), player->getPosition(), 0, marked, player), "ordinary marked-rune use remains available");
		require(marked->getCharges() == 1, "disabled marked rune consumes a charge");
		Player* drinker = makePlayer(101, 100);
		drinker->setVarStats(STAT_MAXMANAPOINTS, 500);
		Item* fluid = carry(*drinker, 2874, 65004, 10);
		require(g_actions->useItemEx(drinker, Position(0xFFFF, 0, 0), drinker->getPosition(), 0, fluid, drinker), "ordinary marked-fluid use remains available");
		require(fluid->getFluidType() == FLUID_NONE, "disabled marked fluid empties");
		return 0;
	}
	require(!EasyMode::useConsumable(*player, "unknown"), "unknown command fails");
	require(!EasyMode::useConsumable(*player, "uh"), "missing item fails");
	Item* ordinary = carry(*player, 3160);
	require(!EasyMode::useConsumable(*player, "uh"), "ordinary rune never substituted");
	ordinary->setActionId(65002);
	require(!EasyMode::useConsumable(*player, "uh"), "wrong marking never substituted");
	ordinary->setActionId(65001);
	require(!EasyMode::useConsumable(*player, "uh"), "normal magic-level rule retained");
	Player* healer = makePlayer(101, 100);
	healer->setVarStats(STAT_MAGICPOINTS, 10);
	healer->changeHealth(-500);
	Item* uh = carry(*healer, 3160, 65001);
	g_game.playerSay(healer->getID(), 0, TALKTYPE_CHANNEL_Y, "", "!uh");
	require(healer->getHealth() == 500, "nonlocal command rejected");
	g_game.playerSay(healer->getID(), 0, TALKTYPE_SAY, "", "!uh unexpected");
	require(healer->getHealth() == 500, "command parameters rejected");
	g_game.playerSay(healer->getID(), 0, TALKTYPE_SAY, "", "!uh");
	require(healer->getHealth() > 500 && uh->getCharges() == 1, "real healing and endless charge");
	const int health = healer->getHealth();
	require(healer->hasCondition(CONDITION_EXHAUST), "real rune exhaustion applied");
	require(!EasyMode::useConsumable(*healer, "uh") && healer->getHealth() == health, "action clock blocks repetition");
	require(!g_spells->getRuneSpell(3160)->executeUse(healer, uh, healer->getPosition(), healer, healer->getPosition()), "ordinary route shares exhaustion");
	std::cout << "Native engine scenario passed: " << scenario << std::endl;
	return 0;
}

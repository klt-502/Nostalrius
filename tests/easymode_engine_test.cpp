#include "otpch.h"
#include "easymode.h"
#include "easyspill.h"
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
#include "protocolgame.h"
#include "rsa.h"
#include "scheduler.h"
#include "spells.h"
#include "talkaction.h"
#include "tools.h"
#include "vocation.h"
#include <cstdlib>
#include <fstream>

LuaEnvironment g_luaEnvironment;
DatabaseTasks g_databaseTasks;
Dispatcher g_dispatcher;
Scheduler g_scheduler;
Game g_game;
ConfigManager g_config;
Monsters g_monsters;
Vocations g_vocations;
RSA g_RSA;
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
void testRespawnEngine(Player& observer);

struct ProtocolGameTestAccess {
	static void bind(ProtocolGame& protocol, Player& player) { protocol.player = &player; }
};

static Player* makePlayer(int x = 100, int y = 100)
{
	ProtocolGame_ptr protocol = std::make_shared<ProtocolGame>(nullptr);
	Player* player = new Player(protocol);
	ProtocolGameTestAccess::bind(*protocol, *player);
	player->setName("Test" + std::to_string(x) + "x" + std::to_string(y));
	player->setGroup(&ordinaryGroup);
	require(player->setVocation(0), "set test vocation");
	require(g_game.internalPlaceCreature(player, Position(x, y, 7)), "place test player");
	g_game.map.clearSpectatorCache();
	return player;
}

static Monster* makeMonster(int health, int x, int y, bool hostile = true, bool attackable = true, MonsterType** createdType = nullptr)
{
	MonsterType* type = new MonsterType();
	type->name = "test monster";
	type->nameDescription = "a test monster";
	type->info.health = health;
	type->info.healthMax = health;
	type->info.isHostile = hostile;
	type->info.isAttackable = attackable;
	if (createdType) *createdType = type;
	Monster* monster = new Monster(type);
	require(g_game.internalPlaceCreature(monster, Position(x, y, 7)), "place test monster");
	g_game.map.clearSpectatorCache();
	return monster;
}

static Item* carry(Player& player, uint16_t id, uint16_t aid = 0, uint16_t subtype = 1)
{
	Container* bag = Item::CreateItem(2854)->getContainer();
	Container* nested = Item::CreateItem(2853)->getContainer();
	require(g_game.internalAddItem(&player, bag, CONST_SLOT_BACKPACK) == RETURNVALUE_NOERROR, "equip carried bag");
	require(g_game.internalAddItem(bag, nested) == RETURNVALUE_NOERROR, "add nested carried bag");
	Item* item = Item::CreateItem(id, subtype);
	if (aid) item->setActionId(aid);
	require(g_game.internalAddItem(nested, item) == RETURNVALUE_NOERROR, "add carried test item");
	return item;
}

static Container* corpseAt(int x, int y)
{
	Container* corpse = Item::CreateItem(3994)->getContainer();
	require(corpse != nullptr, "create real corpse container");
	corpse->setCorpseOwner(0x12345678);
	require(g_game.internalAddItem(g_game.map.getTile(x, y, 7), corpse, INDEX_WHEREEVER, FLAG_NOLIMIT) == RETURNVALUE_NOERROR, "place corpse");
	return corpse;
}

static Item* addCoin(Container& container, uint16_t id, uint16_t count, uint16_t aid = 0)
{
	Item* item = Item::CreateItem(id, count);
	if (aid) item->setActionId(aid);
	require(g_game.internalAddItem(&container, item) == RETURNVALUE_NOERROR, "add test currency");
	return item;
}

static uint64_t contentsValue(Container& container)
{
	uint64_t value = 0;
	for (Item* item : container.getItemList()) {
		value += item->getWorth();
		if (Container* child = item->getContainer()) value += contentsValue(*child);
	}
	return value;
}

#include "gold_bag_cases.h"
#include "loot_spill_cases.h"
#include "hunting_spill_cases.h"
#include "broker_cases.h"
#include "broker_feedback_cases.h"
#include "loot_class_cases.h"

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
	if (scenario == "defaults") {
		require(!Broker::enabled() && !g_config.getBoolean(ConfigManager::EASY_MODE_LOOT_BROKER),
		        "broker defaults disabled");
		require(g_config.getNumber(ConfigManager::EASY_MODE_BROKER_GRACE_SECONDS) == 45 &&
		            g_config.getNumber(ConfigManager::EASY_MODE_BROKER_COMMISSION_BPS) == 2500 &&
		            g_config.getNumber(ConfigManager::EASY_MODE_BROKER_LEASH_TILES) == 7,
		        "broker validated defaults");
		Broker::initialize(); // Disabled mode must not need a schema/DB/catalog.
		const ConfigManager::boolean_config_t wave2[] = {ConfigManager::EASY_MODE_LOOT_SPILL,
			ConfigManager::EASY_MODE_GOLD_BAG, ConfigManager::EASY_MODE_RESPAWN};
		for (auto key : wave2) require(!g_config.getBoolean(key), "Wave 2 flags default disabled");
		require(g_config.getString(ConfigManager::EASY_MODE_RESPAWN_POLICY_FILE) == "data/zedas/spawns.xml", "default manifest path");
		std::ofstream config("config.lua", std::ios::app);
		config << "\neasyModeLootSpill = true\neasyModeGoldBag = true\neasyModeRespawn = true\n"
		          "easyModeRespawnPolicyFile = 'changed.xml'\n";
		config.close();
		require(g_config.load(), "reload valid config without changing startup state");
		for (auto key : wave2) require(!g_config.getBoolean(key), "Wave 2 flags remain startup only");
		require(g_config.getString(ConfigManager::EASY_MODE_RESPAWN_POLICY_FILE) == "data/zedas/spawns.xml", "manifest path remains startup only");
	}
	setup();
    if (scenario=="broker_feedback" || scenario=="broker_feedback_disabled") {
        require(g_moveEvents->loadFromXml(),"feedback real movement handlers");
        testBrokerFeedback(scenario=="broker_feedback_disabled"); return 0;
    }
    if (scenario=="loot_classes") {
        require(g_moveEvents->loadFromXml(), "load class fixture movement handlers");
        testLootClasses(); return 0;
    }
	if (scenario == "broker_seed" || scenario == "broker_sale_restart" || scenario == "broker_payout_restart" ||
	    scenario == "broker_final_restart") {
		require(g_moveEvents->loadFromXml(), "restart real movement handlers");
		testBrokerRestart(scenario);
		return 0;
	}
	if (scenario == "broker" || scenario == "broker_catalog") {
		require(g_moveEvents->loadFromXml(), "load real movement handlers");
		if (scenario == "broker")
			testBroker();
		else
			testBrokerCatalog();
		return 0;
	}
	if (scenario == "spill_hunting") {
		require(g_moveEvents->loadFromXml(), "load actual native equipment handlers");
		testHuntingSpill();
		return 0;
	}
	if (scenario == "wave2_disabled")
		require(!Broker::enabled(), "master switch gates enabled broker flag");
	if (scenario == "spill_only" || scenario == "spill_invalid_policy" || scenario == "spill_missing_policy" || scenario == "wave2_disabled" || scenario == "bag_disabled") {
		Player* collector = makePlayer(100, 100);
		Container* markedBag = carry(*collector, 2856, 65005)->getContainer();
		Container* backpack = collector->getInventoryItem(CONST_SLOT_BACKPACK)->getContainer();
		Item* oldGold = addCoin(*backpack, 3031, 95);
		Container* corpse = corpseAt(100, 101);
		Item* armor = Item::CreateItem(3357);
		require(g_game.internalAddItem(corpse, armor) == RETURNVALUE_NOERROR, "add feature-gate armor fixture");
		addCoin(*corpse, 3031, 10);
		EasyMode::onLootGenerated(*corpse);
		EasyMode::spillLoot();
		const bool spilled = scenario == "spill_only";
		require(armor->getParent() == (spilled ? static_cast<Cylinder*>(corpse->getTile()) : static_cast<Cylinder*>(corpse)), "spill feature/master/policy gate");
		if (scenario == "wave2_disabled" || scenario == "spill_only") {
			require(EasyMode::collectGold(*collector) == 0 && contentsValue(*corpse) == 10, "disabled auto-gold/master retains actual currency");
		} else {
			require(EasyMode::collectGold(*collector) == 10 && oldGold->getItemCount() == 100 && contentsValue(*markedBag) == 0, "disabled Gold Bag and invalid spill policy retain Wave 1 currency fallback");
		}
		return 0;
	}
	if (scenario == "spill_enabled") {
		require(g_moveEvents->loadFromXml(), "load spill movement safety fixtures");
		testLootSpill();
		return 0;
	}
	if (scenario == "bag_enabled") {
		testGoldBag();
		return 0;
	}
	const bool enabled = scenario != "disabled" && scenario != "feature_disabled" && scenario != "defaults";
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
	if (scenario == "respawn_enabled") {
		testRespawnEngine(*player);
		return 0;
	}
	if (scenario == "commands_disabled") {
		player->setVarStats(STAT_MAGICPOINTS, 10);
		carry(*player, 3160, 65001);
		require(!EasyMode::useConsumable(*player, "uh"), "command feature gate");
		return 0;
	}
	if (scenario == "target_disabled") {
		require(!EasyMode::setAutoTarget(*player, true), "auto-target feature gate");
		EasyMode::updateAutoTarget(*player, 1000);
		require(!player->getAttackedCreature(), "disabled auto-target never acquires");
		return 0;
	}
	if (scenario == "gold_disabled") {
		Container* corpse = corpseAt(100, 101);
		addCoin(*corpse, 3031, 20);
		EasyMode::onLootGenerated(*corpse);
		require(EasyMode::collectGold(*player) == 0 && contentsValue(*corpse) == 20, "auto-gold feature gate");
		return 0;
	}
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
	Player* drinker = makePlayer(102, 100);
	drinker->setVarStats(STAT_MAXMANAPOINTS, 500);
	Item* vial = carry(*drinker, 2874, 65004, 11);
	require(!EasyMode::useConsumable(*drinker, "mana"), "wrong fluid subtype rejected");
	g_game.transformItem(vial, 2874, 10);
	g_game.playerSay(drinker->getID(), 0, TALKTYPE_SAY, "", "!mana");
	require(drinker->getMana() >= 50 && drinker->getMana() <= 100 && vial->getFluidType() == 10, "real mana restoration and retained subtype");
	const uint32_t mana = drinker->getMana();
	require(!EasyMode::useConsumable(*drinker, "mana") && drinker->getMana() == mana, "mana shares item action clock");
	Player* bomber = makePlayer(100, 102);
	bomber->setVarStats(STAT_MAGICPOINTS, 10);
	Item* explosion = carry(*bomber, 3200, 65003, 6);
	require(!EasyMode::useConsumable(*bomber, "explo"), "missing offensive target fails");
	bomber->setAttackedCreature(healer);
	require(!EasyMode::useConsumable(*bomber, "explo"), "player target fails");
	Monster* target = makeMonster(1000, 100, 104);
	Monster* adjacent = makeMonster(1000, 101, 104);
	Monster* diagonal = makeMonster(1000, 101, 105);
	require(bomber->setAttackedCreature(target), "set offensive target");
	g_game.playerSay(bomber->getID(), 0, TALKTYPE_SAY, "", "!explo");
	require(target->getHealth() < 1000 && adjacent->getHealth() < 1000, "Explosion preserves existing cross area");
	require(diagonal->getHealth() == 1000 && explosion->getCharges() == 6, "Explosion diagonal exclusion and endless charges");
	require(!EasyMode::useConsumable(*bomber, "explo"), "offensive exhaustion/action clock retained");
	Player* fireCaster = makePlayer(105, 100);
	fireCaster->setVarStats(STAT_MAGICPOINTS, 10);
	Item* gfb = carry(*fireCaster, 3191, 65002, 4);
	Monster* fireTarget = makeMonster(1000, 105, 104);
	Monster* fireDiagonal = makeMonster(1000, 106, 105);
	Monster* outside = makeMonster(1000, 109, 104);
	require(fireCaster->setAttackedCreature(fireTarget), "set GFB target");
	g_game.playerSay(fireCaster->getID(), 0, TALKTYPE_SAY, "", "!gfb");
	require(fireTarget->getHealth() < 1000 && fireDiagonal->getHealth() < 1000, "GFB preserves real circular area");
	require(outside->getHealth() == 1000 && gfb->getCharges() == 4, "GFB outside area and charges");
	Player* blocked = makePlayer(109, 94);
	blocked->setVarStats(STAT_MAGICPOINTS, 10);
	Item* blockedRune = carry(*blocked, 3191, 65002, 4);
	Monster* blockedTarget = makeMonster(1000, 109, 98);
	uint16_t wallId = 0;
	for (size_t id = 100; id < Item::items.size(); ++id) {
		const ItemType& type = Item::items[id];
		if (type.id && !type.isGroundTile() && type.blockSolid && type.blockProjectile && !type.moveable) { wallId = id; break; }
	}
	require(wallId != 0, "find projectile-blocking wall");
	require(g_game.internalAddItem(g_game.map.getTile(109, 96, 7), Item::CreateItem(wallId), INDEX_WHEREEVER, FLAG_NOLIMIT) == RETURNVALUE_NOERROR, "place wall");
	require(blocked->setAttackedCreature(blockedTarget), "select visible target behind wall");
	require(!EasyMode::useConsumable(*blocked, "gfb") && blockedTarget->getHealth() == 1000 && blockedRune->getCharges() == 4, "wall prevents command damage");
	fireCaster->getTile()->setFlag(TILESTATE_PROTECTIONZONE);
	require(!EasyMode::useConsumable(*fireCaster, "gfb"), "protection zone preserved");
	fireCaster->getTile()->resetFlag(TILESTATE_PROTECTIONZONE);
	Player* hunter = makePlayer(90, 94);
	Monster* wounded = makeMonster(1000, 94, 94);
	wounded->changeHealth(-850);
	Monster* healthy = makeMonster(400, 93, 94);
	makeMonster(5000, 92, 93, false);
	makeMonster(6000, 95, 94, true, false);
	Monster* summon = makeMonster(7000, 96, 94);
	hunter->addSummon(summon);
	Monster* invisible = makeMonster(8000, 97, 94);
	invisible->addCondition(Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_INVISIBLE, 100000));
	makePlayer(91, 94);
	EasyMode::updateAutoTarget(*hunter, 1000);
	require(!hunter->getAttackedCreature(), "auto-target starts inactive");
	g_game.playerSay(hunter->getID(), 0, TALKTYPE_SAY, "", "!autotarget on");
	EasyMode::updateAutoTarget(*hunter, 1000);
	require(hunter->getAttackedCreature() == healthy, "highest CURRENT HP eligible hostile selected");
	Monster* stronger = makeMonster(900, 96, 95);
	EasyMode::updateAutoTarget(*hunter, 2000);
	require(hunter->getAttackedCreature() == healthy, "stronger newcomer does not steal target");
	g_game.playerSetAttackedCreature(hunter->getID(), wounded->getID());
	EasyMode::updateAutoTarget(*hunter, 2000);
	require(hunter->getAttackedCreature() == wounded, "manual target overrides automation");
	g_game.playerCancelAttackAndFollow(hunter->getID());
	EasyMode::updateAutoTarget(*hunter, 3000);
	require(!hunter->getAttackedCreature() && !hunter->getEasyModeAutoTargetState().active, "Escape suspends reacquisition");
	require(EasyMode::setAutoTarget(*hunter, true), "explicit resume");
	EasyMode::updateAutoTarget(*hunter, 4000);
	require(hunter->getAttackedCreature() == stronger, "resume acquires highest living target");
	stronger->changeHealth(-stronger->getHealth());
	EasyMode::updateAutoTarget(*hunter, 4000);
	require(hunter->getAttackedCreature() == healthy, "dead automatic target released");
	g_game.playerSay(hunter->getID(), 0, TALKTYPE_SAY, "", "!autotarget on");
	for (int dx = -1; dx <= 1; ++dx) {
		for (int dy = -1; dy <= 1; ++dy) {
			if (dx == 0 && dy == 0) continue;
			require(g_game.internalAddItem(g_game.map.getTile(93 + dx, 94 + dy, 7), Item::CreateItem(wallId), INDEX_WHEREEVER, FLAG_NOLIMIT) == RETURNVALUE_NOERROR, "enclose target");
		}
	}
	EasyMode::updateAutoTarget(*hunter, 5000);
	g_game.playerSay(hunter->getID(), 0, TALKTYPE_SAY, "", "!autotarget on");
	EasyMode::updateAutoTarget(*hunter, 7999);
	require(hunter->getAttackedCreature() == healthy, "transient unreachability retains target");
	EasyMode::updateAutoTarget(*hunter, 8000);
	require(hunter->getAttackedCreature() != healthy, "bounded unreachable target released");
	g_game.playerFollowCreature(hunter->getID(), healer->getID());
	require(!hunter->getEasyModeAutoTargetState().active, "manual follow suspends automation");
	Player* collector = makePlayer(107, 106);
	Item* carriedRune = carry(*collector, 3160);
	Container* carriedBag = carriedRune->getParent()->getItem()->getContainer();
	Item* existingGold = addCoin(*carriedBag, 3031, 95);
	Container* corpse = corpseAt(107, 107);
	Container* lootBag = Item::CreateItem(2853)->getContainer();
	Container* deepBag = Item::CreateItem(2853)->getContainer();
	corpse->internalAddThing(lootBag);
	lootBag->internalAddThing(deepBag);
	addCoin(*deepBag, 3031, 30);
	addCoin(*corpse, 3031, 100);
	addCoin(*lootBag, 3035, 6);
	addCoin(*deepBag, 3043, 1);
	Item* special = addCoin(*corpse, 3031, 50, 65001);
	const uint64_t originalValue = collector->getMoney() + contentsValue(*corpse);
	EasyMode::onLootGenerated(*corpse);
	EasyMode::onLootGenerated(*corpse);
	require(EasyMode::collectGold(*collector) == 10730, "nested real gold/platinum/crystal transferred");
	require(existingGold->getItemCount() == 100 && special->getParent() == corpse, "scoped stacking and special currency exclusion");
	require(collector->getMoney() + contentsValue(*corpse) == originalValue, "total currency value conserved");
	require(EasyMode::collectGold(*collector) == 0, "reprocessing cannot duplicate value");
	addCoin(*corpse, 3031, 20);
	EasyMode::onLootGenerated(*corpse);
	require(EasyMode::collectGold(*collector) == 0, "post-registration inserted currency excluded");
	Player* competitor = makePlayer(108, 106);
	require(EasyMode::collectGold(*competitor) == 0 && competitor->getMoney() == 0, "first successful collector wins");
	Player* limited = makePlayer(111, 106);
	Item* heavy = Item::CreateItem(3264);
	heavy->setIntAttr(ITEM_ATTRIBUTE_WEIGHT, limited->getCapacity() - 50);
	require(g_game.internalAddItem(limited, heavy, CONST_SLOT_RIGHT) == RETURNVALUE_NOERROR, "set capacity-limited inventory");
	require(limited->getFreeCapacity() == 50, "five-coin capacity fixture");
	Container* partial = corpseAt(111, 107);
	Item* partialGold = addCoin(*partial, 3031, 20);
	EasyMode::onLootGenerated(*partial);
	require(EasyMode::collectGold(*limited) == 5 && limited->getMoney() == 5 && partialGold->getItemCount() == 15, "partial movement respects capacity and leaves remainder");
	require(EasyMode::collectGold(*limited) == 0 && partialGold->getItemCount() == 15, "full capacity never drops coins to ground");
	require(EasyMode::collectGold(*competitor) == 15 && competitor->getMoney() == 15, "another collector can take actual remainder");
	Container* relocated = corpseAt(108, 107);
	addCoin(*relocated, 3031, 10);
	EasyMode::onLootGenerated(*relocated);
	require(g_game.internalMoveItem(relocated->getParent(), g_game.map.getTile(108, 108, 7), INDEX_WHEREEVER, relocated, 1, nullptr) == RETURNVALUE_NOERROR, "move registered corpse");
	require(EasyMode::collectGold(*competitor) == 0 && contentsValue(*relocated) == 10, "relocated corpse invalidated safely");
	MonsterType* lootType;
	Monster* lootMonster = makeMonster(5, 110, 108, true, true, &lootType);
	lootType->info.lookcorpse = 3994;
	LootBlock goldLoot;
	goldLoot.id = 3031;
	goldLoot.countmax = 100;
	goldLoot.chance = MAX_LOOTCHANCE;
	lootType->info.lootItems.push_back(goldLoot);
	getRandomGenerator().seed(12345);
	lootMonster->changeHealth(-5);
	lootMonster->onDeath();
	Container* generated = nullptr;
	for (Item* item : *g_game.map.getTile(110, 108, 7)->getItemList()) {
		if (item->getID() == 3994) generated = item->getContainer();
	}
	require(generated && contentsValue(*generated) > 0, "real death generates currency");
	const uint64_t deathValue = contentsValue(*generated);
	Player* deathCollector = makePlayer(110, 107);
	require(EasyMode::collectGold(*deathCollector) == deathValue && deathCollector->getMoney() == deathValue, "real post-loot notification registers completed generated loot");
	Container* tradeCorpse = corpseAt(108, 107);
	Item* offered = addCoin(*tradeCorpse, 3031, 10);
	EasyMode::onLootGenerated(*tradeCorpse);
	collector->addContainer(1, tradeCorpse);
	g_game.playerRequestTrade(collector->getID(), Position(0xFFFF, 0x41, tradeCorpse->getThingIndex(offered)), 0, competitor->getID(), 3031);
	require(collector->getTradeItem() == offered, "offer source currency through real trade API");
	require(EasyMode::collectGold(*competitor) == 0 && offered->getParent() == tradeCorpse, "active source trade is protected");
	g_game.playerCloseTrade(collector->getID());
	require(EasyMode::collectGold(*competitor) == 10, "collection resumes after trade cancellation");
	Container* manuallyLooted = corpseAt(108, 107);
	Item* manualGold = addCoin(*manuallyLooted, 3031, 10);
	EasyMode::onLootGenerated(*manuallyLooted);
	require(g_game.internalMoveItem(manuallyLooted, competitor, INDEX_WHEREEVER, manualGold, 10, nullptr) == RETURNVALUE_NOERROR, "manual currency pickup");
	g_game.cleanup();
	require(EasyMode::collectGold(*collector) == 0, "manually moved currency is not credited again after cleanup");
	Container* removedCorpse = corpseAt(108, 107);
	addCoin(*removedCorpse, 3031, 10);
	EasyMode::onLootGenerated(*removedCorpse);
	require(g_game.internalRemoveItem(removedCorpse) == RETURNVALUE_NOERROR, "remove registered corpse");
	g_game.cleanup();
	require(EasyMode::collectGold(*collector) == 0, "removed corpse handles survive cleanup safely");
	Player* full = makePlayer(109, 106);
	for (int slot = CONST_SLOT_FIRST; slot <= CONST_SLOT_LAST; ++slot) {
		// Saturated inventory fixture; production placement still uses queryAdd.
		static_cast<Cylinder&>(*full).addThing(slot, Item::CreateItem(3031, 100));
	}
	Container* noDestination = corpseAt(109, 107);
	addCoin(*noDestination, 3031, 10);
	EasyMode::onLootGenerated(*noDestination);
	require(EasyMode::collectGold(*full) == 0 && contentsValue(*noDestination) == 10, "full destinations leave currency in corpse");
	std::cout << "Native engine scenario passed: " << scenario << std::endl;
	return 0;
}

#include "otpch.h"
#include "easyspawn.h"
#include "configmanager.h"
#include "game.h"
#include "monster.h"
#include "player.h"
#include "spawn.h"
#include "tools.h"
#include <boost/filesystem.hpp>
#include <cstdlib>
#include <fstream>

extern Game g_game;
extern ConfigManager g_config;
extern Monsters g_monsters;

static void check(bool condition, const char* message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << std::endl;
		std::exit(1);
	}
}

struct SpawnTestAccess {
	static void insert(Spawn& spawn, MonsterType& type, const Position& position,
	                   const EasyMode::RespawnPolicy* policy)
	{
		spawnBlock_t block;
		block.pos = position;
		block.mType = &type;
		block.lastSpawn = 0;
		block.interval = 60000;
		block.direction = DIRECTION_NORTH;
		if (policy) {
			block.easyRespawn = true;
			block.easyPolicy = *policy;
			spawn.hasEasyRespawn = true;
		}
		spawn.spawnMap.emplace(1, block);
	}
	static spawnBlock_t& block(Spawn& spawn) { return spawn.spawnMap.at(1); }
	static Monster* creature(Spawn& spawn) {
		const auto it = spawn.spawnedMap.find(1);
		return it == spawn.spawnedMap.end() ? nullptr : it->second;
	}
	static void poll(Spawn& spawn) {
		// No scheduler thread runs in the fixture. Consume its queued event first.
		spawn.stopEvent();
		spawn.checkSpawn();
		spawn.stopEvent();
	}
	static uint32_t nextCheck(Spawn& spawn) { return spawn.nextSpawnCheck(); }
	static void pollingInterval(Spawn& spawn, uint32_t interval) { spawn.interval = interval; }
	static void scheduledSentinel(Spawn& spawn) { spawn.checkSpawnEvent = 1; }
};

static void testClassicRandomCalls(Spawn& classic)
{
	auto& generator = getRandomGenerator();
	const auto saved = generator;
	// Exercise the historical random branch too, even though current loading
	// caps the polling member at 60 seconds. normal_random caches a distribution
	// sample, so compare pairs and generator state rather than sample values.
	SpawnTestAccess::pollingInterval(classic, 600000);
	SpawnTestAccess::block(classic).lastSpawn = OTSYS_TIME() + 600000;
	generator.seed(72172);
	for (int i = 0; i < 2; ++i) {
		const uint32_t delay = classic.getInterval();
		check(delay >= 300000 && delay <= 600000, "historical random polling bounds");
	}
	const auto baseline = generator;
	generator.seed(72172);
	for (int i = 0; i < 2; ++i) {
		const uint32_t delay = SpawnTestAccess::nextCheck(classic);
		check(delay >= 300000 && delay <= 600000, "unclassified next check retains random polling bounds");
	}
	check(generator == baseline, "Classic nextSpawnCheck consumes exactly the historical RNG draws");
	generator.seed(72172);
	for (int i = 0; i < 2; ++i) {
		classic.startSpawnCheck();
		classic.stopEvent();
	}
	check(generator == baseline, "Classic initial scheduling consumes exactly the historical RNG draws");
	// The fixture deliberately starts no scheduler thread, so addEvent returns
	// zero. A sentinel separately exercises the existing nonzero-event guard.
	const auto alreadyScheduled = generator;
	SpawnTestAccess::scheduledSentinel(classic);
	classic.startSpawnCheck();
	check(generator == alreadyScheduled, "already scheduled Classic check consumes no extra randomness");
	classic.stopEvent();
	generator.seed(72172);
	for (int i = 0; i < 2; ++i) SpawnTestAccess::poll(classic);
	check(generator == baseline, "Classic ongoing polling consumes exactly the historical RNG draws");
	SpawnTestAccess::pollingInterval(classic, 60000);
	const auto beforeDeterministic = generator;
	check(SpawnTestAccess::nextCheck(classic) == 60000, "normal Classic polling remains deterministic");
	classic.startSpawnCheck();
	classic.stopEvent();
	check(generator == beforeDeterministic, "normal Classic scheduling makes no RNG call");
	generator = saved;
}

static void writeFixture(const std::string& filename, const std::string& text)
{
	std::ofstream file(filename);
	file << text;
	check(file.good(), "write native respawn fixture in throwaway data directory");
}

static void testActualSpawnLoading()
{
	// The legacy ConfigManager default overrides XML times with one second.
	// Exercise matching against the actual 60-second XML interval explicitly.
	{
		std::ofstream config("config.lua", std::ios::app);
		config << "\nrateSpawn = 0\n";
	}
	check(g_config.load(), "configure XML spawn times in throwaway fixture");
	boost::filesystem::create_directories("data/monster");
	boost::filesystem::create_directories("data/zedas");
	writeFixture("data/monster/monsters.xml", "<monsters>"
	    "<monster name=\"Fixture Rat\" file=\"fixture-rat.xml\"/>"
	    "<monster name=\"Fixture Passive\" file=\"fixture-passive.xml\"/></monsters>");
	writeFixture("data/monster/fixture-rat.xml", "<monster name=\"Fixture Rat\">"
	    "<health now=\"100\" max=\"100\"/></monster>");
	writeFixture("data/monster/fixture-passive.xml", "<monster name=\"Fixture Passive\">"
	    "<health now=\"100\" max=\"100\"/><flags><flag hostile=\"0\"/></flags></monster>");
	check(g_monsters.loadFromXml(), "load controlled types through actual monster XML loader");
	writeFixture("fixture-world-spawns.xml", "<spawns><spawn centerx=\"106\" centery=\"104\" centerz=\"7\" radius=\"1\">"
	    "<monster name=\"Fixture Rat\" x=\"0\" y=\"0\" spawntime=\"60\"/></spawn></spawns>");
	const std::string policyPath = g_config.getString(ConfigManager::EASY_MODE_RESPAWN_POLICY_FILE);
	const std::string entry = "<spawn monster=\"Fixture Rat\" x=\"106\" y=\"104\" z=\"7\" classification=\"ordinary\" "
	    "baseSeconds=\"60\" intervalPercent=\"50\" minSeconds=\"5\"/>";
	for (int policy = 0; policy < 3; ++policy) {
		if (policy == 0) std::remove(policyPath.c_str());
		else writeFixture(policyPath, "<easyspawns version=\"1\">" + (policy == 2 ? entry : "") + "</easyspawns>");
		Spawns world;
		std::ostringstream diagnostics;
		auto previous = std::cout.rdbuf(diagnostics.rdbuf());
		const auto beforeLoad = getRandomGenerator();
		const bool loaded = world.loadFromXml("fixture-world-spawns.xml");
		std::cout.rdbuf(previous);
		check(loaded, "missing, empty, or exact manifest never prevents Classic world loading");
		check(getRandomGenerator() == beforeLoad, "manifest/world loading consumes no respawn RNG");
		check(diagnostics.str().find(policy == 0 ? "policy disabled" : policy == 1 ? "contains 0" : "contains 1") != std::string::npos,
		      "actual world loader reads and diagnoses configured missing/empty/exact manifest");
		world.startup();
		CreatureVector* creatures = g_game.map.getTile(106, 104, 7)->getCreatures();
		Monster* monster = nullptr;
		if (creatures) for (Creature* creature : *creatures) if (creature->getMonster()) monster = creature->getMonster();
		check(monster && monster->getName() == "Fixture Rat", "actual XML world slot creates intended monster");
		check(g_game.removeCreature(monster), "remove actual XML-loaded fixture monster");
		world.clear();
		g_game.cleanup();
	}
	EasyMode::RespawnPolicy timing;
	timing.intervalPercent = 50;
	timing.minimumMs = 5000;
	Spawn reviewed(Position(106, 104, 7), 1);
	check(reviewed.addMonster("Fixture Rat", Position(106, 104, 7), DIRECTION_NORTH, 60000, &timing) &&
	      SpawnTestAccess::block(reviewed).easyRespawn, "actual addMonster accepts explicit ordinary hostile policy");
	Spawn unsafe(Position(107, 104, 7), 1);
	check(unsafe.addMonster("Fixture Passive", Position(107, 104, 7), DIRECTION_NORTH, 60000, &timing) &&
	      !SpawnTestAccess::block(unsafe).easyRespawn, "actual addMonster refuses non-hostile slot despite explicit policy");
}

void testRespawnEngine(Player& observer)
{
	// The shared native fixture supplies real floor tiles and a real player.
	// Types intentionally survive the process-lifetime world fixture.
	MonsterType* type = new MonsterType();
	type->name = "test ordinary respawn";
	type->nameDescription = "a test ordinary respawn";
	type->info.health = type->info.healthMax = 100;
	const Position position(102, 100, 7);
	EasyMode::RespawnPolicy policy;
	policy.intervalPercent = 50;
	policy.minimumMs = 5000;
	Spawn eligible(position, 1);
	SpawnTestAccess::insert(eligible, *type, position, &policy);
	eligible.startup();
	Monster* original = SpawnTestAccess::creature(eligible);
	check(original && !original->isRemoved(), "eligible world spawn starts normally");
	check(g_game.removeCreature(original), "remove real eligible monster");
	eligible.stopEvent();
	eligible.cleanup();
	spawnBlock_t& block = SpawnTestAccess::block(eligible);
	check(!SpawnTestAccess::creature(eligible), "cleanup releases removed spawn occupant");
	check(block.easyDeadline.isPending() && block.easyDeadline.delay() == 30000,
	      "actual Spawn cleanup installs accelerated per-slot deadline");
	check(SpawnTestAccess::nextCheck(eligible) <= 30000,
	      "actual Spawn schedules before historical 60-second poll");
	block.easyDeadline.postpone(OTSYS_TIME() - 30001);
	SpawnTestAccess::poll(eligible);
	check(!SpawnTestAccess::creature(eligible) && block.easyDeadline.eligibleAt() > OTSYS_TIME(),
	      "real nearby player suppresses an eligible replacement");
	check(block.easyDeadline.delay() == 30000,
	      "nearby player suppression preserves already sampled duration");
	check(g_game.internalTeleport(&observer, Position(88, 100, 7)) == RETURNVALUE_NOERROR,
	      "observer leaves spawn suppression area");
	g_game.map.clearSpectatorCache();
	block.easyDeadline.postpone(OTSYS_TIME() - 30001);
	SpawnTestAccess::poll(eligible);
	Monster* replacement = SpawnTestAccess::creature(eligible);
	check(replacement && !replacement->isRemoved() && !block.easyDeadline.isPending(),
	      "eligible deadline creates real replacement and clears session timer");
	check(g_game.removeCreature(replacement), "remove replacement fixture monster");
	eligible.stopEvent();
	eligible.cleanup();

	Spawn classic(position, 1);
	SpawnTestAccess::insert(classic, *type, position, nullptr);
	classic.startup();
	original = SpawnTestAccess::creature(classic);
	check(original && g_game.removeCreature(original), "remove real unclassified monster");
	classic.stopEvent();
	classic.cleanup();
	spawnBlock_t& ordinary = SpawnTestAccess::block(classic);
	check(!ordinary.easyRespawn && !ordinary.easyDeadline.isPending() &&
	      SpawnTestAccess::nextCheck(classic) == 60000,
	      "unclassified slot retains historical polling with no Easy timer");
	ordinary.lastSpawn = OTSYS_TIME() - 30001;
	SpawnTestAccess::poll(classic);
	check(!SpawnTestAccess::creature(classic), "unclassified spawn cannot use accelerated deadline");
	ordinary.lastSpawn = OTSYS_TIME() - 60001;
	SpawnTestAccess::poll(classic);
	replacement = SpawnTestAccess::creature(classic);
	check(replacement && !replacement->isRemoved(), "unclassified spawn retains historical base interval");
	check(g_game.removeCreature(replacement), "remove Classic replacement fixture monster");
	classic.stopEvent();
	classic.cleanup();
	testClassicRandomCalls(classic);
	g_game.cleanup();
	testActualSpawnLoading();
}

#include "easyspawn.h"

#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#include <unistd.h>

static void require(bool condition, const char* message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

struct TemporaryPolicy {
	char name[64] = "/tmp/zedas-respawn-policy-XXXXXX";
	TemporaryPolicy() { int fd = mkstemp(name); require(fd != -1, "create temporary policy"); close(fd); }
	~TemporaryPolicy() { std::remove(name); }
	void write(const std::string& content) { std::ofstream file(name); file << content; }
};

static const std::string validEntry =
	"<spawn monster=\"Rat\" x=\"101\" y=\"102\" z=\"7\" classification=\"ordinary\" "
	"baseSeconds=\"60\" intervalPercent=\"50\" minSeconds=\"5\"/>";

static std::string changed(std::string source, const std::string& before, const std::string& after)
{
	const auto pos = source.find(before);
	require(pos != std::string::npos, "test variant matches original input");
	source.replace(pos, before.size(), after);
	return source;
}

int main(int argc, char** argv)
{
	using namespace EasyMode;
	require(argc == 2, "fixture path argument");
	RespawnManifest manifest;
	std::string error;
	require(manifest.load(argv[1], error), "load controlled fixture");
	require(manifest.size() == 2, "only two explicitly classified test slots");
	const RespawnPolicy* rat = manifest.find("rat", 101, 102, 7, 60000);
	require(rat && rat->intervalPercent == 50 && rat->minimumMs == 5000 && rat->populationFloor == 0, "exact reviewed slot matches case-insensitive monster lookup");
	require(!manifest.find("rat", 102, 102, 7, 60000), "another spawn of same monster remains Classic");
	require(!manifest.find("rat", 101, 102, 8, 60000), "another floor remains Classic");
	require(!manifest.find("rat", 101, 102, 7, 61000), "rateSpawn/map interval changes invalidate stale review");
	require(!manifest.find("demon", 101, 102, 7, 60000), "another species remains Classic");
	const RespawnPolicy ratPolicy = *rat;

	// Fake clocks and samplers check observable eligibility rather than sleeping.
	int samples = 0;
	RespawnDeadline timer;
	auto noRandom = [&](int32_t, int32_t) -> int32_t { ++samples; throw std::runtime_error("unexpected random call"); };
	timer.begin(ratPolicy, 60000, 1000, 100000, noRandom);
	require(timer.eligibleAt() == 130000 && timer.delay() == 30000 && samples == 0, "explicit deterministic acceleration adjusts real deadline");
	for (int64_t now = 100000; now < 130000; now += 1000) {
		require(!timer.isDue(now) && timer.eligibleAt() == 130000, "polls do not reroll or move eligibility");
	}
	require(timer.nextCheck(100000, 60000) == 30000 && timer.nextCheck(129999, 60000) == 1, "polling reaches adjusted deadline rather than waiting Classic interval");
	require(timer.isDue(130000), "eligible precisely at sampled deadline");
	timer.postpone(130000);
	require(timer.eligibleAt() == 160000 && !timer.isDue(159999) && samples == 0, "nearby player restarts same sampled delay");
	require(timer.nextCheck(160001, 60000) == 1000, "obstructed placement cannot create tight retry loop");
	timer.clear();
	require(!timer.isDue(999999) && timer.nextCheck(999999, 60000) == 60000, "successful spawn clears pending state");

	require(populationRespawnInterval(600000, 200) == 600000, "legacy population low boundary");
	require(populationRespawnInterval(600000, 201) == 600000, "legacy population integer division at 201");
	require(populationRespawnInterval(600000, 800) == 240000, "legacy population at 800");
	require(populationRespawnInterval(600000, 801) == 240000, "legacy population above 800");
	const RespawnPolicy* population = manifest.find("Cyclops", 150, 151, 7, 600000);
	require(population, "controlled population policy matches");
	timer.begin(*population, 600000, 1, 5000, [&](int32_t low, int32_t high) {
		++samples;
		require(low == 120000 && high == 240000, "explicit population floor uses existing curve and normal-random bounds");
		return low;
	});
	require(samples == 1 && timer.eligibleAt() == 125000, "population sampling occurs once per empty cycle");
	for (int i = 0; i < 20; ++i) timer.nextCheck(5000 + i * 1000, 60000);
	timer.postpone(125000);
	require(samples == 1 && timer.eligibleAt() == 245000, "retries/player suppression never resample");
	RespawnPolicy clamped = *population;
	clamped.intervalPercent = 1;
	clamped.minimumMs = 90000;
	timer.begin(clamped, 600000, 1000, 0, [](int32_t low, int32_t) { return low; });
	require(timer.delay() == 90000, "minimum safe interval clamps strong acceleration");
	timer.begin(*population, 500000, 1000, 0, noRandom);
	require(timer.delay() == 500000 && samples == 1, "legacy population threshold remains strictly greater than 500 seconds");
	timer.begin(ratPolicy, 60000, 0, std::numeric_limits<int64_t>::max() - 1, noRandom);
	require(timer.eligibleAt() == std::numeric_limits<int64_t>::max(), "clock arithmetic cannot overflow");

	TemporaryPolicy file;
	const std::string prefix = "<easyspawns version=\"1\">", suffix = "</easyspawns>";
	std::vector<std::string> invalid = {
		"<easyspawns version=\"2\"/>", "<easyspawns version=\"1\" extra=\"1\"/>",
		prefix + "<unknown/>" + suffix, prefix + validEntry + validEntry + suffix,
		prefix + validEntry + changed(validEntry, "Rat", "rAT") + suffix,
		prefix + changed(validEntry, "ordinary", "boss") + suffix,
		prefix + changed(validEntry, " classification=\"ordinary\"", "") + suffix,
		prefix + changed(validEntry, "intervalPercent", "intervalTypo") + suffix,
		prefix + changed(validEntry, "x=\"101\"", "x=\"65536\"") + suffix,
		prefix + changed(validEntry, "z=\"7\"", "z=\"16\"") + suffix,
		prefix + changed(validEntry, "baseSeconds=\"60\"", "baseSeconds=\"2147484\"") + suffix,
		prefix + changed(validEntry, "minSeconds=\"5\"", "minSeconds=\"1\"") + suffix,
		prefix + changed(validEntry, "minSeconds=\"5\"", "minSeconds=\"61\"") + suffix,
		prefix + changed(validEntry, "intervalPercent=\"50\"", "intervalPercent=\"101\"") + suffix,
		prefix + changed(validEntry, "intervalPercent=\"50\"", "intervalPercent=\"0\"") + suffix,
		prefix + changed(validEntry, "intervalPercent=\"50\"", "intervalPercent=\"0.5\"") + suffix,
		prefix + changed(validEntry, "intervalPercent=\"50\"", "intervalPercent=\"-1\"") + suffix,
		prefix + changed(validEntry, "intervalPercent=\"50\"", "intervalPercent=\"999999999999999999999\"") + suffix,
		prefix + changed(validEntry, "x=\"101\"", "x=\"101\" x=\"102\"") + suffix,
		prefix + changed(validEntry, "/>", "><spawn/></spawn>") + suffix,
		prefix + validEntry + suffix + prefix + suffix,
		prefix + changed(validEntry, "/>", " populationFloor=\"10001\"/>") + suffix,
	};
	for (const auto& xml : invalid) {
		file.write(xml);
		require(!manifest.load(file.name, error) && !error.empty() && manifest.size() == 0,
		        "invalid manifest fails closed without partial or stale policies");
	}
	file.write(prefix + validEntry + changed(validEntry, "Rat", "Troll") + suffix);
	require(manifest.load(file.name, error) && manifest.size() == 2, "different species at same location are independently reviewed");
	file.write(prefix + suffix);
	require(manifest.load(file.name, error) && manifest.size() == 0, "empty production policy enables no acceleration");
	require(!manifest.load("/this/manifest/does/not/exist", error) && manifest.size() == 0, "missing policy fails closed");
	return 0;
}

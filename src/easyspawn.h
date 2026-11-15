#ifndef ZEDAS_EASYSPAWN_H
#define ZEDAS_EASYSPAWN_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>

namespace EasyMode {

// Policies are opt-in for one reviewed world-spawn slot, never a monster species.
struct RespawnPolicy {
	uint32_t intervalPercent = 0; // Zero inherits the runtime global percentage.
	uint32_t minimumMs = 120000;
	uint32_t populationFloor = 0; // Zero leaves population scaling/randomness disabled.
};

struct RespawnSettings {
    bool enabled = true, valid = true;
    uint32_t percent = 50;
    size_t reviewed = 0, rejected = 0;
};
RespawnSettings& respawnSettings();

class RespawnManifest
{
	public:
		bool load(const std::string& filename, std::string& error);
		const RespawnPolicy* find(const std::string& monster, uint16_t x, uint16_t y,
		                         uint8_t z, uint32_t baseInterval) const;
		size_t size() const { return entries.size(); }

	private:
		struct Key {
			std::string monster;
			uint16_t x;
			uint16_t y;
			uint8_t z;
			bool operator<(const Key& other) const;
		};
		struct Entry {
			uint32_t baseInterval;
			RespawnPolicy policy;
		};
		std::map<Key, Entry> entries;
};

// The same population curve as Spawn::getInterval, expressed without sampling.
uint32_t populationRespawnInterval(uint32_t interval, size_t playersOnline);

class RespawnDeadline
{
	public:
		using Random = std::function<int32_t(int32_t, int32_t)>;
		void begin(const RespawnPolicy& policy, uint32_t interval, size_t playersOnline,
		           int64_t now, const Random& random);
		void postpone(int64_t now); // Test/manual deadline anchor using sampled delay.
        void retry(int64_t now) { if (pending) deadline = now > INT64_MAX-5000 ? INT64_MAX : now+5000; }
		void clear() { pending = false; }
		bool isPending() const { return pending; }
		bool isDue(int64_t now) const { return pending && now >= deadline; }
		int64_t eligibleAt() const { return deadline; }
		uint32_t delay() const { return sampledDelay; }
		uint32_t nextCheck(int64_t now, uint32_t classicPoll) const;

	private:
		bool pending = false;
		uint32_t sampledDelay = 0;
		int64_t deadline = 0;
};

}

#endif

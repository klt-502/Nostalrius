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
	uint32_t intervalPercent = 100;
	uint32_t minimumMs = 2000;
	uint32_t populationFloor = 0; // Zero leaves population scaling/randomness disabled.
};

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
		void postpone(int64_t now);
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

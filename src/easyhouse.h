#ifndef ZEDAS_EASYHOUSE_H
#define ZEDAS_EASYHOUSE_H
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <cstdint>
#include <ostream>
#include "position.h"
class Map;
namespace EasyMode {
struct HouseRole {
	uint16_t item = 0, count = 1;
	bool loot = false;
	std::string name;
};
struct HousePoint {
	std::string role;
	Position pos;
	uint16_t ground = 0;
};
struct HouseInterior {
	std::string id, profile, review;
	Position anchor;
	uint16_t ground = 0;
	std::vector<HousePoint> points;
};
struct HouseCounts {
	size_t approved = 0, fixtures = 0, loot = 0, rejected = 0;
};
class HouseOverlay {
  public:
	using Claim = std::function<bool(const std::string &, const std::string &)>;
	bool load(const std::string &file, const std::string &mapIdentity, std::string &error);
	HouseCounts apply(Map &map, bool furnishing, bool loot, const Claim &claim);
	size_t size() const { return interiors.size(); }

  private:
	std::string identity;
	std::map<std::string, std::map<std::string, HouseRole>> profiles;
	std::vector<HouseInterior> interiors;
	std::set<std::string> processed;
};
// Explicit one-time world issuance, durable reservation before item placement.
bool claimHouseLoot(const std::string &key, const std::string &identity);
void initializeHouses();
} // namespace EasyMode
#endif

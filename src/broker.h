#ifndef ZEDAS_BROKER_H
#define ZEDAS_BROKER_H
#include <cstdint>
#include <string>
#include <memory>
class Player;
class Npc;
class Item;
class Tile;
struct Position;
namespace Broker
{
class Store;
static constexpr int64_t FODDER_GRACE_MS = 0; // The legacy configurable grace is reserved for future categories.
static constexpr int64_t FLOOR_REJOIN_DELAY_MS = 1500;
static constexpr size_t AUTO_BATCH_MIN_INSTANCES = 10;
static constexpr uint64_t AUTO_BATCH_MIN_GROSS = 250;
static constexpr int64_t AUTO_BATCH_IDLE_MS = 120000;
bool enabled();
void initialize();
void initialize(std::unique_ptr<Store> connection);
void tick(Player &owner, int64_t now = 0);
void stop(Player &owner);
void ownerMoved(Player &owner, const Position &oldPos, const Position &newPos);
void command(Player &owner, const std::string &text);
bool canStep(const Npc &npc, const Position &destination);
bool locked(const Item *item);
bool loadCatalog(const std::string &path, std::string &error);
uint64_t price(const Item &item);
} // namespace Broker
#endif

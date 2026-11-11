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
bool enabled();
void initialize();
void initialize(std::unique_ptr<Store> connection);
void tick(Player &owner, int64_t now = 0);
void stop(Player &owner);
void command(Player &owner, const std::string &text);
bool canStep(const Npc &npc, const Position &destination);
bool locked(const Item *item);
bool loadCatalog(const std::string &path, std::string &error);
uint64_t price(const Item &item);
} // namespace Broker
#endif

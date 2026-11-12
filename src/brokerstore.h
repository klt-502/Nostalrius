#ifndef ZEDAS_BROKERSTORE_H
#define ZEDAS_BROKERSTORE_H
#include <mysql/mysql.h>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <stdexcept>

namespace Broker {
constexpr uint64_t MONEY_LIMIT = 9007199254740991ULL; // Historical Lua bank API uses doubles.
constexpr size_t INVENTORY_LIMIT = 20;
constexpr unsigned TRIP_SECONDS = 60;
struct Connection {
    std::string host, user, password, database, socket;
    unsigned port = 3306;
};
using Row = std::map<std::string, std::string>;
using Rows = std::vector<Row>;
uint64_t number(const Row& row, const std::string& key);
uint64_t commission(uint64_t gross, unsigned bps);
struct Bank { uint64_t balance, watermark; };
// Dispatcher-only, dedicated connection. Never reconnect/replay within an operation.
class Store {
public:
    explicit Store(Connection settings);
    virtual ~Store();
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;
    void connect(); // Also obtains the exclusive per-database broker lease.
    bool connected() const { return handle != nullptr; }
    void close();
    std::string key();
    void account(uint32_t owner);
    Row status(uint32_t owner);
    Rows inventory(uint32_t owner);
    Rows fieldInventory(uint32_t owner);
    Row claimNotice(uint32_t owner, bool sale);
    void prepare(const std::string& key, uint32_t owner, uint16_t item, unsigned count, uint64_t price);
    bool prepared(const std::string& key, uint32_t owner);
    void depart(uint32_t owner, const std::string& trip, uint64_t now);
    void settle(uint32_t owner, uint64_t now, unsigned bps);
    void returned(uint32_t owner, uint64_t now);
    Bank reconcile(uint32_t owner, uint64_t baseline, uint64_t watermark);
    Bank cashout(uint32_t owner, const std::string& key, uint64_t baseline, uint64_t watermark);
    Rows query(const std::string& sql);
    void execute(const std::string& sql);
    // SQL expression used by the actual historical player-save path.
    static std::string savedBank(uint32_t owner, uint64_t baseline, uint64_t watermark);
#ifdef ZEDAS_BROKER_FAULT_TEST
    enum class Fault { None, BeforeCommit, AfterCommit };
    Fault fault = Fault::None;
#endif
private:
    Connection settings;
    MYSQL* handle = nullptr;
    std::string session;
    uint64_t sequence = 0;
    std::string quote(const std::string& value);
    void begin();
protected:
    virtual void commit(const char* operation); // Fixture subclasses can lose an acknowledgement.
private:
    void abort();
};
}
#endif

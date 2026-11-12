#include "brokerstore.h"
#include <algorithm>
#include <limits>
#include <sstream>
#include <utility>

namespace Broker {
uint64_t number(const Row& row, const std::string& key)
{
    const auto it = row.find(key);
    if (it == row.end() || it->second.empty()) throw std::runtime_error("missing broker numeric field");
    uint64_t result = 0;
    for (char c : it->second) {
        if (c < '0' || c > '9' || result > (std::numeric_limits<uint64_t>::max() - (c - '0')) / 10)
            throw std::runtime_error("invalid broker numeric field");
        result = result * 10 + c - '0';
    }
    return result;
}
uint64_t commission(uint64_t gross, unsigned bps)
{
    if (gross > MONEY_LIMIT || bps > 10000) throw std::runtime_error("broker amount/rate outside safe range");
    return (gross / 10000) * bps + ((gross % 10000) * bps) / 10000;
}
Store::Store(Connection value) : settings(std::move(value)) {}
Store::~Store() { close(); }
void Store::close() { if (handle) mysql_close(handle); handle = nullptr; }
void Store::connect()
{
    if (handle) return;
    handle = mysql_init(nullptr);
    if (!handle) throw std::runtime_error("broker mysql_init failed");
    unsigned timeout = 2;
    bool reconnect = false;
    mysql_options(handle, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_options(handle, MYSQL_OPT_READ_TIMEOUT, &timeout);
    mysql_options(handle, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
    mysql_options(handle, MYSQL_OPT_RECONNECT, &reconnect);
    if (!mysql_real_connect(handle, settings.host.c_str(), settings.user.c_str(), settings.password.c_str(),
            settings.database.c_str(), settings.port, settings.socket.empty() ? nullptr : settings.socket.c_str(), 0)) {
        close(); throw std::runtime_error("broker database connection unavailable");
    }
    try {
        // GET_LOCK is connection-scoped, so a disconnected/second process cannot mutate broker state.
        auto lock = query("SELECT GET_LOCK(CONCAT('zedas-broker:',DATABASE()),0) AS acquired");
        if (lock.empty() || number(lock.front(), "acquired") != 1) throw std::runtime_error("broker database already leased");
        auto tables = query("SELECT TABLE_NAME AS name, ENGINE AS engine FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME IN ('players','zedas_broker_accounts','zedas_broker_escrow','zedas_broker_sales','zedas_broker_payouts')");
        if (tables.size() != 5) throw std::runtime_error("apply explicit broker migration first");
        for (const auto& table : tables) if (table.at("engine") != "InnoDB") throw std::runtime_error("broker requires InnoDB tables");
        auto version = query("SELECT version FROM zedas_broker_schema WHERE id=1");
        if (version.size() != 1 || number(version[0], "version") != 2) throw std::runtime_error("broker schema version mismatch");
        if (session.empty()) session = query("SELECT UUID() AS id").at(0).at("id");
    } catch (...) { close(); throw; }
}
Rows Store::query(const std::string& sql)
{
    if (!handle) throw std::runtime_error("broker connection closed");
    if (mysql_real_query(handle, sql.data(), sql.size()) != 0) {
        // Do not retry a statement or continue a possibly lost transaction.
        close(); throw std::runtime_error("broker SQL failed; operation requires reconciliation");
    }
    MYSQL_RES* result = mysql_store_result(handle);
    if (!result) {
        if (mysql_field_count(handle)) { close(); throw std::runtime_error("broker SQL result unavailable"); }
        return {};
    }
    Rows rows;
    MYSQL_FIELD* fields = mysql_fetch_fields(result);
    while (MYSQL_ROW values = mysql_fetch_row(result)) {
        const unsigned long* lengths = mysql_fetch_lengths(result);
        Row row;
        for (unsigned i = 0; i < mysql_num_fields(result); ++i)
            row[fields[i].name] = values[i] ? std::string(values[i], lengths[i]) : "0";
        rows.push_back(std::move(row));
    }
    mysql_free_result(result);
    return rows;
}
void Store::execute(const std::string& sql) { query(sql); }
std::string Store::quote(const std::string& value)
{
    if (!handle) throw std::runtime_error("broker connection closed");
    std::string out(value.size() * 2 + 1, '\0');
    out.resize(mysql_real_escape_string(handle, &out[0], value.data(), value.size()));
    return "'" + out + "'";
}
std::string Store::key() { return session + ":" + std::to_string(++sequence); }
void Store::begin() { execute("START TRANSACTION"); }
void Store::abort() { if (handle) { try { execute("ROLLBACK"); } catch (...) {} } }
void Store::commit(const char* /*operation*/)
{
#ifdef ZEDAS_BROKER_FAULT_TEST
    if (fault == Fault::BeforeCommit) { fault = Fault::None; close(); throw std::runtime_error("injected before COMMIT"); }
#endif
    execute("COMMIT");
#ifdef ZEDAS_BROKER_FAULT_TEST
    if (fault == Fault::AfterCommit) { fault = Fault::None; close(); throw std::runtime_error("injected lost COMMIT acknowledgement"); }
#endif
}
void Store::account(uint32_t owner)
{
    execute("INSERT INTO zedas_broker_accounts(owner) VALUES(" + std::to_string(owner) + ") ON DUPLICATE KEY UPDATE owner=VALUES(owner)");
}
Row Store::status(uint32_t owner)
{
    const auto rows = query("SELECT * FROM zedas_broker_accounts WHERE owner=" + std::to_string(owner));
    if (rows.size() != 1) throw std::runtime_error("missing broker owner");
    return rows[0];
}
Rows Store::batchSummary(uint32_t owner)
{
    const auto state = status(owner);
    return query("SELECT COALESCE(SUM(count),0) AS items FROM zedas_broker_escrow WHERE owner=" + std::to_string(owner) + " AND trip=" + quote(state.at("trip")));
}
Rows Store::fieldInventory(uint32_t owner)
{
    return query("SELECT * FROM zedas_broker_escrow WHERE sold=0 AND trip IS NULL AND owner=" + std::to_string(owner) + " ORDER BY claim_key");
}
Rows Store::inventory(uint32_t owner)
{
    return query("SELECT * FROM zedas_broker_escrow WHERE sold=0 AND owner=" + std::to_string(owner) + " ORDER BY claim_key");
}
bool Store::prepared(const std::string& id, uint32_t owner)
{
    auto rows = query("SELECT owner FROM zedas_broker_escrow WHERE claim_key=" + quote(id));
    if (rows.empty()) return false;
    if (number(rows[0], "owner") != owner) throw std::runtime_error("broker claim owner mismatch");
    return true;
}
void Store::prepare(const std::string& id, uint32_t owner, uint16_t item, unsigned count, uint64_t price)
{
    if (!owner || !item || !count || count > 100 || !price || price > MONEY_LIMIT / count)
        throw std::runtime_error("invalid broker claim");
    begin();
    try {
        query("SELECT owner FROM zedas_broker_accounts WHERE owner=" + std::to_string(owner) + " FOR UPDATE");
        const auto existing = query("SELECT * FROM zedas_broker_escrow WHERE claim_key=" + quote(id));
        if (!existing.empty()) {
            const auto& row = existing[0];
            if (number(row,"owner") != owner || number(row,"item") != item || number(row,"count") != count || number(row,"price") != price)
                throw std::runtime_error("broker operation identity mismatch");
        } else {
            if (fieldInventory(owner).size() >= INVENTORY_LIMIT)
                throw std::runtime_error("broker field escrow full");
            execute("INSERT INTO zedas_broker_escrow(claim_key,owner,item,count,price) VALUES(" + quote(id) + "," + std::to_string(owner) + "," + std::to_string(item) + "," + std::to_string(count) + "," + std::to_string(price) + ")");
        }
        commit("claim");
    } catch (...) { abort(); throw; }
}
void Store::depart(uint32_t owner, const std::string& trip, uint64_t now)
{
    if (trip.empty() || trip.size() > 80) throw std::runtime_error("invalid courier trip identity");
    begin();
    try {
        query("SELECT owner FROM zedas_broker_accounts WHERE owner=" + std::to_string(owner) + " FOR UPDATE");
        const auto previous = query("SELECT owner FROM zedas_broker_escrow WHERE trip=" + quote(trip) + " LIMIT 1");
        if (!previous.empty() && number(previous[0], "owner") != owner)
            throw std::runtime_error("courier trip owner mismatch");
        // A stable handoff key is never reused for a different batch, including after return.
        if (previous.empty() && number(status(owner), "state") == 0 && !fieldInventory(owner).empty()) {
            execute("UPDATE zedas_broker_escrow SET trip=" + quote(trip) + " WHERE sold=0 AND trip IS NULL AND owner=" + std::to_string(owner));
            execute("UPDATE zedas_broker_accounts SET state=1,departure_notified=0,sale_notified=0,deadline=" + std::to_string(now + TRIP_SECONDS) + ",trip=" + quote(trip) + " WHERE owner=" + std::to_string(owner));
        }
        commit("depart");
    } catch (...) { abort(); throw; }
}
void Store::settle(uint32_t owner, uint64_t now, unsigned bps)
{
    begin();
    try {
        query("SELECT owner FROM zedas_broker_accounts WHERE owner=" + std::to_string(owner) + " FOR UPDATE");
        auto state = status(owner);
        if (number(state,"state") != 1 || number(state,"deadline") > now) { commit("read"); return; }
        const auto items = query("SELECT * FROM zedas_broker_escrow WHERE sold=0 AND owner=" + std::to_string(owner) + " AND trip=" + quote(state.at("trip")));
        if (items.empty()) throw std::runtime_error("courier batch missing; settlement paused");
        uint64_t gross = 0;
        for (const auto& item : items) {
            const uint64_t price = number(item,"price"), count = number(item,"count");
            if (!count || count > 100 || !price || price > (MONEY_LIMIT - gross) / count)
                throw std::runtime_error("broker gross overflow");
            gross += price * count;
        }
        const uint64_t fee = commission(gross,bps), net = gross - fee;
        if (number(state,"pending") > MONEY_LIMIT-net || number(state,"funds") > MONEY_LIMIT-fee)
            throw std::runtime_error("broker balance full; inventory retained");
        execute("INSERT INTO zedas_broker_sales(operation,owner,gross,net,commission,bps) VALUES(" + quote(state.at("trip")) + "," + std::to_string(owner) + "," + std::to_string(gross) + "," + std::to_string(net) + "," + std::to_string(fee) + "," + std::to_string(bps) + ")");
        execute("UPDATE zedas_broker_escrow SET sold=1,sale=" + quote(state.at("trip")) + " WHERE sold=0 AND owner=" + std::to_string(owner) + " AND trip=" + quote(state.at("trip")));
        execute("UPDATE zedas_broker_accounts SET pending=pending+" + std::to_string(net) + ",funds=funds+" + std::to_string(fee) + ",state=2,deadline=" + std::to_string(now+TRIP_SECONDS) + " WHERE owner=" + std::to_string(owner));
        commit("sale");
    } catch (...) { abort(); throw; }
}
void Store::returned(uint32_t owner, uint64_t now)
{
    execute("UPDATE zedas_broker_accounts SET state=0 WHERE owner=" + std::to_string(owner) + " AND state=2 AND deadline<=" + std::to_string(now));
}
Row Store::claimNotice(uint32_t owner, bool sale)
{
    begin();
    try {
        query("SELECT owner FROM zedas_broker_accounts WHERE owner=" + std::to_string(owner) + " FOR UPDATE");
        const auto state = status(owner);
        const std::string flag = sale ? "sale_notified" : "departure_notified";
        Row result;
        if (!number(state, flag)) {
            Rows rows;
            if (sale)
                rows = query("SELECT gross,net,commission FROM zedas_broker_sales WHERE owner=" + std::to_string(owner) + " AND operation=" + quote(state.at("trip")));
            else
                rows = query("SELECT COUNT(*) AS instances,COALESCE(SUM(count),0) AS items,COALESCE(SUM(price*count),0) AS gross FROM zedas_broker_escrow WHERE owner=" + std::to_string(owner) + " AND trip=" + quote(state.at("trip")));
            if (!rows.empty() && (sale || number(rows[0], "instances"))) {
                result = rows[0];
                execute("UPDATE zedas_broker_accounts SET " + flag + "=1 WHERE owner=" + std::to_string(owner));
            }
        }
        // Claim before sending: a lost acknowledgement never causes duplicate chat.
        // A crash between this commit and network send can omit a message; balances remain queryable.
        commit("notice");
        return result;
    } catch (...) { abort(); throw; }
}
std::string Store::savedBank(uint32_t owner, uint64_t baseline, uint64_t watermark)
{
    return std::to_string(baseline) + "+(SELECT COALESCE(SUM(amount),0) FROM zedas_broker_payouts WHERE owner=" + std::to_string(owner) + " AND id>" + std::to_string(watermark) + ")";
}
Bank Store::reconcile(uint32_t owner, uint64_t baseline, uint64_t watermark)
{
    auto rows = query("SELECT COALESCE(SUM(amount),0) AS amount,COALESCE(MAX(id)," + std::to_string(watermark) + ") AS watermark FROM zedas_broker_payouts WHERE owner=" + std::to_string(owner) + " AND id>" + std::to_string(watermark));
    const uint64_t amount = number(rows.at(0),"amount");
    if (amount > MONEY_LIMIT || baseline > MONEY_LIMIT-amount) throw std::runtime_error("bank outside broker safe range");
    return {baseline+amount, number(rows[0],"watermark")};
}
Bank Store::cashout(uint32_t owner, const std::string& id, uint64_t baseline, uint64_t watermark)
{
    begin();
    try {
        auto player = query("SELECT id FROM players WHERE id=" + std::to_string(owner) + " FOR UPDATE");
        if (player.size() != 1) throw std::runtime_error("broker player does not exist");
        query("SELECT owner FROM zedas_broker_accounts WHERE owner=" + std::to_string(owner) + " FOR UPDATE");
        const auto existing = query("SELECT owner FROM zedas_broker_payouts WHERE operation=" + quote(id));
        if (!existing.empty() && number(existing[0],"owner") != owner) throw std::runtime_error("broker payout owner mismatch");
        Bank bank = reconcile(owner,baseline,watermark);
        if (existing.empty()) {
            const auto pending = number(status(owner),"pending");
            if (pending > MONEY_LIMIT-bank.balance) throw std::runtime_error("bank full; pending retained");
            if (pending) {
                execute("INSERT INTO zedas_broker_payouts(operation,owner,amount) VALUES(" + quote(id) + "," + std::to_string(owner) + "," + std::to_string(pending) + ")");
                bank.watermark = mysql_insert_id(handle);
                bank.balance += pending;
                execute("UPDATE players SET balance=" + std::to_string(bank.balance) + " WHERE id=" + std::to_string(owner));
                execute("UPDATE zedas_broker_accounts SET pending=0 WHERE owner=" + std::to_string(owner));
            }
        }
        commit("payout");
        return bank;
    } catch (...) { abort(); throw; }
}
}

#include "brokerstore.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <functional>
#include <unistd.h>
#include <sys/wait.h>

static void fails(const std::function<void()>& op) { bool failed=false; try { op(); } catch (...) { failed=true; } assert(failed); }
int main()
{
    using namespace Broker;
    assert(commission(10000,2500)==2500 && commission(1,2500)==0 && commission(7,2500)==1);
    assert(commission(MONEY_LIMIT,10000)==MONEY_LIMIT);
    fails([] { commission(MONEY_LIMIT+1,2500); });
    const char* port=std::getenv("ZEDAS_BROKER_TEST_PORT");
    if (!port) { std::cout << "SQL fixture not requested (set ZEDAS_BROKER_TEST_PORT)\n"; return 77; }
    Connection c; c.host="127.0.0.1"; c.user="root"; c.password="fixture-only"; c.database="zedas_broker_test"; c.port=std::stoul(port);
    Store db(c); db.connect();
    // Fixture-only database name and credentials are compiled in. Never use a live database.
    db.execute("DELETE FROM zedas_broker_escrow"); db.execute("DELETE FROM zedas_broker_sales");
    db.execute("DELETE FROM zedas_broker_payouts"); db.execute("DELETE FROM zedas_broker_accounts");
    db.execute("UPDATE players SET balance=100 WHERE id IN (1,2)");
    db.account(1); db.account(2);
    Store competing(c); fails([&] { competing.connect(); });
    const auto id=db.key();
    db.fault=Store::Fault::BeforeCommit; fails([&] {db.prepare(id,1,3264,1,25);});
    db.connect(); assert(!db.prepared(id,1));
    db.fault=Store::Fault::AfterCommit; fails([&] {db.prepare(id,1,3264,1,25);});
    db.connect(); assert(db.prepared(id,1));
    db.prepare(id,1,3264,1,25); assert(db.inventory(1).size()==1);
    fails([&]{db.prepare(id,1,3264,2,25);});
    fails([&]{db.prepared(id,2);});
    const auto trip=db.key(); db.depart(1,trip,100); db.settle(1,159,2500);
    assert(number(db.status(1),"pending")==0 && db.inventory(1).size()==1);
    db.fault=Store::Fault::BeforeCommit; fails([&]{db.settle(1,160,2500);});
    db.connect(); assert(db.inventory(1).size()==1 && number(db.status(1),"pending")==0);
    db.fault=Store::Fault::AfterCommit; fails([&]{db.settle(1,160,2500);});
    db.connect(); db.settle(1,160,2500);
    assert(db.inventory(1).empty() && number(db.status(1),"pending")==19 && number(db.status(1),"funds")==6);
    assert(db.query("SELECT * FROM zedas_broker_sales").size()==1);
    db.returned(1,219); assert(number(db.status(1),"state")==2);
    db.returned(1,220); assert(number(db.status(1),"state")==0);
    const auto payout=db.key();
    db.fault=Store::Fault::BeforeCommit; fails([&]{db.cashout(1,payout,100,0);});
    db.connect(); assert(number(db.status(1),"pending")==19);
    db.fault=Store::Fault::AfterCommit; fails([&]{db.cashout(1,payout,100,0);});
    db.connect(); assert(number(db.status(1),"pending")==0);
    // The same expression used in IOLoginData protects stale live memory and
    // an ordinary 10gp withdrawal after a committed-but-unacknowledged payout.
    for (int i=0;i<2;++i) db.execute("UPDATE players SET balance="+Store::savedBank(1,90,0)+" WHERE id=1");
    assert(number(db.query("SELECT balance FROM players WHERE id=1")[0],"balance")==109);
    const auto recovered=db.cashout(1,payout,90,0);
    assert(recovered.balance==109 && recovered.watermark>0);
    const auto repeated=db.cashout(1,db.key(),recovered.balance,recovered.watermark);
    assert(repeated.balance==109 && repeated.watermark==recovered.watermark);
    fails([&]{db.cashout(2,payout,100,0);});
    assert(db.query("SELECT * FROM zedas_broker_payouts").size()==1);
    // Crash/restart: a child exits with durable escrow; a new process/connection recovers once.
    db.close();
    const auto child=fork(); assert(child>=0);
    if (child==0) { Store restarted(c); restarted.connect(); restarted.prepare(restarted.key(),2,3357,1,400); _exit(0); }
    int status; waitpid(child,&status,0); assert(WIFEXITED(status)&&WEXITSTATUS(status)==0);
    db.connect(); assert(db.inventory(2).size()==1);
    for (unsigned i=1;i<INVENTORY_LIMIT;++i) db.prepare(db.key(),2,3355,1,4);
    fails([&]{db.prepare(db.key(),2,3355,1,4);}); assert(db.inventory(2).size()==INVENTORY_LIMIT);
    // Overflow retains all unsold images and does not produce a sale ledger.
    db.execute("UPDATE zedas_broker_accounts SET pending="+std::to_string(MONEY_LIMIT)+" WHERE owner=2");
    db.depart(2,db.key(),300); fails([&]{db.settle(2,360,2500);});
    assert(db.inventory(2).size()==INVENTORY_LIMIT);
    fails([&]{db.cashout(2,db.key(),MONEY_LIMIT,0);});
    assert(number(db.status(2),"pending")==MONEY_LIMIT);
    std::cout << "Broker InnoDB escrow, lost acknowledgements, process restart, conservation, bank save and replay passed\n";
}

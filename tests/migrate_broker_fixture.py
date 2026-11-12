#!/usr/bin/env python3
"""Exercise the additive v1->v2 migration on the explicitly requested disposable DB.

The caller imports nostalrius.sql into a fresh zedas_broker_test database first.
An optional client command after -- supports Docker's mariadb client locally.
Never point this fixture at a live database; credentials/name are fixed here.
"""
import os
from pathlib import Path
import subprocess
import sys

port = os.environ.get("ZEDAS_BROKER_TEST_PORT")
if not port:
    raise SystemExit("ZEDAS_BROKER_TEST_PORT is required for the disposable migration fixture")
client = sys.argv[1:]
if client[:1] == ["--"]:
    client = client[1:]
client = (client or ["mysql"]) + ["-h127.0.0.1", "-uroot", "-P" + str(int(port)),
                                  "--batch", "--skip-column-names", "zedas_broker_test"]
env = dict(os.environ, MYSQL_PWD="fixture-only")
root = Path(__file__).resolve().parents[1]


def sql(text):
    return subprocess.run(client, input=text, text=True, capture_output=True,
                          check=True, env=env).stdout.strip()


sql((root / "migrations/001_broker.sql").read_text())
sql("""
INSERT INTO zedas_broker_accounts(owner,pending,funds,state,deadline,trip) VALUES
 (1,0,0,1,4102444800,'legacy-outbound'),(2,3,1,2,4102444860,'legacy-returning');
INSERT INTO zedas_broker_sales(operation,owner,gross,net,commission,bps)
 VALUES('legacy-returning',2,4,3,1,2500);
INSERT INTO zedas_broker_escrow(claim_key,owner,item,count,price,sold,sale) VALUES
 ('legacy-claim-1',1,3354,1,30,0,NULL),('legacy-claim-2',2,3355,1,4,1,'legacy-returning');
""")
sql((root / "migrations/002_broker_courier.sql").read_text())
assert sql("SELECT version FROM zedas_broker_schema WHERE id=1") == "2"
assert sql("SELECT owner,pending,funds,state,deadline,trip,departure_notified,sale_notified FROM zedas_broker_accounts ORDER BY owner") == (
    "1\t0\t0\t1\t4102444800\tlegacy-outbound\t1\t1\n"
    "2\t3\t1\t2\t4102444860\tlegacy-returning\t1\t1")
assert sql("SELECT claim_key,item,count,price,sold,COALESCE(sale,'NULL'),trip FROM zedas_broker_escrow ORDER BY claim_key") == (
    "legacy-claim-1\t3354\t1\t30\t0\tNULL\tlegacy-outbound\n"
    "legacy-claim-2\t3355\t1\t4\t1\tlegacy-returning\tlegacy-returning")
assert sql("SELECT operation,gross,net,commission FROM zedas_broker_sales") == "legacy-returning\t4\t3\t1"
sql("DELETE FROM zedas_broker_escrow; DELETE FROM zedas_broker_sales; DELETE FROM zedas_broker_payouts; DELETE FROM zedas_broker_accounts;")
print("Legacy outbound/returning migration preserves items, batch ownership, deadlines, money and muted historical notices")

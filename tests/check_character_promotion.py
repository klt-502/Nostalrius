#!/usr/bin/env python3
"""Exercise the operator SQL in the existing explicitly disposable fixture only."""
import os
from pathlib import Path
import subprocess
import sys

port = os.environ.get("ZEDAS_BROKER_TEST_PORT")
if not port:
    raise SystemExit(77)
client = sys.argv[1:]
if client[:1] == ["--"]:
    client = client[1:]
client = (client or ["mysql"]) + ["-h127.0.0.1", "-uroot", "-P" + str(int(port)),
                                  "--batch", "--binary-as-hex", "--skip-column-names", "zedas_broker_test"]
env = dict(os.environ, MYSQL_PWD="fixture-only")
root = Path(__file__).resolve().parents[1]
template = (root / "tools/admin/promote_zedas_characters.sql").read_text()


def sql(text):
    return subprocess.run(client, input=text, text=True, capture_output=True,
                          check=True, env=env).stdout.strip()


def reset():
    sql("DELETE FROM players_online; DELETE FROM players WHERE id IN (501,502,503);")
    sql("INSERT INTO players(id,name,account_id,vocation,conditions,balance) VALUES "
        "(501,'Kyle Behe Hunter',1234567,3,'',123),"
        "(502,'Kyle Zedas',1234567,4,'',456),"
        "(503,'Unrelated promotion fixture',1234567,3,'',789);")
    sql("INSERT INTO player_storage(player_id,`key`,`value`) VALUES(501,45000,99),(503,30018,0);")


def snapshot():
    return (sql("SELECT * FROM players ORDER BY id"),
            sql("SELECT * FROM player_storage ORDER BY player_id,`key`"),
            sql("SELECT id,type,premdays,lastday FROM accounts ORDER BY id"))


def configured(**changes):
    values = dict(zedas_paladin_id=501, zedas_paladin_account=1234567,
                  zedas_knight_id=502, zedas_knight_account=1234567,
                  zedas_server_stopped=1, zedas_premium_verified=1)
    values.update(changes)
    text = template
    for name, value in values.items():
        default = "0" if name in {"zedas_server_stopped", "zedas_premium_verified"} else "NULL"
        text = text.replace(f"SET @{name} = {default};", f"SET @{name} = {value};")
    return text


try:
    reset()
    before = snapshot()
    sql(template)
    assert snapshot() == before, "unconfigured template must change nothing"
    sql(configured())
    assert snapshot() == before, "configured dry run must roll back all changes"
    for options in ({"zedas_paladin_id": 999999}, {"zedas_paladin_account": 999999},
                    {"zedas_paladin_id": 502}, {"zedas_server_stopped": 0},
                    {"zedas_premium_verified": 0}):
        sql(configured(**options).replace("ROLLBACK; -- Dry run", "COMMIT; -- Dry run"))
        assert snapshot() == before, "identity/operator guard must block entire update"
    for mutation in ("UPDATE players SET vocation=4 WHERE id=501",
                     "UPDATE players SET name='Different character' WHERE id=501",
                     "UPDATE players SET deleted=1 WHERE id=501",
                     "UPDATE players SET deletion=1 WHERE id=501",
                     "INSERT INTO players_online(player_id) VALUES(501)"):
        reset(); sql(mutation); unchanged = snapshot()
        sql(configured().replace("ROLLBACK; -- Dry run", "COMMIT; -- Dry run"))
        assert snapshot() == unchanged, "mismatched/deleted/online targets must block both updates"
    reset(); before = snapshot()
    sql(configured().replace("ROLLBACK; -- Dry run", "COMMIT; -- Dry run"))
    assert sql("SELECT id,vocation FROM players WHERE id IN(501,502,503) ORDER BY id") == "501\t7\n502\t8\n503\t3"
    assert sql("SELECT player_id,`value` FROM player_storage WHERE `key`=30018 ORDER BY player_id") == "501\t1\n502\t1\n503\t0"
    after = snapshot()
    columns = sql("SHOW COLUMNS FROM players").splitlines()
    vocation_column = [r.split('\t')[0] for r in columns].index("vocation")
    expected = []
    for line in before[0].splitlines():
        cells = line.split('\t')
        if cells[0] in {"501", "502"}: cells[vocation_column] = {"501":"7", "502":"8"}[cells[0]]
        expected.append('\t'.join(cells))
    assert after[0] == '\n'.join(expected), "all other player fields must remain unchanged"
    assert before[2] == after[2], "account type/premium data must remain unchanged"
    sql(configured().replace("ROLLBACK; -- Dry run", "COMMIT; -- Dry run"))
    assert snapshot() == after, "promotion/flag update must be idempotent"
    print("Promotion SQL: dry-run rollback, identities, online/deletion/vocation guards, exact updates, unrelated fields and replay validated.")
finally:
    sql("DELETE FROM players_online; DELETE FROM players WHERE id IN (501,502,503);")

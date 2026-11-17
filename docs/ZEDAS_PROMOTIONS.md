# Promotion of the two intended Zedas characters

Requested mapping, using the user's listed order:

| Exact character name | Expected base | Promoted target |
| --- | --- | --- |
| Kyle Behe Hunter | Paladin (3) | Royal Paladin (7) |
| Kyle Zedas | Knight (4) | Elite Knight (8) |

**No live character/account data has been changed.** This checkout has no live
`config.lua` or configured live database connection. Its tracked `nostalrius.sql`
contains only GM Nostalrius, GM Nostalrius 2 and Mage; it cannot establish the Kyle
characters' IDs, owners, vocations or premium state. Do not apply the mapping if
live inspection shows a different intended base vocation; review the mapping first.
No startup migration or automatic character-name matching was added.

## Audited persistence and login behavior

- `data/XML/vocations.xml`: 7 derives from 3; 8 derives from 4. These are authentic
  promoted vocations with their existing regeneration/soul/death-loss behavior.
- `players.vocation` is loaded through `Player::setVocation` in `IOLoginData` and
  saved from `getVocationId()`. Changing only this field does not set promotion history.
- Native NPC `Promote` in `BehaviourDatabase` sets vocation to base + 4 **and**
  storage `30018 = 1`. The NPC `Promoted` condition checks that storage value.
- `data/creaturescripts/scripts/login.lua` promotes base vocations with that flag
  only while premium; non-premium characters with IDs 5–8 are demoted at login.
  The flag remains so promotion can be restored on a later premium login.
- `Player::isPremium()` accepts `freePremium`, the group's IsAlwaysPremium flag,
  or positive effective premium days. `Game::updatePremium` ages `accounts.premdays`
  against `lastday`, so a stale positive database value is not sufficient proof.
  This pass changes no premium days, global premium switch, group flags or login rules.

Tracked schema: `players` and `player_storage` use InnoDB. `players` has primary
key `id`, unique `name`, and `account_id`; `player_storage` has composite primary
key `(player_id,key)` and a cascading FK to `players.id`. `players_online` is a
MEMORY table keyed by `player_id`. Verify these facts against the live deployment.

## Inspect current live data first (read only)

Connect with your existing administrative DB client to the actual Zedas schema;
do not import `nostalrius.sql` into it. Run:

```sql
SELECT DATABASE();
SHOW CREATE TABLE players;
SHOW CREATE TABLE player_storage;
SHOW CREATE TABLE players_online;
SELECT p.id, p.name, p.account_id, p.group_id, p.level, p.vocation,
       p.deleted, p.deletion, a.premdays, a.lastday,
       s.`value` AS promotion_storage,
       EXISTS(SELECT 1 FROM players_online o WHERE o.player_id=p.id) AS online
FROM players p JOIN accounts a ON a.id=p.account_id
LEFT JOIN player_storage s ON s.player_id=p.id AND s.`key`=30018
WHERE BINARY p.name=BINARY 'Kyle Behe Hunter'
   OR BINARY p.name=BINARY 'Kyle Zedas'
ORDER BY p.id;
```

Expect exactly two intended, undeleted identities with their real account IDs.
Record the current vocation and whether the promotion storage is absent, 0 or 1.
Verify effective premium in the running server before stopping it. Never change
account ownership or add GM privileges simply to perform this promotion.

## Existing in-game methods (no new commands)

The ordinary promotion is available from King Tibianus at his native home
`32311,32171,6`: **`hail king` → `promotion` → `yes`**. His actual NPC definition
requires level 20, premium and 20000 carried GP; it sets vocation and promotion
storage together. Use this only on the verified base vocation with a consistent
current promotion flag. It retains the historical price and eligibility rules.

If an intended character **already has an access group**, the existing self-only
GM command is:

```
/storagevalue 30018,1
```

Run it while logged in **as that specific verified Paladin/Knight**, then log out
normally (saving storage) and log back in while premium. The native login script
then sets 3 → 7 or 4 → 8. Log out normally again after verifying the result to
save the promoted vocation. This command does not target a name and does not itself
change vocation immediately. Do not pass a character name to it. Already promoted
7/8 characters are not promoted again by the login script. Ordinary players cannot
use it; use the NPC or guarded offline SQL instead. It bypasses NPC cost/level
requirements as a deliberate existing GM operation, not a global rules change.

## Guarded offline SQL alternative

[tools/admin/promote_zedas_characters.sql](../tools/admin/promote_zedas_characters.sql)
is an operator-run template, **not a migration**. It defaults to NULL identity
values, unverified operator assertions and final **ROLLBACK**.

1. Inspect the live schema/data above; back up the DB and record old promotion
   state. Stop the server so its in-memory player saves cannot overwrite the update.
2. In a local copy of the SQL file, fill the four ID/account variables from the
   preview. Set `@zedas_server_stopped=1` only after stopping the server, and
   `@zedas_premium_verified=1` only after verifying effective premium for both.
3. Run that copy in your existing DB client without `--force`. It locks the two
   player rows, checks InnoDB/primary keys, exact names/IDs/owners, base or already
   desired vocations, undeleted status and an empty online table. Any failed guard
   blocks **both** player and storage writes. An empty online table alone is not
   proof that the server is stopped. Stale rows require investigation, not blind deletion.
4. The final preview must show `guards_passed=1`, the two expected identities,
   vocations 7/8, and promotion storage 1. The default final ROLLBACK leaves the
   live DB unchanged even on success. Re-read the persisted rows to confirm that.
5. After reviewing the dry run, replace **only the final `ROLLBACK` statement**
   with `COMMIT` in the operator copy and rerun. Re-read the preview query before
   starting the server, then relog each character and verify vocation/storage.

The committed update affects only those two `vocation` fields and their key
30018 rows. It is idempotent for already-correct 7/8 vocations, preserves other
storage and all other character fields, and changes no account, group, skills,
XP, balance, inventory, health or mana. No database schema migration is needed.
If rollback after a committed change is needed, stop the server and restore only
these fields/key rows to their **recorded original state**; do not blindly remove
promotion history or reset both characters to base vocations.

The disposable MariaDB test exercises dry-run rollback, missing/wrong IDs,
wrong ownership/name/vocation, deletion/online blocks, operator assertions,
exact updates, unrelated player/account fields and idempotent replay. It creates
synthetic Kyle-named rows only in `zedas_broker_test`; those are not live evidence.

## Saved food regeneration after offline SQL

Prefer the NPC or the existing GM/login method for a **base-vocation** character:
`Player::setVocation` refreshes the active regeneration condition. During login,
stored conditions are attached in `Player::onCreatureAppear` before Lua onLogin
runs, so the 3/4 → 7/8 promotion in that script also refreshes restored food rates.

The offline SQL deliberately preserves the `conditions` blob. `IOLoginData` sets
the saved vocation before those stored conditions are attached, and onLogin does
not call setVocation again for an already-promoted 7/8 character. An existing food
condition saved at base-vocation rates can therefore keep those rates until it
expires. After offline SQL, verify the promoted vocation, let the old food condition
expire without feeding, then eat to create a new condition using the promoted
vocation. Simply eating while the old condition is active extends its time;
`Player.feed` does not refresh its rates. Do not erase or edit the serialized blob:
it can include unrelated conditions. This existing persistence behavior is not
changed by the polish pass and does not affect the requested stored vocation IDs.

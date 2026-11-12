# Original Tibia 7.72 broker retest

Use a backed-up disposable Easy Mode world, with migrations 001 then 002 applied
while stopped. Preserve all existing live ledgers. Enable master, spill and broker;
keep `allowClones=false`. The tests below use the accepted catalog and commission
2500 bps. Automated tests cannot establish how captions/status text feel in the
original client's console, viewport or movement presentation.

1. Hire once, then repeatedly. There is one Loot Broker, no player summon slot,
   combat, blocking body or duplicate actor. Repeated hire reports `Carrying N/20`
   and pending/travel status. Hunt normally: the collector uses native walking,
   approaches within one tile and takes approved actual generated fodder only.
2. Pick up several nearby hatchets/brass helmets quickly. `Got a hatchet.` floats
   like monster speech for every successful pickup, including several within three
   seconds. It never enters the console. Currency still goes through Gold
   Bag/auto-gold. Gems/ammo/quest/marked loot and worms keep their accepted behavior.
3. Leave one generated brass helmet untouched after spill. Leave visibility for
   more than five minutes (before its source corpse changes/decays), then return
   and resume hiring if the leash paused it. It remains collectable. Repeat after
   many intervening kills. Pick another item up and drop it back: it must never
   regain eligibility. Corpse move/removal/decay must still invalidate the source.
4. One cheap hatchet must cause no automatic dispatch even after two idle minutes.
   At 10+ instances, 250+ gp and 120 idle seconds, a runner can dispatch; at 20
   field instances it dispatches on the next tick. `!broker sell` dispatches any
   nonempty smaller batch. An active trip answers repeated sell without another
   trip. Block every safe runner placement: the batch must remain with collector.
5. At handoff, a second original dwarf named Broker Runner appears beside the
   collector: `Pickup!`, then after about two seconds `Off to town!` and disappears.
   The collector stays present and follows/collects throughout the 60-second trip.
   The runner never picks ground loot. No mount/new sprite or second summon slot.
6. The console shows exactly one departure summary with actual item count and
   estimated catalog gross. Collect new loot while the runner is gone: status
   distinguishes field slots from the runner batch, and that new loot must not
   enter the earlier sale. At field capacity while runner busy, overflow stays on
   ground. The runner completes the return off-scene after a further 60 seconds.
7. On committed sale, see exactly one gross/share/fund console summary. One leather
   helmet: 4/3/1; one brass helmet: 30/23/7; one sword: 25/19/6; two swords together:
   50/38/12. Repeated status/hire/ticks must not repeat departure or sale summaries.
8. Cashout twice: one bank credit, no physical coins. Save/relog/restart and ordinary
   bank withdrawals must preserve bank and pending/fund conservation. Local balance/
   cashout speech works only with your own nearby collector, not the runner.
9. Restart during handoff, outbound, committed sale and return. There must be one
   assigned batch, one sale, correct balances, restored field escrow and no second
   runner. The collector can resume while the old trip finishes off-scene. Already
   announced summaries must not repeat. A crash between durable notification claim
   and socket send can omit a summary; status/ledger remain authoritative.
10. Interrupt fixture DB connectivity at prepare/handoff/sale/payout. Unknown ground
    claims quarantine their exact source; recovery must not create a second item.
    Failed/uncommitted sales produce no completion summary. Never delete ledgers
    or manually unlock uncertain sources as a recovery shortcut.
11. Test blocked routes, removed loot mid-approach, house tiles, player insertion,
    protected state/handler changes and two competing owners. Existing provenance,
    tile visibility budget, capacity and first completed claim protections remain.
12. Change floor: immediate disappearance, then safe adjacent collector rejoin
    after roughly two seconds. Rapid floor changes reset the wait; blocked placement
    must wait, and hire during the delay must report status without bypassing it.
    Test while a runner is in handoff/travel: the assigned trip continues off-scene
    and the collector resumes on the new floor. Verify no absent/cross-floor pickup.
    Also test far same-floor travel,
    logout, death and dismiss must stop visible collection and preserve all durable
    state. No full stair/rope/hole pathfinding or teleport-to-loot is provided.
13. Disable broker, then master, and restart. Historical bank, loot, consumables,
    target behavior and other accepted Easy Mode features remain unchanged. No
    broker DB migration is required when disabled. Retain tables for later recovery.

No simulated purchases, market or broker-fund spending is included.

Food/ring policy retest additions:

- Generate meat 3577, ham 3582, bread 3600, cheese 3607 and other reviewed food.
  All stay inside ordinary generated bags; the collector uses no slots for them.
  Retrieve and eat manually to verify unchanged historical consumption.
- Generate wedding/crystal/gold/sky rings: safe passive instances spill but remain
  player-controlled. Charged, active/dormant transforming and special rings stay
  protected; none is collected or sold.
- Generate three adjacent equipment drops: each successful pickup emits its own
  floating caption request within the old three-second interval. Check native
  client overlap/replacement/lifetime; no pickup text should enter the console.
- A failed approach, marked item, manual pickup winning a race or failed prepare
  must produce no successful-pickup caption.
- Recheck collector coverage, runner dispatch/summaries, floor rejoin and restart
  recovery unchanged. Before upgrading, drain old food escrow as described in
  `BROKER_PICKUP_POLICY.md`; do not delete or rewrite ledger rows.

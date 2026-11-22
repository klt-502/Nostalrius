# Original 7.72 XP-bar tooltip: audit only

No tooltip, tracker, client patch, rate change or protocol extension is implemented
in this round.

`ProtocolGame::AddPlayerStats` (`src/protocolgame.cpp`) sends packet `0xA0`:
health, maximum health, capacity, uint32 experience, level, level percentage,
mana, maximum mana, magic level/percentage and soul. There is no arbitrary XP-bar
hover-text field. `sendTextMessage` sends ordinary messages; it cannot attach a
new tooltip to the original client's experience bar. Adding unknown packet fields
would desynchronize an unchanged original client.

`Tibia.dat` describes world things (item/creature/effect/missile flags and sprite
references), not programmable stats-bar widgets. `.spr`/`.pic` graphics changes
cannot implement a dynamic rolling-rate calculation or a new hover event. This
repository contains no original-client UI source or supported UI scripting
extension. The original executable is not modified or reverse engineered here.
Original-client acceptance should also record whatever hover information it
already displays; that native behavior is not an extensible server API.

| Approach | What it can provide | Original executable changed? |
| --- | --- | --- |
| Server command / private status message | XP, remaining XP, hunting rate and ETA in existing chat | No; cannot become a bar-hover tooltip |
| Read-only external local dashboard | Separate display, with an explicitly designed read-only data feed | No; not an in-client tooltip |
| Compatible open-source client UI | Real bar-hover tooltip; can use existing `0xA0` for total XP/level | Separate client, no patch to the original |
| Original-client hook/binary change | Potentially a genuine new tooltip | Yes; separate future client-modification phase |
| Asset-only edit or protocol-only addition | Cannot add the requested dynamic original-client UI behavior | Not a sound solution |

The least invasive next step that retains the original client is a **separately
reviewed server-side `!xp` report** using its normal message channel. It requires
no protocol change or new endpoint. If a literal bar-hover tooltip is essential,
prefer a compatible open-source client's UI in a separate client phase. A client
can infer positive XP deltas from `0xA0`, but those deltas cannot reliably distinguish
hunting rewards from quests or GM grants. The wire field also becomes zero in this
server when XP exceeds uint32; client-only deltas are not authoritative there.

For a later authoritative hunting tracker, inspect `Player::onGainExperience`,
`gainExperience`, `onGainSharedExperience`, `addExperience`, `removeExperience`
and death handling in `src/player.cpp`. Track **actual credited positive hunting
XP after native adjustments**, including the native party-share path. Avoid counting
both the originating kill and the resulting party share. Quest/admin grants should
be classified separately. Retain timestamped gains in a bounded 15-minute deque;
expire entries with a monotonic clock. Do not subtract death/other XP losses from
hunting gains; report losses separately and use current actual XP for remaining XP.

The eventual displayed rolling rate should use a 900-second window (including
idle time within it), not total login duration. Choose and document a warm-up rule:
for example, use elapsed session tracking time until 15 minutes have accumulated,
showing the shorter sample duration explicitly. Empty windows report zero and ETA
unavailable, not infinity. With rate `r > 0`, ETA is remaining XP / r. Remaining XP
is `max(0, Player::getExpForLevel(level+1) - actualExperience)` using the existing
native formula in `src/player.h`; preserve its overflow guards. Session-only
sampling avoids a new DB schema. None of this tracker is implemented by this note.

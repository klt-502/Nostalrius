# Auto-target highlighting investigation

Inspected baseline: `ed3c7c671711f82e97c1fadd619ffb7b4929bebd`
(`zedas-easy-mode`, accepted Wave 1). The user reports that automatic attacks
work in the original Tibia 7.72 client, but do not select the target visually.

Decision: leave targeting and outgoing packets unchanged. No existing packet
in this server has been established to set the original client's normal attack
selection, red target square, and battle-list selection together. The existing
creature-square packet is a separate visual effect; using it as an attack
selection notification would not establish that client state. No executable
change, new opcode, configuration option, or Action ID is introduced.

## Source evidence

The following line numbers refer to the baseline above; function names are the
stable anchors.

| Direction / operation | Source | Meaning |
| --- | --- | --- |
| Client to server `0xA1`, then a 32-bit creature ID | `src/protocolgame.cpp:413`, `ProtocolGame::parseAttack` at line 833 | Player requests a target; zero means cancellation. This is an incoming packet, not a server notification. |
| Client to server `0xBE` | `src/protocolgame.cpp:423`, `Game::playerCancelAttackAndFollow` in `src/game.cpp:2724` | Cancel attack/follow and stop walking. The handler routes through a zero attack ID, suspending automatic reacquisition. |
| Manual target handling | `Game::playerSetAttackedCreature`, `src/game.cpp:2736` | Records manual input, validates the creature and `Combat::canTargetCreature`, calls `Player::setAttackedCreature`, and schedules normal walking. Invalid/cancelled requests send target cancellation; successful requests do not send a positive target-selection response. |
| Server target state | `Player::setAttackedCreature`, `src/player.cpp:2777`; `Creature::setAttackedCreature`, `src/creature.cpp:843` | Stores the attacked creature, applies normal visibility/floor constraints, chase behavior, attack scheduling, and summon propagation. Setting a target does not serialize a client selection notification. |
| Automatic target state | `EasyMode::updateAutoTarget`, `src/easymodetarget.cpp:60` | Calls the same `Player::setAttackedCreature` and records the automatic creature ID. There is no outgoing positive target-selection packet. Manual input remains authoritative. |
| Server to client `0xA3`, no payload | `ProtocolGame::sendCancelTarget`, `src/protocolgame.cpp:1299` | Clears the client's target. It contains no creature ID with which to select another target. Incoming `0xA3` is separately a party invitation; opcode meaning depends on direction. |
| Server to client `0x86`, then a 32-bit creature ID and one palette-color byte | `ProtocolGame::sendCreatureSquare`, `src/protocolgame.cpp:1030` | Sends a visible-creature square effect. The current caller is `Player::blockHit`, `src/player.cpp:1487`, which sends `SQ_COLOR_BLACK` for an attacker. This is not exclusively attack-target feedback. |

`Player::onAttackedCreatureDisappear` (`src/player.cpp:844`) also sends target
cancellation. `EasyMode::setAutoTarget` and `EasyMode::updateAutoTarget` send it
when clearing an automatic target. These paths support clearing client state,
not choosing a client target.

## Compatible open-client corroboration

The original Tibia 7.72 executable is closed source and was not inspected or
modified here. The following is corroborating evidence from OTClient, not proof
of every behavior of the original executable. All links are pinned to OTClient
commit `dab86e610991092fe8b2471f7de55a7c986b3cd2`; its source supports multiple
protocol versions, so later-version fields must not be copied into 7.72 packets.

- [Protocol opcode definitions](https://github.com/edubart/otclient/blob/dab86e610991092fe8b2471f7de55a7c986b3cd2/src/client/protocolcodes.h#L98)
  identify decimal 134 (`0x86`) as `GameServerMarkCreature` and decimal 163
  (`0xA3`) as `GameServerClearTarget`.
- [ProtocolGame::parseCreatureMark](https://github.com/edubart/otclient/blob/dab86e610991092fe8b2471f7de55a7c986b3cd2/src/client/protocolgameparse.cpp#L1126)
  reads the ID and color, then calls `Creature::addTimedSquare`; it does not set
  the attacking creature.
- [Creature::addTimedSquare](https://github.com/edubart/otclient/blob/dab86e610991092fe8b2471f7de55a7c986b3cd2/src/client/creature.cpp#L814)
  schedules removal of the square. Repeatedly sending a colored square would
  therefore provide a timed visual cue with its own lifetime, not synchronized
  target selection or a verified way to clear that selection immediately.
- [Game::attack](https://github.com/edubart/otclient/blob/dab86e610991092fe8b2471f7de55a7c986b3cd2/src/client/game.cpp#L936)
  changes the client's attacked creature locally **before** sending the attack
  request to the server.
- [Game::setAttackingCreature](https://github.com/edubart/otclient/blob/dab86e610991092fe8b2471f7de55a7c986b3cd2/src/client/game.cpp#L1695)
  emits the client attack-state change event;
  [the battle-list handler](https://github.com/edubart/otclient/blob/dab86e610991092fe8b2471f7de55a7c986b3cd2/modules/game_battle/battle.lua#L519)
  uses that event to mark a target. The creature-mark parser emits no such event.
- [ProtocolGame::parsePlayerCancelAttack](https://github.com/edubart/otclient/blob/dab86e610991092fe8b2471f7de55a7c986b3cd2/src/client/protocolgameparse.cpp#L1467)
  routes server cancellation to `Game::processAttackCancel`. Its optional
  sequence field belongs to other protocol feature sets; this server correctly
  sends only the 7.72 opcode.

Together with the accepted gameplay observation, this explains why the server
can attack an automatically chosen monster while the client has no locally
selected target. Sending the incoming attack opcode in the opposite direction
would not solve this: the two directions have different packet grammars.

## Actual-client validation and limits

Source inspection does not validate the original executable's renderer or its
battle-list state. No original-client test or packet capture was performed in
this investigation. There is no packet/code change to build or gameplay-test.

For the original Tibia 7.72 client, retain this regression checklist:

1. Manually select a monster. Record the red square and selected battle-list
   row, then verify normal attack and command behavior.
2. Clear the target, explicitly enable auto-targeting, and let it choose a
   monster. Verify attacks and `!explo`/`!gfb` use the server target; record the
   expected current limitation that the client may show no normal selection.
3. Manually select a different monster while automation is active. Verify it
   immediately takes priority; a stronger monster arriving must not replace it.
4. Press Escape/cancel. Verify automatic attacks and reacquisition remain
   suspended until automation is explicitly resumed; no misleading square
   should remain from a newly introduced effect (none is sent by this change).
5. Exercise target death, disappearance, floor changes, bounded unreachability,
   automation off/on, and logout/login. Verify no stale server target survives
   and session-only state resets as documented in the Wave 1 checklist.
6. If further protocol research is desired, capture decoded traffic in a
   controlled local session and compare manual selection, automatic selection,
   cancellation, and receiving an attack. Keep packet direction explicit. This
   can confirm the original client's existing behavior without changing it;
   it does not authorize a custom opcode or executable modification.

A future visual cue would need a separate, explicit product decision and
actual-client acceptance. A timed colored square alone must not be described
as fixing normal attack or battle-list selection.

# Field pickup captions and item policy

## Native speech experiment

The collector no longer has `nextPickupSpeech` or the three-second suppression
check. Each normal successfully completed field claim attempts one
`Game::internalCreatureSay(..., TALKTYPE_MONSTER_SAY, ...)` after the real item has
entered private escrow. Failed approaches, invalid/lost claims and uncertain
unacknowledged claims do not produce a success caption. Startup hydration is
ledger recovery, not a new field pickup, and does not fabricate captions.
Routine pickups have no persistent chat/status copy; significant courier/sale
messages retain their accepted console-visible channel and durable receipts.

Source audit: `Game::internalCreatureSay` selects same-floor viewport spectators,
sends through `Player::sendCreatureSay`, `ProtocolGame::sendCreatureSay` and
`AddCreatureSpeak`, and invokes the normal creature speech hooks. Every send
appends a separate 0xAA positional speech event with a statement ID to the output
buffer. This path has no speech-rate timer, same-speaker text replacement or
caption-count cap. Incoming player-chat mute/flood controls are a separate path;
no such controls or native output behavior were changed. Normal socket buffering
and network/disconnect limits remain in place.

The collector still completes at most one ordinary claim per one-second decision
tick, with native approach/path delays. This change does not accelerate pickup.
The proprietary original 7.72 client's display lifetime, overlap, replacement and
maximum simultaneously visible captions cannot be established from this server
source. No executable inspection/modification or custom UI is introduced: test
three adjacent pickups over approximately two seconds and observe native rendering.
Wire tests verify one monster-speech packet per pickup, including the second and
third pickup inside the old suppression interval, and no persistent pickup text.

Caption validation: full normal suite 33/33 passed, including rapid actual speech
packets, failed preparation, invalid items and a manual pickup winning the race.

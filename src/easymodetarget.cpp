#include "otpch.h"
#include "easymode.h"
#include "combat.h"
#include "configmanager.h"
#include "game.h"
#include "monster.h"
#include "player.h"

extern Game g_game;
extern ConfigManager g_config;

namespace {
bool enabled()
{
	return g_config.getBoolean(ConfigManager::EASY_MODE_ENABLED) &&
	       g_config.getBoolean(ConfigManager::EASY_MODE_AUTO_TARGET);
}

bool eligible(Player& player, Creature& creature)
{
	Monster* monster = creature.getMonster();
	return monster && !creature.isRemoved() && creature.getHealth() > 0 && monster->isHostile() &&
	       creature.isAttackable() && !EasyMode::isPlayerControlled(creature) &&
	       creature.getPosition().z == player.getPosition().z &&
	       Position::areInRange<7, 5, 0>(player.getPosition(), creature.getPosition()) &&
	       player.canSee(creature.getPosition()) && player.canSeeCreature(&creature) &&
	       Combat::canTargetCreature(&player, &creature) == RETURNVALUE_NOERROR;
}

bool reachable(Player& player, Creature& target)
{
	const Item* weapon = player.getWeapon();
	const int range = weapon ? std::max<int>(1, weapon->getShootRange()) : 1;
	if (g_game.canThrowObjectTo(player.getPosition(), target.getPosition(), true, range, range)) {
		return true;
	}
	std::forward_list<Direction> path;
	return player.getPathTo(target.getPosition(), path, 1, 1, true, true, 12);
}
}

bool EasyMode::setAutoTarget(Player& player, bool active)
{
	if (!enabled()) {
		player.sendCancelMessage("Easy Mode auto-targeting is disabled.");
		return false;
	}
	AutoTargetState& state = player.getEasyModeAutoTargetState();
	if (active && state.active) return true; // Repeated "on" must retain ownership and failure time.
	Creature* target = player.getAttackedCreature();
	if (!active && target && target->getID() == state.automaticTargetId) {
		player.setAttackedCreature(nullptr);
		player.sendCancelTarget();
	}
	state.manualInput(false);
	state.active = active;
	if (!active) state.unreachableUntil.clear();
	return true;
}

void EasyMode::updateAutoTarget(Player& player, int64_t now)
{
	AutoTargetState& state = player.getEasyModeAutoTargetState();
	if (!enabled() || !state.active || player.isRemoved() || player.getHealth() <= 0) return;
	Creature* target = player.getAttackedCreature();
	if (target) {
		if (target->getID() != state.automaticTargetId) {
			state.manualInput(false); // A manual/script target is authoritative.
			return;
		}
		if (eligible(player, *target)) {
			if (reachable(player, *target)) {
				state.unreachableSince = 0;
				return;
			}
			if (state.unreachableSince == 0) state.unreachableSince = now;
			if (now - state.unreachableSince < AUTO_TARGET_UNREACHABLE_MS) return;
			if (state.unreachableUntil.size() >= MAX_SUPPRESSED_TARGETS) {
				auto oldest = std::min_element(state.unreachableUntil.begin(), state.unreachableUntil.end(),
					[](const std::pair<const uint32_t, int64_t>& a, const std::pair<const uint32_t, int64_t>& b) {
						return a.second < b.second;
					});
				state.unreachableUntil.erase(oldest);
			}
			state.unreachableUntil[target->getID()] = now + AUTO_TARGET_RETRY_MS;
		}
		player.setAttackedCreature(nullptr);
		player.sendCancelTarget();
	}
	state.manualInput(false);
	for (auto it = state.unreachableUntil.begin(); it != state.unreachableUntil.end();) {
		if (it->second <= now) it = state.unreachableUntil.erase(it);
		else ++it;
	}
	SpectatorVec spectators;
	g_game.map.getSpectators(spectators, player.getPosition(), false, false, 7, 7, 5, 5);
	std::vector<Creature*> candidates;
	for (Creature* creature : spectators) {
		if (!state.unreachableUntil.count(creature->getID()) && eligible(player, *creature)) candidates.push_back(creature);
	}
	const Position center = player.getPosition();
	std::sort(candidates.begin(), candidates.end(), [&center](const Creature* left, const Creature* right) {
		const auto distance = [&center](const Creature* creature) {
			return std::max(Position::getDistanceX(center, creature->getPosition()),
			                Position::getDistanceY(center, creature->getPosition()));
		};
		return distance(left) != distance(right) ? distance(left) < distance(right) : left->getID() < right->getID();
	});
	for (Creature* candidate : candidates) {
		// Check reachability only for the retained target; never pathfind every acquisition candidate.
		if (player.setAttackedCreature(candidate) && player.getAttackedCreature() == candidate) {
			state.automaticTargetId = candidate->getID();
			return;
		}
	}
}

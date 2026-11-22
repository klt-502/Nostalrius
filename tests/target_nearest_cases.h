#ifndef ZEDAS_TARGET_NEAREST_CASES_H
#define ZEDAS_TARGET_NEAREST_CASES_H
static void testNearestTarget()
{
    Player* hunter = makePlayer(100, 100);
    Monster* far = makeMonster(5000, 104, 100);
    Monster* diagonal = makeMonster(20, 102, 102);
    Monster* axis = makeMonster(1000, 103, 100);
    Monster* equal = makeMonster(9000, 98, 99);
    makeMonster(10000, 100, 99, false);
    makeMonster(10000, 99, 100, true, false);
    Monster* invisible = makeMonster(10000, 101, 100);
    invisible->addCondition(Condition::createCondition(CONDITIONID_DEFAULT, CONDITION_INVISIBLE, 100000));
    Monster* summon = makeMonster(10000, 100, 101);
    hunter->addSummon(summon);
    Monster* indirect = makeMonster(10000, 101, 101);
    summon->addSummon(indirect);
    makePlayer(101, 99);
    StaticTile* upstairs = new StaticTile(100, 100, 6);
    upstairs->internalAddThing(Item::CreateItem(hunter->getTile()->getGround()->getID()));
    g_game.map.setTile(Position(100, 100, 6), upstairs);
    MonsterType* type = new MonsterType();
    type->name = "other-floor"; type->info.health = type->info.healthMax = 10000;
    type->info.isHostile = type->info.isAttackable = true;
    require(g_game.internalPlaceCreature(new Monster(type), Position(100, 100, 6)), "other-floor fixture");
    require(EasyMode::setAutoTarget(*hunter, true), "nearest automation enabled");
    EasyMode::updateAutoTarget(*hunter, 1000);
    require(hunter->getAttackedCreature() == diagonal, "Chebyshev diagonal beats farther axis; HP ignored; ID resolves equal distance");
    Monster* closer = makeMonster(10, 99, 99);
    EasyMode::updateAutoTarget(*hunter, 1100);
    require(hunter->getAttackedCreature() == diagonal, "closer newcomer cannot steal valid automatic target");
    diagonal->changeHealth(-diagonal->getHealth());
    EasyMode::updateAutoTarget(*hunter, 1200);
    require(hunter->getAttackedCreature() == closer, "death selects nearest surviving eligible monster");
    g_game.playerSetAttackedCreature(hunter->getID(), far->getID());
    EasyMode::updateAutoTarget(*hunter, 1300);
    require(hunter->getAttackedCreature() == far, "manual distant target overrides nearest automation");
    g_game.playerCancelAttackAndFollow(hunter->getID());
    EasyMode::updateAutoTarget(*hunter, 1400);
    require(!hunter->getAttackedCreature() && !hunter->getEasyModeAutoTargetState().active, "Escape remains suspended");
    closer->changeHealth(-closer->getHealth());
    equal->changeHealth(-equal->getHealth());
    axis->changeHealth(-axis->getHealth());
    Monster* fallback = makeMonster(1, 106, 104);
    uint16_t wall = 0;
    for (size_t id = 100; id < Item::items.size(); ++id) {
        const auto& t = Item::items[id];
        if (t.id && !t.isGroundTile() && t.blockSolid && t.blockProjectile && !t.moveable) { wall = id; break; }
    }
    require(wall != 0, "blocking fixture type");
    for (int dx = -1; dx <= 1; ++dx) for (int dy = -1; dy <= 1; ++dy) {
        if (!dx && !dy) continue;
        require(g_game.internalAddItem(g_game.map.getTile(104 + dx, 100 + dy, 7), Item::CreateItem(wall),
                INDEX_WHEREEVER, FLAG_NOLIMIT) == RETURNVALUE_NOERROR, "enclose nearest living target");
    }
    EasyMode::setAutoTarget(*hunter, true);
    EasyMode::updateAutoTarget(*hunter, 2000);
    require(hunter->getAttackedCreature() == far, "acquisition does not pathfind candidates");
    EasyMode::updateAutoTarget(*hunter, 2100);
    EasyMode::updateAutoTarget(*hunter, 5099);
    require(hunter->getAttackedCreature() == far, "three-second unreachable grace retained");
    EasyMode::updateAutoTarget(*hunter, 5100);
    require(hunter->getAttackedCreature() == fallback, "unreachable nearest yields to next eligible target");
    require(hunter->getEasyModeAutoTargetState().unreachableUntil.count(far->getID()) == 1, "temporary suppression recorded");
    fallback->changeHealth(-fallback->getHealth());
    EasyMode::updateAutoTarget(*hunter, 5200);
    require(!hunter->getAttackedCreature(), "blocked target not immediately reacquired");
    EasyMode::updateAutoTarget(*hunter, 15099);
    require(!hunter->getAttackedCreature(), "suppression lasts ten seconds");
    EasyMode::updateAutoTarget(*hunter, 15100);
    require(hunter->getAttackedCreature() == far, "suppression expires; no permanent blacklist");
    hunter->getEasyModeAutoTargetState().unreachableUntil[far->getID()] = 99999;
    g_game.playerSetAttackedCreature(hunter->getID(), far->getID());
    EasyMode::updateAutoTarget(*hunter, 15200);
    require(hunter->getAttackedCreature() == far && !hunter->getEasyModeAutoTargetState().automaticTargetId,
            "manual selection ignores automatic suppression");
}
#endif

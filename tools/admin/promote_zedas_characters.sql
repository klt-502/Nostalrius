-- Operator-run only; never a startup migration. Read docs/ZEDAS_PROMOTIONS.md.
-- First inspect the live schema/data, stop the server and take a backup.
-- Fill these four values from the read-only preview; do not infer IDs.
SET @zedas_paladin_id = NULL;
SET @zedas_paladin_account = NULL;
SET @zedas_knight_id = NULL;
SET @zedas_knight_account = NULL;
-- Explicit operator assertions, not facts inferred from an empty online table.
SET @zedas_server_stopped = 0;
SET @zedas_premium_verified = 0;

START TRANSACTION;
SELECT id, name, account_id, vocation, deleted, deletion
FROM players WHERE id IN (@zedas_paladin_id, @zedas_knight_id)
ORDER BY id FOR UPDATE;

SET @zedas_schema_ok = (
    (SELECT COUNT(*) FROM information_schema.TABLES
     WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME IN ('players', 'player_storage')
       AND ENGINE = 'InnoDB') = 2
    AND (SELECT GROUP_CONCAT(COLUMN_NAME ORDER BY SEQ_IN_INDEX)
         FROM information_schema.STATISTICS WHERE TABLE_SCHEMA = DATABASE()
         AND TABLE_NAME = 'players' AND INDEX_NAME = 'PRIMARY') = 'id'
    AND (SELECT GROUP_CONCAT(COLUMN_NAME ORDER BY SEQ_IN_INDEX)
         FROM information_schema.STATISTICS WHERE TABLE_SCHEMA = DATABASE()
         AND TABLE_NAME = 'player_storage' AND INDEX_NAME = 'PRIMARY') = 'player_id,key'
);
SET @zedas_targets_ok = COALESCE(
    @zedas_schema_ok AND @zedas_server_stopped = 1 AND @zedas_premium_verified = 1
    AND @zedas_paladin_id <> @zedas_knight_id
    AND NOT EXISTS (SELECT 1 FROM players_online)
    AND (SELECT COUNT(*) FROM players p JOIN accounts a ON a.id = p.account_id
         WHERE p.deleted = 0 AND p.deletion = 0 AND (
            (p.id = @zedas_paladin_id AND p.account_id = @zedas_paladin_account
             AND BINARY p.name = BINARY 'Kyle Behe Hunter' AND p.vocation IN (3,7))
            OR
            (p.id = @zedas_knight_id AND p.account_id = @zedas_knight_account
             AND BINARY p.name = BINARY 'Kyle Zedas' AND p.vocation IN (4,8))
         )) = 2, 0
);

UPDATE players
SET vocation = CASE id WHEN @zedas_paladin_id THEN 7 WHEN @zedas_knight_id THEN 8 END
WHERE @zedas_targets_ok = 1 AND id IN (@zedas_paladin_id, @zedas_knight_id);

INSERT INTO player_storage (player_id, `key`, `value`)
SELECT id, 30018, 1 FROM players
WHERE @zedas_targets_ok = 1 AND (
    (id = @zedas_paladin_id AND vocation = 7) OR
    (id = @zedas_knight_id AND vocation = 8)
)
ON DUPLICATE KEY UPDATE `value` = 1;

-- Both expected names, IDs, owners and vocations must be correct before applying.
SELECT @zedas_targets_ok AS guards_passed;
SELECT p.id, p.name, p.account_id, p.vocation, s.`value` AS promotion_storage
FROM players p LEFT JOIN player_storage s ON s.player_id = p.id AND s.`key` = 30018
WHERE p.id IN (@zedas_paladin_id, @zedas_knight_id) ORDER BY p.id;

ROLLBACK; -- Dry run by default. Replace only this final ROLLBACK with COMMIT after review.

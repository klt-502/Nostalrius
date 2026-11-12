-- Easy Mode ONLY. Stop the server and back up before applying, after 001_broker.sql.
-- Additive schema v2: immutable per-item batch assignment; no item or money rewrite.
-- Keep the server stopped if any statement fails; DDL is not transactional in MySQL.
ALTER TABLE zedas_broker_escrow
 ADD COLUMN trip VARCHAR(80) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
 ADD INDEX broker_batch (owner,sold,trip);
ALTER TABLE zedas_broker_accounts
 ADD COLUMN departure_notified TINYINT UNSIGNED NOT NULL DEFAULT 1,
 ADD COLUMN sale_notified TINYINT UNSIGNED NOT NULL DEFAULT 1;
START TRANSACTION;
-- v1 prohibited collection during a trip: all unsold items of an outbound account
-- belong to its existing batch. Preserve deadlines, balances, claims and sale keys.
UPDATE zedas_broker_escrow e JOIN zedas_broker_accounts a ON a.owner=e.owner
 SET e.trip=a.trip WHERE e.sold=0 AND a.state=1;
UPDATE zedas_broker_escrow SET trip=sale WHERE sold=1;
-- Historical trips are deliberately considered already announced.
UPDATE zedas_broker_schema SET version=2 WHERE id=1 AND version=1;
COMMIT;

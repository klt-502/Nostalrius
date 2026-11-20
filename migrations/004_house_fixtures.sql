-- Explicit additive migration. No automatic DDL at server startup.
-- Immutable, per-house fixture plans; never delete/rekey rows as a reset.
CREATE TABLE IF NOT EXISTS zedas_house_fixtures (
  house_id INT NOT NULL,
  plan_hash CHAR(40) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  plan_data TEXT CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
  activated_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (house_id),
  CONSTRAINT zedas_house_fixtures_house_fk FOREIGN KEY (house_id) REFERENCES houses(id) ON DELETE RESTRICT
) ENGINE=InnoDB;

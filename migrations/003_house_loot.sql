-- Explicit additive migration; never automatically run on startup.
-- Do not truncate/delete issuance records or use this table as a reset mechanism.
CREATE TABLE IF NOT EXISTS zedas_house_loot (
  slot_key VARCHAR(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  identity CHAR(40) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
  issued_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (slot_key)
) ENGINE=InnoDB;

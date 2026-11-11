-- Explicit opt-in migration for the Easy Mode database ONLY. Back up first.
-- Additive; never run against Classic. All participating tables must be InnoDB.
CREATE TABLE zedas_broker_schema (id TINYINT NOT NULL PRIMARY KEY, version INT NOT NULL) ENGINE=InnoDB;
INSERT INTO zedas_broker_schema VALUES (1,1);
CREATE TABLE zedas_broker_accounts (
 owner INT NOT NULL PRIMARY KEY,
 pending BIGINT UNSIGNED NOT NULL DEFAULT 0,
 funds BIGINT UNSIGNED NOT NULL DEFAULT 0,
 state TINYINT UNSIGNED NOT NULL DEFAULT 0,
 deadline BIGINT UNSIGNED NOT NULL DEFAULT 0,
 trip VARCHAR(80) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '',
 FOREIGN KEY (owner) REFERENCES players(id) ON DELETE RESTRICT
) ENGINE=InnoDB;
CREATE TABLE zedas_broker_sales (
 operation VARCHAR(80) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
 owner INT NOT NULL,
 gross BIGINT UNSIGNED NOT NULL,
 net BIGINT UNSIGNED NOT NULL,
 commission BIGINT UNSIGNED NOT NULL,
 bps INT UNSIGNED NOT NULL,
 created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
 FOREIGN KEY (owner) REFERENCES zedas_broker_accounts(owner) ON DELETE RESTRICT
) ENGINE=InnoDB;
CREATE TABLE zedas_broker_escrow (
 claim_key VARCHAR(80) CHARACTER SET ascii COLLATE ascii_bin NOT NULL PRIMARY KEY,
 owner INT NOT NULL,
 item SMALLINT UNSIGNED NOT NULL,
 count SMALLINT UNSIGNED NOT NULL,
 price BIGINT UNSIGNED NOT NULL,
 sold TINYINT UNSIGNED NOT NULL DEFAULT 0,
 sale VARCHAR(80) CHARACTER SET ascii COLLATE ascii_bin DEFAULT NULL,
 created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
 INDEX (owner,sold),
 FOREIGN KEY (owner) REFERENCES zedas_broker_accounts(owner) ON DELETE RESTRICT,
 FOREIGN KEY (sale) REFERENCES zedas_broker_sales(operation) ON DELETE RESTRICT
) ENGINE=InnoDB;
-- Only attribute-free, non-decaying, reviewed items enter escrow. Item/count are
-- their complete durable image; no player/house item table also saves them.
CREATE TABLE zedas_broker_payouts (
 id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
 operation VARCHAR(80) CHARACTER SET ascii COLLATE ascii_bin NOT NULL UNIQUE,
 owner INT NOT NULL,
 amount BIGINT UNSIGNED NOT NULL,
 created TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
 INDEX (owner,id),
 FOREIGN KEY (owner) REFERENCES zedas_broker_accounts(owner) ON DELETE RESTRICT
) ENGINE=InnoDB;

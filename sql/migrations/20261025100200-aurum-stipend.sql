-- The Aurum (AUR) stipend's last payment per character.
--
-- Why:  the server can pay every character account.aurStipendAmount AUR each account.aurStipendIntervalHours
--  (src/eve-server/store/AurumStipend.cpp).  lastPaid (a filetime) is advanced with a compare-and-swap UPDATE before a
--  payment, so a restart or a second check never pays a period twice.  A character with no row hasn't been paid yet.
--  see doc/code_and_design_notes/planning/PLAN-nex-store-aur.md
--
-- Idempotent:  CREATE TABLE IF NOT EXISTS / DROP TABLE IF EXISTS.

-- +migrate Up
CREATE TABLE IF NOT EXISTS `chrAurumStipend` (
  `characterID` int(10) unsigned NOT NULL,
  `lastPaid` bigint(20) NOT NULL,
  PRIMARY KEY (`characterID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- +migrate Down
DROP TABLE IF EXISTS `chrAurumStipend`;

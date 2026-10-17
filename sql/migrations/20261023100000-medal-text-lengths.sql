-- Corporation decoration text lengths and lookups.
--
-- Why:  the Crucible client lets a medal's title run to 100 characters and its description to 1000
--  (appConst medalMaxNameLength / medalMaxDescriptionLength, enforced by the server since this change), and the
--  award reason to 200 (medals.GiveMedalToCharacters' NamePopup).  crpMedals.title was varchar(50),
--  crpMedals.description varchar(150) and chrMedals.reason varchar(150), so longer texts the client accepts were cut
--  or refused.  chrMedals and crpMedalData had no index although every lookup is by recipient or medal.
--
-- Idempotent:  MODIFY sets the same definition again, the indexes use IF (NOT) EXISTS.  Down cuts longer texts to
--  the old widths before narrowing the columns.

-- +migrate Up
ALTER TABLE `crpMedals`
  MODIFY `title` varchar(100) COLLATE utf8_unicode_ci NOT NULL,
  MODIFY `description` varchar(1000) COLLATE utf8_unicode_ci NOT NULL;

ALTER TABLE `chrMedals`
  MODIFY `reason` varchar(200) COLLATE utf8_unicode_ci NOT NULL,
  ADD INDEX IF NOT EXISTS `recepientID` (`recepientID`),
  ADD INDEX IF NOT EXISTS `medalID` (`medalID`);

ALTER TABLE `crpMedalData`
  ADD INDEX IF NOT EXISTS `medalID` (`medalID`);

-- +migrate Down
ALTER TABLE `crpMedalData`
  DROP INDEX IF EXISTS `medalID`;

ALTER TABLE `chrMedals`
  DROP INDEX IF EXISTS `medalID`,
  DROP INDEX IF EXISTS `recepientID`;

UPDATE `chrMedals` SET `reason` = LEFT(`reason`, 150) WHERE CHAR_LENGTH(`reason`) > 150;
ALTER TABLE `chrMedals`
  MODIFY `reason` varchar(150) COLLATE utf8_unicode_ci NOT NULL;

UPDATE `crpMedals` SET `title` = LEFT(`title`, 50) WHERE CHAR_LENGTH(`title`) > 50;
UPDATE `crpMedals` SET `description` = LEFT(`description`, 150) WHERE CHAR_LENGTH(`description`) > 150;
ALTER TABLE `crpMedals`
  MODIFY `title` varchar(50) COLLATE utf8_unicode_ci NOT NULL,
  MODIFY `description` varchar(150) COLLATE utf8_unicode_ci NOT NULL;

-- Infrastructure hub reinforcement exit hour.
--
-- Why:  the Crucible client's hub settings window (infrastructureHub.py InfrastructureHubManagementWnd) reads the hub's
--  reinforcement exit time with remoteBallpark.GetSovStructExitTime(hubID) and changes it with
--  CmdChangeSovStructExitTime(hubID, old, new), both as hour * 100 (0 - 2300).  The server stored no such value, so the
--  window raised on None / 100.  The hour is kept per structure here, 0 by default (00:00).
--
-- Idempotent:  ADD / DROP COLUMN IF (NOT) EXISTS.

-- +migrate Up
ALTER TABLE `posStructureData`
  ADD COLUMN IF NOT EXISTS `sovExitTime` smallint(4) NOT NULL DEFAULT 0;

-- +migrate Down
ALTER TABLE `posStructureData`
  DROP COLUMN IF EXISTS `sovExitTime`;

-- Each game's seed, the eight bytes after the recording's table number: what every chance in the
-- game is drawn from, so a recording's keys reach its score only with its own seed. The first
-- player to send a seed owns it; the same seed from anyone else is a copy of their game, however
-- the rest of the file was changed. Filled in here for the games already sent.
ALTER TABLE runs ADD COLUMN seed TEXT;
UPDATE runs SET seed = (SELECT lower(hex(substr(data, 8, 8))) FROM replays WHERE replays.run_id = runs.id);
CREATE INDEX runs_seed ON runs (seed);

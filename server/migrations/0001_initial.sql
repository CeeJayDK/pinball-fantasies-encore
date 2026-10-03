-- Players: a nickname, and the hash of the secret token their game made the first time it
-- sent a score. Nobody else can send scores under that nickname without the token.
CREATE TABLE players (
  id INTEGER PRIMARY KEY,
  nickname TEXT NOT NULL UNIQUE,
  token_hash TEXT NOT NULL UNIQUE,
  created_at INTEGER NOT NULL
);

-- One game each, as it was sent. It counts once the verifier has played the recording again
-- (status 'verified'); what it found is what is kept, not what the recording claims.
CREATE TABLE runs (
  id INTEGER PRIMARY KEY,
  player_id INTEGER NOT NULL REFERENCES players(id),
  replay_sha256 TEXT NOT NULL UNIQUE,
  format INTEGER NOT NULL,
  client_version TEXT,
  status TEXT NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'verified', 'rejected')),
  reason TEXT,
  table_no INTEGER,
  balls INTEGER,
  angle TEXT,
  frames INTEGER,
  score INTEGER,
  submitted_at INTEGER NOT NULL,
  verified_at INTEGER
);
CREATE INDEX runs_board ON runs (status, table_no, balls, angle, score DESC);
CREATE INDEX runs_pending ON runs (status, submitted_at);
CREATE INDEX runs_player ON runs (player_id, submitted_at);

-- The recordings themselves, a few KB each, apart from the rows the boards are read from.
CREATE TABLE replays (
  run_id INTEGER PRIMARY KEY REFERENCES runs(id),
  data BLOB NOT NULL
);

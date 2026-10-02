-- Scores carry the initials typed for them, arcade-style, and anyone may type any initials. A
-- player is an installation of the game, known to the server by the hash of its secret token
-- and to everyone else by a public tag of five hexadecimal digits, unique, given out here the
-- first time it sends a game: "RDX (4e87a)".
PRAGMA defer_foreign_keys = true;

CREATE TABLE players_new (
  id INTEGER PRIMARY KEY,
  token_hash TEXT NOT NULL UNIQUE,
  tag TEXT NOT NULL UNIQUE,
  created_at INTEGER NOT NULL
);
INSERT INTO players_new (id, token_hash, tag, created_at)
  SELECT id, token_hash, substr(token_hash, 1, 5), created_at FROM players;
DROP TABLE players;
ALTER TABLE players_new RENAME TO players;

ALTER TABLE runs ADD COLUMN initials TEXT;

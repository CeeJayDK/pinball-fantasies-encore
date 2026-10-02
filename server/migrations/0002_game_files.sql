-- The original game's table files, which the verifier needs to play recordings again. Only
-- the verifier's token can put them here or read them back; they are never served to anyone
-- else, and they are not in the public repository.
CREATE TABLE game_files (
  name TEXT PRIMARY KEY,
  data BLOB NOT NULL,
  sha256 TEXT NOT NULL,
  uploaded_at INTEGER NOT NULL
);

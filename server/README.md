# Online high scores

When a one-player game makes the local high scores, the game asks, after the initials are
typed, whether to send it online; if so, it sends the game as a recording. A Cloudflare Worker keeps it
as *pending*; a GitHub Actions job (`.github/workflows/verify-scores.yml`) plays it again with
`encore-play --verify` against the game's own files and reports what it found; only then does
the score count. What the recording claims is never believed: the score on the board is the
one the replay arrives at.

- `src/index.ts`: the Worker, with the API at the top of the file.
- `migrations/`: the D1 database.
- `verify.sh`: the checking, as the job runs it.
- `upload-game-files.sh`: puts the game's table files on the server, for the job.

The Worker, its database, the recordings (a few KB each) and the game's table files (about
3 MB, readable only with the verifier's token) all fit in Cloudflare's free plan.

## Setting it up

You need a Cloudflare account and Node.js. In this folder:

```bash
npm install
npx wrangler login
npx wrangler d1 create encore-scores
```

Put the `database_id` it prints into `wrangler.toml`, then:

```bash
npx wrangler d1 migrations apply encore-scores --remote
npx wrangler secret put VERIFIER_TOKEN
npx wrangler deploy
```

For `VERIFIER_TOKEN`, paste a long random secret, for example the output of
`openssl rand -hex 32`. Keep it: the checking job needs the same one. `deploy` prints the
Worker's address, `https://pinball-fantasies-encore.<your subdomain>.workers.dev`.

Then put the game's table files on the server, from a machine that has the game:

```bash
ENCORE_API=https://pinball-fantasies-encore.<your subdomain>.workers.dev VERIFIER_TOKEN=<the secret> \
  ./upload-game-files.sh <the game's folder>
```

And give the checking job the two things it needs, in the GitHub repository's **Settings →
Secrets and variables → Actions**: a **secret** `VERIFIER_TOKEN` (the same secret) and a
**variable** `ENCORE_API` (the Worker's address). The job runs every ten minutes from the
default branch, or by hand from the Actions tab. GitHub stops scheduled jobs in a repository
with no activity for 60 days; the Actions tab turns it back on.

After a change to the Worker or a new migration:

```bash
npx wrangler d1 migrations apply encore-scores --remote
npx wrangler deploy
```

## Trying it locally

```bash
printf 'VERIFIER_TOKEN=local-secret\n' > .dev.vars
npx wrangler d1 migrations apply encore-scores --local
npx wrangler dev
```

Then, from the repository root, with a player token of your own (`openssl rand -hex 32`), and a
recording with initials (`ENCORE_INITIALS=RDX ENCORE_RECORD=rdx.RPL build/encore-play <the
game's folder> 2` makes one; or point the game itself at the local server with
`ENCORE_API=http://localhost:8787`):

```bash
curl -X POST -H "Authorization: Bearer $TOKEN" --data-binary @rdx.RPL http://localhost:8787/v1/runs
ENCORE_API=http://localhost:8787 VERIFIER_TOKEN=local-secret ENCORE_PLAY=build/encore-play \
  GAME_DIR=<the game's folder> server/verify.sh
curl "http://localhost:8787/v1/scores?table=1"
```

## What counts

One whole game, by one player, played to its end without cheats (no tilt, slow motion, or
more balls than the options give), with initials typed for it, in a recording format the
verifier can play.

A player is an installation of the game: it makes a secret token the first time it sends a
game (kept in `online.txt` beside the high scores) and the server gives it a public tag of
five hexadecimal digits. A score shows the initials typed for it and the tag, `RDX (4e87a)`,
so anyone may type any initials and still be told apart. The boards show the best
verified score of each installation and initials per table, so everyone who plays on one
computer has a place of their own, and can be narrowed to a ball count and an angle; `/v1/players/<tag>` lists all of one installation's games, and every verified game's
recording can be downloaded from `/v1/runs/<id>/replay`.

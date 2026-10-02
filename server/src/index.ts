// Online high scores for Pinball Fantasies: Encore!
//
// The game sends each game it played as a recording. Nothing it claims is believed: a
// recording waits here as 'pending' until the verifier (a GitHub Actions job running
// encore-play --verify against the game's own files) has played it again and reported what it
// found, and only then does its score count.
//
//   GET  /                                         a page with the four boards
//   GET  /v1/scores?table=1&balls=3&angle=high   best verified score per player
//   GET  /v1/players/<tag>                         a player's verified games
//   POST /v1/runs               <recording>        send a game                  (player token)
//   GET  /v1/runs/<id>                             a game, and its rank once verified
//   GET  /v1/runs/<id>/replay                      a verified game's recording
//   GET  /v1/verifier/pending                      games waiting to be checked  (verifier token)
//   GET  /v1/verifier/runs/<id>/replay             any game's recording         (verifier token)
//   POST /v1/verifier/runs/<id>  <verdict JSON>    what the verifier found      (verifier token)
//   GET  /v1/verifier/files                        the game's table files kept  (verifier token)
//   GET  /v1/verifier/files/<name>                 one of them                  (verifier token)
//   PUT  /v1/verifier/files/<name>  <file>         keep one (TABLE1.PRG ...)    (verifier token)
//
// A player is an installation of the game. Its token is a random 64-hex-digit secret the game
// makes and keeps, and sends with every game; only its SHA-256 is stored here. Everyone else
// knows it by its tag, five hexadecimal digits given out the first time it sends a game. A
// score shows the initials typed for it, which anyone may type, and the tag: "RDX (4e87a)".
// The verifier's token is a Worker secret, VERIFIER_TOKEN.

export interface Env {
  DB: D1Database;
  VERIFIER_TOKEN: string;
}

/** Recording formats a verifier can play (Replay::kFormat in the game). */
const FORMATS = [3];
const MAX_RECORDING = 512 * 1024;
const MAX_PENDING_PER_PLAYER = 50;
const MAX_RUNS_PER_DAY = 300;
/** As the game lets them be typed: three capitals or spaces. */
const INITIALS = /^[A-Z ]{3}$/;
const TAG = /^[0-9a-f]{5}$/;
/** The tables as recordings are named after them (kTableCodes in the game). */
const TABLE_CODES = ["PARTYLND", "SPDDEVLS", "GAMESHOW", "STONBONE"];

/** As the game names a recording, with the tag: FANTASY-STONBONE-RDX-56070-67108120-20261002.RPL */
function fileName(table: number, initials: string | null, tag: string, score: number, at: number): string {
  const who = initials && INITIALS.test(initials) ? initials.replace(/ /g, "_") : "---";
  const day = new Date(at * 1000).toISOString().slice(0, 10).replace(/-/g, "");
  return `FANTASY-${TABLE_CODES[table - 1]}-${who}-${tag}-${score}-${day}.RPL`;
}
const ANGLES = ["low", "high", "higher"];
/** The original game's files the verifier plays recordings with. */
const GAME_FILE = /^TABLE[1-4]\.(PRG|MOD)$/;
const MAX_GAME_FILE = 1536 * 1024;

const CORS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
  "Access-Control-Allow-Headers": "Authorization, Content-Type, X-Encore-Version",
};

function json(body: unknown, status = 200): Response {
  return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json", ...CORS } });
}
const fail = (status: number, error: string) => json({ error }, status);

async function sha256(data: ArrayBuffer | Uint8Array): Promise<string> {
  const hash = await crypto.subtle.digest("SHA-256", data);
  return [...new Uint8Array(hash)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

function bearer(req: Request): string | null {
  const m = /^Bearer ([0-9a-f]{64})$/.exec(req.headers.get("Authorization") ?? "");
  return m ? m[1] : null;
}

/** Comparing secrets without the time it takes saying how much of one was right. */
function sameSecret(a: string, b: string): boolean {
  if (a.length !== b.length) return false;
  let d = 0;
  for (let i = 0; i < a.length; ++i) d |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return d === 0;
}

const now = () => Math.floor(Date.now() / 1000);

interface Player {
  id: number;
  tag: string;
}

/** The installation a token belongs to, made the first time it is seen, with a tag of its own. */
async function playerOf(env: Env, req: Request): Promise<Player | null> {
  const token = bearer(req);
  if (!token) return null;
  const tokenHash = await sha256(new TextEncoder().encode(token));
  const known = await env.DB.prepare("SELECT id, tag FROM players WHERE token_hash = ?").bind(tokenHash).first<Player>();
  if (known) return known;
  for (let tries = 0; tries < 20; ++tries) {
    const tag = [...crypto.getRandomValues(new Uint8Array(3))].map((b) => b.toString(16).padStart(2, "0")).join("").slice(0, 5);
    const made = await env.DB.prepare(
      "INSERT INTO players (token_hash, tag, created_at) VALUES (?, ?, ?) ON CONFLICT DO NOTHING RETURNING id, tag",
    )
      .bind(tokenHash, tag, now())
      .first<Player>();
    if (made) return made;
    // The tag was taken, or the same token arrived twice at once.
    const raced = await env.DB.prepare("SELECT id, tag FROM players WHERE token_hash = ?").bind(tokenHash).first<Player>();
    if (raced) return raced;
  }
  return null;
}

function blob(value: unknown): Uint8Array {
  return value instanceof ArrayBuffer ? new Uint8Array(value) : Uint8Array.from(value as number[]);
}

// ---- the boards ----------------------------------------------------------------------------

/** The best verified score of each player on a table, optionally for one ball count and angle. */
async function scores(env: Env, url: URL): Promise<Response> {
  const table = Number(url.searchParams.get("table"));
  if (!(table >= 1 && table <= 4)) return fail(400, "table must be 1 to 4");
  const ballsParam = url.searchParams.get("balls");
  const balls = ballsParam === null ? null : Number(ballsParam);
  if (balls !== null && !(balls >= 1 && balls <= 9)) return fail(400, "balls must be 1 to 9");
  const angle = url.searchParams.get("angle");
  if (angle !== null && !ANGLES.includes(angle)) return fail(400, "angle must be low, high or higher");
  const limit = Math.min(Math.max(Number(url.searchParams.get("limit") ?? 50) || 50, 1), 200);
  const { results } = await env.DB.prepare(
    `SELECT initials, tag, score, balls, angle, run, at FROM (
       SELECT r.initials, p.tag, r.score, r.balls, r.angle, r.id AS run, r.verified_at AS at,
              ROW_NUMBER() OVER (PARTITION BY r.player_id ORDER BY r.score DESC, r.verified_at ASC) AS n
       FROM runs r JOIN players p ON p.id = r.player_id
       WHERE r.status = 'verified' AND r.table_no = ?1 AND (?2 IS NULL OR r.balls = ?2) AND (?3 IS NULL OR r.angle = ?3))
     WHERE n = 1 ORDER BY score DESC, at ASC LIMIT ?4`,
  )
    .bind(table, balls, angle, limit)
    .all();
  return json({ table, balls, angle, scores: results.map((r, i) => ({ rank: i + 1, ...r })) });
}

async function playerPage(env: Env, tag: string): Promise<Response> {
  if (!TAG.test(tag)) return fail(404, "no such player");
  const player = await env.DB.prepare("SELECT id, tag, created_at FROM players WHERE tag = ?")
    .bind(tag)
    .first<Player & { created_at: number }>();
  if (!player) return fail(404, "no such player");
  const { results } = await env.DB.prepare(
    `SELECT id AS run, initials, table_no AS "table", balls, angle, score, frames, verified_at AS at FROM runs
     WHERE player_id = ? AND status = 'verified' ORDER BY verified_at DESC LIMIT 200`,
  )
    .bind(player.id)
    .all();
  return json({ tag: player.tag, since: player.created_at, runs: results });
}

// ---- games ---------------------------------------------------------------------------------

/** Takes a recording to be checked. Only what can be seen without playing it is looked at. */
async function sendRun(env: Env, req: Request): Promise<Response> {
  const player = await playerOf(env, req);
  if (!player) return fail(401, "a player token is needed");
  const data = new Uint8Array(await req.arrayBuffer());
  if (data.length < 8 || data.length > MAX_RECORDING) return fail(413, "not a recording, or too big");
  if (String.fromCharCode(...data.slice(0, 4)) !== "PFRP") return fail(400, "not a recording");
  const format = data[4] | (data[5] << 8);
  if (!FORMATS.includes(format)) return fail(400, "a recording from a version this server cannot check");
  const hash = await sha256(data);
  const existing = await env.DB.prepare("SELECT id, player_id, status FROM runs WHERE replay_sha256 = ?")
    .bind(hash)
    .first<{ id: number; player_id: number; status: string }>();
  if (existing) {
    if (existing.player_id !== player.id) return fail(409, "already sent by someone else");
    return json({ id: existing.id, status: existing.status, tag: player.tag });
  }
  const counts = await env.DB.prepare(
    `SELECT SUM(status = 'pending') AS pending, SUM(submitted_at > ?2) AS today FROM runs WHERE player_id = ?1`,
  )
    .bind(player.id, now() - 86400)
    .first<{ pending: number | null; today: number | null }>();
  if ((counts?.pending ?? 0) >= MAX_PENDING_PER_PLAYER || (counts?.today ?? 0) >= MAX_RUNS_PER_DAY)
    return fail(429, "too many games waiting; try again later");
  const version = (req.headers.get("X-Encore-Version") ?? "").slice(0, 32) || null;
  const run = await env.DB.prepare(
    `INSERT INTO runs (player_id, replay_sha256, format, client_version, submitted_at) VALUES (?, ?, ?, ?, ?) RETURNING id`,
  )
    .bind(player.id, hash, format, version, now())
    .first<{ id: number }>();
  await env.DB.prepare("INSERT INTO replays (run_id, data) VALUES (?, ?)").bind(run!.id, data).run();
  return json({ id: run!.id, status: "pending", tag: player.tag }, 202);
}

async function getRun(env: Env, id: number): Promise<Response> {
  const run = await env.DB.prepare(
    `SELECT r.id, r.initials, p.tag, r.status, r.reason, r.table_no AS "table", r.balls, r.angle, r.score, r.frames,
            r.submitted_at, r.verified_at
     FROM runs r JOIN players p ON p.id = r.player_id WHERE r.id = ?`,
  )
    .bind(id)
    .first<Record<string, unknown>>();
  if (!run) return fail(404, "no such game");
  if (run.status === "verified") {
    // Among each player's best on the same table, balls and angle.
    const above = await env.DB.prepare(
      `SELECT COUNT(*) AS n FROM (SELECT MAX(score) AS best FROM runs
         WHERE status = 'verified' AND table_no = ? AND balls = ? AND angle = ? GROUP BY player_id) WHERE best > ?`,
    )
      .bind(run.table, run.balls, run.angle, run.score)
      .first<{ n: number }>();
    run.rank = (above?.n ?? 0) + 1;
  }
  return json(run);
}

async function getReplay(env: Env, id: number, anyStatus: boolean): Promise<Response> {
  const row = await env.DB.prepare(
    `SELECT d.data, r.status, r.table_no, r.initials, r.score, r.verified_at, p.tag
     FROM replays d JOIN runs r ON r.id = d.run_id JOIN players p ON p.id = r.player_id
     WHERE r.id = ? AND (? OR r.status = 'verified')`,
  )
    .bind(id, anyStatus ? 1 : 0)
    .first<{ data: unknown; status: string; table_no: number; initials: string | null; score: number; verified_at: number; tag: string }>();
  if (!row) return fail(404, "no such recording");
  const name =
    row.status === "verified" ? fileName(row.table_no, row.initials, row.tag, row.score, row.verified_at) : `FANTASY-${id}.RPL`;
  return new Response(blob(row.data), {
    headers: {
      "Content-Type": "application/octet-stream",
      "Content-Disposition": `attachment; filename="${name}"`,
      ...CORS,
    },
  });
}

// ---- the verifier --------------------------------------------------------------------------

function isVerifier(env: Env, req: Request): boolean {
  const token = (req.headers.get("Authorization") ?? "").replace(/^Bearer /, "");
  return !!env.VERIFIER_TOKEN && sameSecret(token, env.VERIFIER_TOKEN);
}

async function pending(env: Env, url: URL): Promise<Response> {
  const limit = Math.min(Math.max(Number(url.searchParams.get("limit") ?? 50) || 50, 1), 500);
  const { results } = await env.DB.prepare(
    "SELECT id, format FROM runs WHERE status = 'pending' ORDER BY submitted_at LIMIT ?",
  )
    .bind(limit)
    .all();
  return json({ runs: results });
}

/** What encore-play --verify printed for a game: the game counts only for one player, whole. */
interface Verdict {
  ok: boolean;
  reason?: string;
  format?: number;
  table?: number;
  balls?: number;
  angle?: string;
  frames?: number;
  games?: { endFrame: number; abandoned: boolean; initials?: string; scores: number[] }[];
}

async function report(env: Env, req: Request, id: number): Promise<Response> {
  const v = (await req.json().catch(() => null)) as Verdict | null;
  if (!v || typeof v.ok !== "boolean") return fail(400, "not a verdict");
  const run = await env.DB.prepare("SELECT status FROM runs WHERE id = ?").bind(id).first<{ status: string }>();
  if (!run) return fail(404, "no such game");
  let reason: string | null = null;
  if (!v.ok) reason = v.reason ?? "not believed";
  else if (v.games?.length !== 1) reason = "not one whole game";
  else if (v.games[0].scores.length !== 1) reason = "more than one player";
  else if (!INITIALS.test(v.games[0].initials ?? "")) reason = "no initials typed for it";
  else if (!(v.table! >= 1 && v.table! <= 4) || !ANGLES.includes(v.angle!) || !(v.balls! >= 1 && v.balls! <= 9))
    reason = "a verdict that makes no sense";
  if (reason) {
    await env.DB.prepare("UPDATE runs SET status = 'rejected', reason = ?, verified_at = ? WHERE id = ?")
      .bind(reason.slice(0, 200), now(), id)
      .run();
    return json({ id, status: "rejected", reason });
  }
  await env.DB.prepare(
    `UPDATE runs SET status = 'verified', reason = NULL, table_no = ?, balls = ?, angle = ?, frames = ?, score = ?,
       initials = ?, verified_at = ? WHERE id = ?`,
  )
    .bind(v.table, v.balls, v.angle, v.frames, v.games![0].scores[0], v.games![0].initials, now(), id)
    .run();
  return json({ id, status: "verified" });
}

// ---- the game's own files, for the verifier only --------------------------------------------

async function listFiles(env: Env): Promise<Response> {
  const { results } = await env.DB.prepare(
    "SELECT name, sha256, length(data) AS size, uploaded_at FROM game_files ORDER BY name",
  ).all();
  return json({ files: results });
}

async function getFile(env: Env, name: string): Promise<Response> {
  if (!GAME_FILE.test(name)) return fail(404, "no such file");
  const row = await env.DB.prepare("SELECT data FROM game_files WHERE name = ?").bind(name).first<{ data: unknown }>();
  if (!row) return fail(404, "no such file");
  return new Response(blob(row.data), { headers: { "Content-Type": "application/octet-stream" } });
}

async function putFile(env: Env, req: Request, name: string): Promise<Response> {
  if (!GAME_FILE.test(name)) return fail(400, "only TABLE1.PRG to TABLE4.MOD");
  const data = new Uint8Array(await req.arrayBuffer());
  if (data.length === 0 || data.length > MAX_GAME_FILE) return fail(413, "empty, or too big");
  const hash = await sha256(data);
  await env.DB.prepare(
    `INSERT INTO game_files (name, data, sha256, uploaded_at) VALUES (?1, ?2, ?3, ?4)
     ON CONFLICT (name) DO UPDATE SET data = ?2, sha256 = ?3, uploaded_at = ?4`,
  )
    .bind(name, data, hash, now())
    .run();
  return json({ name, sha256: hash, size: data.length });
}

// ---- the page ------------------------------------------------------------------------------

/** The four boards, read from the API by the page itself; plain for now. */
const PAGE = `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Pinball Fantasies: Encore! high scores</title>
</head>
<body>
<h1>Pinball Fantasies: Encore! high scores</h1>
<p>Every score here was played again by the server from the game's own recording.</p>
<div id="boards">Loading...</div>
<script>
const TABLES = ["Party Land", "Speed Devils", "Billion Dollar Gameshow", "Stones 'n' Bones"];
const CODES = ["PARTYLND", "SPDDEVLS", "GAMESHOW", "STONBONE"];
const text = (tag, content) => { const e = document.createElement(tag); e.textContent = content; return e; };
async function board(n) {
  const section = document.createElement("section");
  section.append(text("h2", TABLES[n - 1]));
  try {
    const r = await fetch("/v1/scores?table=" + n);
    const { scores } = await r.json();
    if (!scores.length) {
      section.append(text("p", "No scores yet."));
      return section;
    }
    const table = document.createElement("table");
    const head = table.insertRow();
    for (const h of ["#", "Player", "Score", "Balls", "Angle", "Date", "Recording"]) head.append(text("th", h));
    for (const s of scores) {
      const row = table.insertRow();
      row.append(text("td", s.rank), text("td", s.initials + " (" + s.tag + ")"),
                 text("td", Number(s.score).toLocaleString("en")), text("td", s.balls), text("td", s.angle),
                 text("td", new Date(s.at * 1000).toISOString().slice(0, 10)));
      const link = document.createElement("a");
      link.href = "/v1/runs/" + s.run + "/replay";
      // Named here too, as the server names it, since not every browser goes by the server.
      link.download = ["FANTASY", CODES[n - 1], s.initials.replace(/ /g, "_"), s.tag, s.score,
                       new Date(s.at * 1000).toISOString().slice(0, 10).replace(/-/g, "")].join("-") + ".RPL";
      link.textContent = "download";
      const cell = document.createElement("td");
      cell.append(link);
      row.append(cell);
    }
    section.append(table);
  } catch {
    section.append(text("p", "The scores could not be read."));
  }
  return section;
}
Promise.all([1, 2, 3, 4].map(board)).then((sections) => document.getElementById("boards").replaceChildren(...sections));
</script>
</body>
</html>
`;

// ---- routing -------------------------------------------------------------------------------

export default {
  async fetch(req: Request, env: Env): Promise<Response> {
    if (req.method === "OPTIONS") return new Response(null, { status: 204, headers: CORS });
    const url = new URL(req.url);
    const path = url.pathname.replace(/\/+$/, "");
    const get = req.method === "GET", post = req.method === "POST";
    let m: RegExpExecArray | null;
    try {
      if (get && path === "") return new Response(PAGE, { headers: { "Content-Type": "text/html; charset=utf-8" } });
      if (get && path === "/v1/scores") return await scores(env, url);
      if (get && (m = /^\/v1\/players\/([^/]+)$/.exec(path))) return await playerPage(env, decodeURIComponent(m[1]));
      if (post && path === "/v1/runs") return await sendRun(env, req);
      if (get && (m = /^\/v1\/runs\/(\d+)$/.exec(path))) return await getRun(env, Number(m[1]));
      if (get && (m = /^\/v1\/runs\/(\d+)\/replay$/.exec(path))) return await getReplay(env, Number(m[1]), false);
      if (path.startsWith("/v1/verifier/")) {
        if (!isVerifier(env, req)) return fail(401, "verifier only");
        if (get && path === "/v1/verifier/pending") return await pending(env, url);
        if (get && (m = /^\/v1\/verifier\/runs\/(\d+)\/replay$/.exec(path))) return await getReplay(env, Number(m[1]), true);
        if (post && (m = /^\/v1\/verifier\/runs\/(\d+)$/.exec(path))) return await report(env, req, Number(m[1]));
        if (get && path === "/v1/verifier/files") return await listFiles(env);
        if (get && (m = /^\/v1\/verifier\/files\/([^/]+)$/.exec(path))) return await getFile(env, m[1]);
        if (req.method === "PUT" && (m = /^\/v1\/verifier\/files\/([^/]+)$/.exec(path)))
          return await putFile(env, req, m[1]);
      }
      return fail(404, "nothing here");
    } catch (e) {
      console.error(e);
      return fail(500, "something went wrong");
    }
  },
};

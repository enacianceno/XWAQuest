# AGENTS.md

Instructions for AI coding agents (OpenAI Codex, GitHub Copilot, Cursor,
Windsurf, Amp, Devin) working in this repository. See <https://agentsmd.io>.

<!-- BEGIN sqz-agents-guidance (auto-installed by sqz init; remove this block to disable) -->

## sqz — Token-Optimized CLI Output

When running shell commands whose output may be long (directory listings,
git log/diff, test runners, build logs, `docker ps`, `kubectl get`, etc.),
pipe the output through `sqz compress` to reduce token consumption.

`sqz` is a stdin-to-stdout compressor, not a command wrapper. The correct
usage is to pipe the command's output into `sqz compress`:

```bash
# Instead of:     Use:
git status        git status 2>&1 | C:/Users/enaci/AppData/Local/Programs/sqz/bin/sqz.exe compress
cargo test        cargo test 2>&1 | C:/Users/enaci/AppData/Local/Programs/sqz/bin/sqz.exe compress
git log -10       git log -10 2>&1 | C:/Users/enaci/AppData/Local/Programs/sqz/bin/sqz.exe compress
docker ps         docker ps 2>&1 | C:/Users/enaci/AppData/Local/Programs/sqz/bin/sqz.exe compress
ls -la            ls -la 2>&1 | C:/Users/enaci/AppData/Local/Programs/sqz/bin/sqz.exe compress
```

The `2>&1` captures stderr too, which is useful for commands like `cargo
test` where diagnostics go to stderr. `sqz compress` filters and compresses
the combined output while preserving filenames, paths, and identifiers.
It typically saves 60-90% tokens on verbose commands.

Do NOT pipe output for:
- Interactive commands (`vim`, `ssh`, `python`, REPLs)
- Compound commands with shell operators (`cmd && other`, `cmd > file.txt`,
  `cmd; other`) — run those directly
- Short commands whose output is already a few lines

If `sqz` is not on PATH, run commands normally.

The `sqz-mcp` MCP server is also available — Codex reads it from
`~/.codex/config.toml` under `[mcp_servers.sqz]`. It exposes
`compress` (the default pipeline), `passthrough` (return text
unchanged — the escape hatch below), `expand` (resolve a
`§ref:HASH§` token back to the original bytes), `sqz_recall`
(full-text search over everything sqz has compressed — recover
context lost to compaction), and `sqz_read_file` / `sqz_grep` /
`sqz_list_dir` (file access with a dedup cache: a repeat read of an
unchanged file costs ~13 tokens, and a ranged re-read via `offset` /
`limit` of a file you already have in full returns a
`§ref:HASH:L40-80§` line-range reference). Prefer those three over
`cat`, `grep` and `ls` for anything you might read more than once.

## Escape hatch — when sqz output confuses you

If you see a `§ref:HASH§` token and can't parse it, or compressed
output is leading you to make lots of small retries instead of one
big request, use one of these:

- **`C:/Users/enaci/AppData/Local/Programs/sqz/bin/sqz.exe expand <prefix>`** — resolve a dedup ref back to the
  original bytes. Accepts bare hex (`sqz expand a1b2c3d4`) or the full
  token pasted verbatim (`sqz expand §ref:a1b2c3d4§`).
- **`SQZ_NO_DEDUP=1`** — set this env var for one command to disable
  dedup: `SQZ_NO_DEDUP=1 git status 2>&1 | sqz compress`. You'll get
  the full compressed output with no `§ref:…§` tokens.
- **`--no-cache`** — same opt-out as a CLI flag:
  `git status 2>&1 | sqz compress --no-cache`.
- **`SQZ_NO_ABBREV=1`** (or **`--no-abbrev`**) — disable n-gram phrase
  abbreviation, which rewrites repeated phrases to `«A1»` symbols and
  keeps only the first occurrence. Use it when output is full of
  SHAs/paths/URLs you'll copy-paste verbatim:
  `SQZ_NO_ABBREV=1 git log 2>&1 | sqz compress`.

If you're using the MCP server, the `passthrough` tool returns raw
text and the `expand` tool resolves refs — call them when you need
data sqz hasn't touched.

<!-- END sqz-agents-guidance -->

# AGENTS.md

This repo is intentionally simple. Prefer direct fixes over process.
No packet systems, control planes, approval workflows, or governance frameworks.

## Pre-authorized (do not ask)

Agents may read, edit, build, test, commit, and push to `main` autonomously for normal project work:

- File reads/writes inside this repo
- Git: status, diff, add, commit, push, pull, fetch, log, show, branch, checkout/switch
- Python scripts used by this repo
- Arduino / ESP32 compile, serial monitor, local validation
- Flashing the known multimeter XIAO ESP32-C6 when firmware work was requested
- HTTP (`curl`) to localhost, `meter.local`, the meter's LAN IP, GitHub, GitHub Pages, and the project's public relay
- GitHub Actions inspection/reruns, GitHub Pages updates, ordinary repo operations

Do not ask for approval for routine development commands.

## Validating `meter.local`

Browser site permissions (e.g. "Allow Claude to execute JavaScript on meter.local?") are an app-level setting, not controlled by `.claude/settings.json`, so they prompt every time. Avoid that surface: validate with `curl`, direct WebSocket inspection (Python), serial output, and HTTP endpoints. Use browser JavaScript on `meter.local` only when no equivalent non-browser path exists.

## Ask first

- Deleting large amounts of repo content
- Force-pushing or rewriting public history
- Changing repo visibility or deleting the repo
- Rotating/deleting credentials, or changing Wi-Fi credentials
- Flashing any device other than the known multimeter, or destructive erase (unless a firmware task clearly requires it)
- Installing broad system software or changing OS settings
- Paid external services
- Publishing secrets
- Making the relay/private data model materially less secure

## Scope

Keep changes inside this repo. Do not touch Device Hub or any other repo.

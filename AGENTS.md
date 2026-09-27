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

## Browser validation (MANDATORY route: localhost proxy)

Claude browser JavaScript/DOM actions on `unit-meter.local` or `192.168.1.x` trigger a per-action site-permission prompt that no repo setting can suppress. So:

- NEVER run Claude browser actions (clicks, screenshots, DOM inspection, JavaScript) directly against `unit-meter.local` or `192.168.1.x`.
- When browser/UI validation is needed, start the proxy yourself (no permission needed; idempotent, reuses a running one):
  `python3 tools/meter_proxy.py start`
- Do all browser automation at `http://localhost:8765/` (proxies HTTP and `/ws` to the meter).
- Confirm it works with `python3 tools/meter_proxy.py check` (HTTP + WebSocket PASS/FAIL). Stop with `... stop`.
- Keep using `curl`, direct WebSocket clients and serial tools straight against `unit-meter.local`.
- localStorage is per-origin: saved readings made on `localhost:8765` are separate from those on the meter's own origins.

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

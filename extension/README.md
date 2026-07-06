# animepahe downloader (Chrome extension)

A self-contained Chrome/Edge/Brave extension that resolves **and downloads**
animepahe episodes entirely inside your real browser.

## Why this exists

animepahe.\*, pahe.win, and kwik.cx all sit behind Cloudflare's interactive
JS challenge. No external HTTP client — libcurl, curl-impersonate, yt-dlp, or
the companion CLI — can get past it, even with a valid `cf_clearance` cookie,
because the challenge requires a real browser running its JavaScript.

This extension sidesteps that completely: it drives real (minimized, unfocused)
browser tabs where the challenge auto-solves, does same-origin fetches / reads
the rendered DOM to resolve the download chain, and lets the browser itself
perform the final download (which carries the `Referer` the CDN's hotlink
protection requires).

## Install

1. Open `chrome://extensions` (or `edge://extensions`, `brave://extensions`).
2. Toggle **Developer mode** on (top right).
3. Click **Load unpacked** and select this `extension/` folder.
4. Pin the toolbar icon (optional): puzzle-piece menu → pin.

No cookie copying, no env vars, no CLI needed.

## Use

1. Click the extension's toolbar icon.
2. Paste an anime URL (`https://animepahe.pw/anime/…`) or a single episode
   URL (`…/play/…`).
3. Choose episodes (`all`, `3`, or `1-12`), quality, and audio.
4. Click **Download**.

Episodes save to `Downloads/<AnimeTitle>/<AnimeTitle>_EP01_1080p.mp4`, etc.

A small minimized popup window flashes as the extension drives tabs through the
resolve chain — that's expected. The progress log in the popup (and the
service-worker console) shows each episode's status. Leave the browser open
until it reports done; downloads themselves continue in the background.

## How it works

- **animepahe** — opens the `/anime/` page in a real tab, then same-origin
  fetches the release API + each `/play/` page from a content script (these
  pass because they're first-party requests from a challenge-solved page).
  Play-page fetches are throttled + retried to avoid HTTP 429.
- **pahe.win → kwik** — navigates the worker tab to each pahe.win link and
  reads the kwik URL from the rendered page.
- **kwik → file** — loads the kwik `/f/` page (challenge auto-solves), waits
  for kwik's own JS to build the download form, then submits it so the browser
  performs the download with the correct referer. Chrome's "multiple files"
  block is lifted for kwik via the `contentSettings` API so batches go through.

## Notes / limits

- Windows/macOS/Linux all work — it's pure browser, no native code.
- Quality/audio selection is best-effort from the pahe.win labels; if a
  requested quality isn't present it falls back to the highest available.
- Requires the browser to stay open during a run.
- If a single episode fails (transient challenge/redirect), it's logged and the
  batch continues; just re-run for the missed ones.
- Permissions requested: `tabs`, `scripting` (drive the worker tab),
  `downloads` (save files + name them), `contentSettings` (allow batch
  downloads for kwik), `storage` (remember your last inputs). No data leaves
  your machine.

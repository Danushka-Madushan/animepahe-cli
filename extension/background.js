/**
 * animepahe downloader — resolves and downloads episodes entirely inside the
 * user's real browser.
 *
 * Why: animepahe.<tld>, pahe.win, and kwik.cx/f all sit behind Cloudflare
 * challenges that 403 any external HTTP client (libcurl, curl-impersonate,
 * yt-dlp) even with a valid cf_clearance cookie — the challenge needs a real
 * browser running its JS. So we drive real (minimized, unfocused) tabs: each
 * gated page loads in a genuine browser context where the challenge auto-solves,
 * then we same-origin fetch / read the DOM, and finally let the browser itself
 * download the mp4 (which also handles the CDN's referer/token requirements).
 */

const NAV_TIMEOUT_MS = 45000;
const FORM_TIMEOUT_MS = 30000;
const DOWNLOAD_WAIT_MS = 40000;

let workerWindowId = null;
let workerTabId = null;
let jobRunning = false;
let pendingFilename = null;

function isFrameGoneError(e) {
  const m = (e && e.message) || "";
  return (
    m.includes("Frame with ID") ||
    m.includes("No frame with id") ||
    m.includes("No tab with id") ||
    m.includes("The tab was closed") ||
    m.includes("Cannot access") ||
    m.includes("frame was removed")
  );
}

/* ------------------------------------------------------------------ */
/* download filename control + creation wait                          */
/* ------------------------------------------------------------------ */

/* The kwik download is triggered by submitting kwik's own form, so the browser
 * performs the navigation → 302 → CDN download itself, carrying the correct
 * Referer (which the CDN's Cloudflare hotlink protection requires). We only
 * override the saved filename to our Anime/EP structure. */
chrome.downloads.onDeterminingFilename.addListener((item, suggest) => {
  if (pendingFilename) {
    suggest({ filename: pendingFilename, conflictAction: "uniquify" });
    pendingFilename = null;
  } else {
    suggest();
  }
});

function waitForDownloadCreated(timeoutMs) {
  return new Promise((resolve) => {
    const timer = setTimeout(() => {
      chrome.downloads.onCreated.removeListener(onCreated);
      resolve(null);
    }, timeoutMs);
    function onCreated(item) {
      clearTimeout(timer);
      chrome.downloads.onCreated.removeListener(onCreated);
      resolve(item);
    }
    chrome.downloads.onCreated.addListener(onCreated);
  });
}

/* ------------------------------------------------------------------ */
/* progress + messaging                                               */
/* ------------------------------------------------------------------ */

function progress(text, level, done = false) {
  console.log("[dl]", text);
  chrome.runtime
    .sendMessage({ type: "progress", text, level, done })
    .catch(() => {}); /* popup may be closed */
}

chrome.runtime.onMessage.addListener((msg, _sender, sendResponse) => {
  if (msg.type === "start") {
    if (jobRunning) {
      progress("A download is already running — wait for it to finish.", "err", true);
      sendResponse({ ok: false });
      return false;
    }
    jobRunning = true;
    runJob(msg.job)
      .catch((e) => progress("Fatal: " + e.message, "err", true))
      .finally(() => {
        jobRunning = false;
        sendResponse({ ok: true });
      });
    return true; /* async */
  }
});

/* ------------------------------------------------------------------ */
/* tab helpers                                                        */
/* ------------------------------------------------------------------ */

/* Return a live worker tab id, (re)creating the minimized popup window if the
 * previous one was closed or discarded by the browser. */
async function ensureTab() {
  if (workerTabId != null) {
    try {
      const tab = await chrome.tabs.get(workerTabId);
      if (tab) return workerTabId;
    } catch {
      workerTabId = null;
    }
  }
  const win = await chrome.windows.create({
    url: "about:blank",
    type: "popup",
    state: "minimized",
    focused: false,
  });
  workerWindowId = win.id;
  workerTabId = win.tabs[0].id;
  return workerTabId;
}

async function closeWorker() {
  if (workerWindowId != null) {
    try {
      await chrome.windows.remove(workerWindowId);
    } catch {}
  }
  workerWindowId = null;
  workerTabId = null;
}

function sleep(ms) {
  return new Promise((r) => setTimeout(r, ms));
}

/* executeScript on the worker tab, retrying transient frame-swap errors
 * (pahe.win client-side redirects tear the frame out mid-call). */
async function exec(func, args = []) {
  let lastErr;
  for (let attempt = 0; attempt < 4; attempt++) {
    const tabId = await ensureTab();
    try {
      const res = await chrome.scripting.executeScript({
        target: { tabId },
        func,
        args,
      });
      return res && res[0] ? res[0].result : undefined;
    } catch (e) {
      lastErr = e;
      if (!isFrameGoneError(e)) throw e;
      await sleep(500);
    }
  }
  throw lastErr;
}

/* Navigate the worker tab and wait until the document is loaded AND past any
 * Cloudflare "Just a moment…" interstitial. */
async function navigate(url) {
  for (let attempt = 0; attempt < 3; attempt++) {
    const tabId = await ensureTab();
    try {
      await chrome.tabs.update(tabId, { url });
      break;
    } catch (e) {
      if (!isFrameGoneError(e)) throw e;
      workerTabId = null; /* force recreate */
    }
  }
  const deadline = Date.now() + NAV_TIMEOUT_MS;
  await sleep(600);
  while (Date.now() < deadline) {
    let ready = false;
    try {
      ready = await exec(() => {
        const challenge =
          document.title === "Just a moment..." ||
          !!document.querySelector("#challenge-running, #cf-challenge-running");
        return document.readyState === "complete" && !challenge;
      });
    } catch {
      /* page mid-navigation; retry */
    }
    if (ready) return true;
    await sleep(700);
  }
  throw new Error("timed out loading " + url);
}

/* Repeatedly run a predicate in the tab until it returns truthy or times out. */
async function pollExec(func, timeoutMs) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    try {
      const v = await exec(func);
      if (v) return v;
    } catch {}
    await sleep(600);
  }
  return null;
}

/* ------------------------------------------------------------------ */
/* animepahe resolution (runs in a real animepahe tab, same-origin)   */
/* ------------------------------------------------------------------ */

/* Executed IN the animepahe page. Fetches the release API across all pages,
 * filters to the wanted episodes, fetches each play page, and returns the
 * pahe.win download candidates with resolution + audio. */
async function inPageResolveSeries(id, base, want) {
  const eps = [];
  let page = 1;
  let last = 1;
  do {
    const r = await fetch(
      `${base}/api?m=release&id=${id}&sort=episode_asc&page=${page}`,
      { credentials: "include" }
    );
    if (!r.ok) throw new Error("api page " + page + " → " + r.status);
    const j = await r.json();
    last = j.last_page || 1;
    for (const d of j.data || [])
      eps.push({ episode: Number(d.episode), session: d.session });
    page++;
  } while (page <= last);

  const wanted = eps.filter(
    (e) => want.all || (e.episode >= want.start && e.episode <= want.end)
  );

  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  const out = [];
  for (const e of wanted) {
    /* animepahe rate-limits rapid play-page fetches (HTTP 429); throttle and
     * retry with backoff. */
    let pr = null;
    for (let attempt = 0; attempt < 5; attempt++) {
      pr = await fetch(`${base}/play/${id}/${e.session}`, {
        credentials: "include",
      });
      if (pr.status !== 429) break;
      await sleep(1200 * (attempt + 1));
    }
    if (!pr || !pr.ok) {
      out.push({
        episode: e.episode,
        candidates: [],
        error: "play " + (pr ? pr.status : "?"),
      });
      await sleep(400);
      continue;
    }
    const html = (await pr.text()).replace(/\r?\n/g, "");
    out.push({ episode: e.episode, candidates: parsePahe(html) });
    await sleep(400);
  }
  return out;

  function parsePahe(html) {
    const cands = [];
    const re = /<a[^>]+href="(https?:\/\/pahe\.win\/[^"]+)"[^>]*>([\s\S]*?)<\/a>/g;
    let m;
    while ((m = re.exec(html))) {
      const href = m[1].replace(/&amp;/g, "&");
      const label = m[2];
      const resM = label.match(/(\d{3,4})p/);
      const res = resM ? Number(resM[1]) : 0;
      let audio = "jp";
      if (/\beng\b|dub/i.test(label)) audio = "eng";
      else if (/\bchi\b|chinese/i.test(label)) audio = "zh";
      cands.push({ paheUrl: href, res, audio });
    }
    return cands;
  }
}

function pickCandidate(cands, quality, audio) {
  if (!cands || !cands.length) return null;
  let pool = cands.filter((c) => c.audio === audio);
  if (!pool.length) pool = cands;
  if (quality === -1) return pool.reduce((a, b) => (b.res < a.res ? b : a));
  if (quality > 0) {
    const exact = pool.find((c) => c.res === quality);
    if (exact) return exact;
  }
  return pool.reduce((a, b) => (b.res > a.res ? b : a)); /* max / fallback */
}

/* ------------------------------------------------------------------ */
/* pahe.win → kwik, kwik → download                                   */
/* ------------------------------------------------------------------ */

async function paheToKwik(paheUrl) {
  /* kwik rotates TLDs like animepahe (kwik.cx, kwik.si, …); match any. */
  const rx = /https?:\/\/kwik\.\w+\/[efd]\/[\w-]+/;
  for (let attempt = 0; attempt < 2; attempt++) {
    await navigate(paheUrl);
    const info = await exec(() => ({
      url: location.href,
      html: document.documentElement.outerHTML,
    }));
    const kwik = (info.url.match(rx) || info.html.match(rx) || [])[0];
    if (kwik) return kwik.replace(/\/[ed]\//, "/f/");
    await sleep(800);
  }
  return null;
}

/* Resolve the kwik /f/ page to its final CDN URL, then download it via the
 * extension (chrome.downloads). Extension-initiated downloads are NOT subject
 * to Chrome's "site wants to download multiple files" block that kills batch
 * downloads when triggered from the page, and we get direct filename control. */
async function kwikDownload(kwikUrl, filename) {
  await navigate(kwikUrl);

  /* Wait for kwik's own JS to build the download form (input[name=_token]). */
  const hasForm = await pollExec(
    () =>
      ![...document.querySelectorAll("form")].every(
        (f) => !f.querySelector('input[name="_token"]')
      ),
    FORM_TIMEOUT_MS
  );
  if (!hasForm) throw new Error("kwik download form never appeared");

  /* Submit kwik's form so the BROWSER performs the download navigation. This
   * sends Referer: kwik.cx, which the CDN's Cloudflare hotlink protection
   * requires — chrome.downloads.download() can't set a referer and gets a 403
   * HTML error page instead. Chrome's multiple-download block is lifted for
   * kwik via contentSettings (see runJob). */
  pendingFilename = filename;
  const created = waitForDownloadCreated(DOWNLOAD_WAIT_MS);
  await exec(() => {
    const f = [...document.querySelectorAll("form")].find((x) =>
      x.querySelector('input[name="_token"]')
    );
    if (f) f.submit();
  });
  const item = await created;
  if (!item) {
    pendingFilename = null;
    throw new Error("download did not start");
  }
  return item;
}

/* ------------------------------------------------------------------ */
/* job orchestration                                                  */
/* ------------------------------------------------------------------ */

function parseEpisodes(spec) {
  if (!spec || spec === "all") return { all: true };
  const single = spec.match(/^(\d+)$/);
  if (single) return { all: false, start: +single[1], end: +single[1] };
  const range = spec.match(/^(\d+)-(\d+)$/);
  if (range) return { all: false, start: +range[1], end: +range[2] };
  throw new Error("bad episode spec: " + spec);
}

function sanitize(name) {
  return name.replace(/[<>:"/\\|?*\x00-\x1F]/g, "_").replace(/[. ]+$/, "");
}

async function runJob(job) {
  const want = parseEpisodes(job.episodes);
  const base = (job.url.match(/^https:\/\/animepahe\.[a-z]+/) || [])[0];
  if (!base) throw new Error("not an animepahe URL");

  /* Lift Chrome's "site wants to download multiple files" block for kwik so a
   * batch of form-submit downloads all go through. */
  for (const pattern of ["https://kwik.cx/*", "https://*.kwik.cx/*", "https://kwik.si/*", "https://*.kwik.si/*"]) {
    try {
      await chrome.contentSettings.automaticDownloads.set({
        primaryPattern: pattern,
        setting: "allow",
      });
    } catch {}
  }

  await ensureTab();

  try {
    progress("Opening animepahe (solving Cloudflare)…", "dim");
    await navigate(job.url);

    /* Series vs single episode. */
    let seriesTitle = "animepahe";
    let resolved;

    const animeM = job.url.match(/anime\/([a-f0-9-]{36})/);
    const playM = job.url.match(/play\/([a-f0-9-]{36})\/([a-f0-9]{64})/);

    if (animeM) {
      seriesTitle = await exec(() => {
        const el = document.querySelector(".title-wrapper h1, h1");
        return el ? el.textContent.trim() : document.title;
      });
      progress("Resolving episode list…", "dim");
      resolved = await exec(inPageResolveSeries, [animeM[1], base, want]);
      if (!resolved) throw new Error("resolver returned nothing (page blocked?)");
    } else if (playM) {
      progress("Resolving episode…", "dim");
      const html = await exec(() => document.documentElement.outerHTML);
      const cands = [];
      const re = /<a[^>]+href="(https?:\/\/pahe\.win\/[^"]+)"[^>]*>([\s\S]*?)<\/a>/g;
      let m;
      while ((m = re.exec(html))) {
        const label = m[2];
        const resM = label.match(/(\d{3,4})p/);
        let audio = "jp";
        if (/\beng\b|dub/i.test(label)) audio = "eng";
        else if (/\bchi\b|chinese/i.test(label)) audio = "zh";
        cands.push({
          paheUrl: m[1].replace(/&amp;/g, "&"),
          res: resM ? Number(resM[1]) : 0,
          audio,
        });
      }
      resolved = [{ episode: 1, candidates: cands }];
    } else {
      throw new Error("URL must be an /anime/ or /play/ link");
    }

    const dirName = sanitize(seriesTitle);
    const targets = resolved
      .map((r) => ({
        episode: r.episode,
        pick: pickCandidate(r.candidates, job.quality, job.audio),
        error: r.error,
      }))
      .filter((t) => {
        if (!t.pick) {
          progress(`EP${t.episode}: no source (${t.error || "no candidates"})`, "err");
          return false;
        }
        return true;
      });

    if (!targets.length) throw new Error("nothing to download");
    progress(`${targets.length} episode(s) to fetch.`, "ok");

    let done = 0;
    for (const t of targets) {
      const tag = "EP" + String(t.episode).padStart(2, "0");
      try {
        progress(`${tag}: pahe.win → kwik…`, "dim");
        const kwik = await paheToKwik(t.pick.paheUrl);
        if (!kwik) {
          progress(`${tag}: no kwik link found`, "err");
          continue;
        }
        progress(`${tag}: kwik → download (${t.pick.res || "?"}p)…`, "dim");
        const filename = `${dirName}/${dirName}_${tag}_${t.pick.res || 0}p.mp4`;
        await kwikDownload(kwik, filename);
        done++;
        progress(`${tag}: downloading ✓`, "ok");
      } catch (e) {
        progress(`${tag}: ${e.message}`, "err");
      }
    }

    progress(
      `Done — ${done}/${targets.length} started. Check your Downloads/${dirName} folder.`,
      done ? "ok" : "err",
      true
    );
  } finally {
    await closeWorker();
  }
}

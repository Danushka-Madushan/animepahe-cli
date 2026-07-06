const $ = (id) => document.getElementById(id);
const logEl = $("log");
const goBtn = $("go");

function line(msg, cls) {
  const span = document.createElement("div");
  if (cls) span.className = cls;
  span.textContent = msg;
  logEl.appendChild(span);
  logEl.scrollTop = logEl.scrollHeight;
}

/* Progress messages streamed from the background worker while a job runs. */
chrome.runtime.onMessage.addListener((msg) => {
  if (msg.type !== "progress") return;
  line(msg.text, msg.level);
  if (msg.done) {
    goBtn.disabled = false;
    goBtn.textContent = "Download";
  }
});

goBtn.addEventListener("click", async () => {
  const url = $("url").value.trim();
  if (!url) {
    line("Enter an anime URL first.", "err");
    return;
  }
  logEl.innerHTML = "";
  goBtn.disabled = true;
  goBtn.textContent = "Working…";

  const job = {
    url,
    episodes: $("episodes").value.trim() || "all",
    quality: parseInt($("quality").value, 10),
    audio: $("audio").value,
  };

  /* Persist inputs so re-opening the popup remembers them. */
  chrome.storage.local.set({ lastJob: job });

  try {
    await chrome.runtime.sendMessage({ type: "start", job });
  } catch (e) {
    line("Failed to start: " + e.message, "err");
    goBtn.disabled = false;
    goBtn.textContent = "Download";
  }
});

/* Restore last inputs. */
chrome.storage.local.get("lastJob").then(({ lastJob }) => {
  if (!lastJob) return;
  $("url").value = lastJob.url || "";
  $("episodes").value = lastJob.episodes || "all";
  if (lastJob.quality != null) $("quality").value = String(lastJob.quality);
  if (lastJob.audio) $("audio").value = lastJob.audio;
});

/* Reading Room frontend. Talks to the Flask API only; no analysis happens here. */
const PALETTE = ["#a4452c","#2f5d50","#c08a2b","#2b3a67","#6b3a5b","#7a7a2e","#3d7f9a","#8c5a3c","#4f6f3a","#b5707a","#5c4b99","#9a6b00","#35605f","#8a2f4d","#667085"];
const NOISE = "#8d8d8d";
const $ = (s) => document.querySelector(s);
const esc = (s) => String(s ?? "").replace(/[&<>"']/g, (c) => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]));
const col = (l) => (l === -1 ? NOISE : PALETTE[l % PALETTE.length]);
const fmt = (v, d = 2) => (v == null ? "n/a" : Number(v).toFixed(d));
const api = () => $("#api").value.replace(/\/$/, "");

const S = { R: null, docs: [], labels: [], method: "hierarchical", sel: null, hits: new Set(), pending: null };

const CAPTIONS = {
  "01_missing_values.svg": "<b>What was missing.</b> Catalogue fields that were empty before cleaning. Gaps like these are why each field needs an explicit rule (median fill, 'Unknown' category, fall back to title).",
  "02_pca_scree.svg": "<b>How much two axes can show.</b> PCA is used here for viewing. The low share on the first two bars means flat pictures hide structure, so overlap in the scatter plots does not always mean overlap in the data.",
  "03_dendrograms.svg": "<b>Four ways to build the tree.</b> Ward builds even branches; single linkage chains documents together one at a time. The dashed line is where the tree was cut.",
  "04_silhouette_vs_k.svg": "<b>Choosing the number of groups.</b> Higher silhouette means tighter, better separated groups. A curve that keeps rising with k is a warning that the data has no single clean answer.",
  "05_pca_hierarchical.svg": "<b>Same documents, four groupings.</b> Colours move between panels: this is linkage sensitivity.",
  "06_k_distance.svg": "<b>Picking DBSCAN's radius.</b> Sorted distance to each document's MinPts-th neighbour. The bend in each curve is where documents stop being dense neighbours.",
  "07_dbscan_sensitivity.svg": "<b>Parameter sensitivity.</b> A small nudge to epsilon swings the number of groups and the share of noise. Bright, flat regions are stable choices; steep colour changes are fragile.",
  "08_pca_dbscan.svg": "<b>Tight, chosen, loose.</b> Smaller epsilon turns much of the collection into noise; larger epsilon merges distinct topics into one blob.",
  "09_silhouette_hier.svg": "<b>Silhouette per document (hierarchical).</b> Bars that dip below zero are documents closer to another group than to their own.",
  "10_silhouette_dbscan.svg": "<b>Silhouette per document (DBSCAN).</b> Noise points are excluded, which flatters the score. Read it with the noise percentage.",
  "11_stability.svg": "<b>Would the answer survive a different sample?</b> Each method was re-run on 80% subsamples. Taller bars mean the grouping is repeatable.",
  "12_method_comparison.svg": "<b>Side by side.</b> Silhouette, stability and smallest group size. Tiny groups usually mean chaining or over-splitting.",
  "13_contingency_hier.svg": "<b>Sanity check against catalogue subjects.</b> The subject column was not used for clustering. Clean blocks mean the text really does carry topic.",
  "14_contingency_dbscan.svg": "<b>The same check for DBSCAN.</b> Unclassifiable records should land in the noise row.",
  "15_terms_hier.svg": "<b>What each hierarchical group is about.</b> Terms that most distinguish it from the collection.",
  "16_terms_dbscan.svg": "<b>What each DBSCAN group is about.</b> Compare with the hierarchical terms to see where methods agree."
};

const LIMITS = [
  ["A sample is not the library", "Group counts and shapes will change with more scans. Treat results as a method demonstration on a sample, not as a map of the whole collection."],
  ["Excerpts, not books", "Titles and short OCR excerpts carry thin signal. Catalogue wording can dominate real content."],
  ["OCR and language", "Damaged text clusters with other damaged text. English stop-word handling serves other languages poorly, so Hindi, Punjabi, Persian or Sanskrit records may group by transliteration habit."],
  ["Silhouette is not truth", "It favours compact round groups and cannot tell a historian whether a group is meaningful."],
  ["Whose heritage is visible?", "Digitisation and cataloguing priorities favour some languages and communities. Clusters inherit that skew, and colonial-era administrative vocabulary can be repeated uncritically."],
  ["Noise must not mean deletion", "Small collections and minority-language material look like noise to density methods. Noise goes to a human, never into a discard pile."],
  ["Suggestions, not verdicts", "Output is a proposal for librarians. No scan should be hidden or ranked lower because of its cluster label."]
];

async function get(path) {
  const r = await fetch(api() + path);
  if (!r.ok) throw new Error(path + " " + r.status);
  return r.json();
}

function banner(msg) {
  const b = $("#banner");
  b.hidden = !msg;
  b.textContent = msg || "";
}

async function init() {
  try {
    S.R = await get("/api/analysis");
    S.docs = await get("/api/documents");
    banner("");
  } catch (e) {
    banner("Cannot reach the analysis backend at " + api() + ". Start it with: cd backend && python api.py (then reload).");
    return;
  }
  const R = S.R;
  const f = R.features;
  $("#corpus-note").textContent = `Current corpus: ${R.meta.n_docs} scans from ${R.meta.csv}. ` +
    `${f.missing.filter(m => m.missing).map(m => `${m.field}: ${m.missing} missing`).join(", ")}.`;
  S.method = "hierarchical";
  const bl = R.best_hierarchical;
  $("#c-link").value = bl; $("#c-k").value = R.hierarchical[bl].best_k; $("#o-k").textContent = R.hierarchical[bl].best_k;
  const b = R.dbscan.best; $("#c-eps").value = b.eps.toFixed(2); $("#o-eps").textContent = b.eps.toFixed(2);
  $("#c-mp").value = b.min_pts; $("#o-mp").textContent = b.min_pts;
  renderShelves("hierarchical");
  renderCompare();
  renderNoise();
  renderLimits();
  await recluster();
}

function clusterName(c) { return c.top_terms.slice(0, 3).join(" · "); }

function renderShelves(mode) {
  const R = S.R, list = R.clusters[mode];
  const grid = $("#shelf-grid"); grid.innerHTML = "";
  $("#shelf-summary").textContent = mode === "hierarchical"
    ? `Linkage: ${R.best_hierarchical} (highest silhouette, ${fmt(R.hierarchical[R.best_hierarchical].silhouette)}). Every document is assigned to a shelf.`
    : `eps ${fmt(R.dbscan.best.eps, 3)}, MinPts ${R.dbscan.best.min_pts}. ${(R.dbscan.best.noise_frac * 100).toFixed(0)}% of documents are left unplaced (see Review desk).`;
  list.slice().sort((a, b) => b.size - a.size).forEach((c) => {
    const el = document.createElement("article");
    el.className = "card"; el.style.setProperty("--c", col(c.id));
    el.innerHTML = `<h3>Shelf ${c.id + 1}</h3>
      <div class="meta">${c.size} scans · mostly ${esc(c.main_language)} (${Math.round(c.language_share * 100)}%) · median year ${c.median_year ? Math.round(c.median_year) : "unknown"} · fit ${fmt(c.mean_silhouette)}</div>
      <div class="terms">${esc(c.top_terms.slice(0, 6).join(", "))}</div>
      <ul>${c.titles.slice(0, 4).map((t) => `<li>${esc(t)}</li>`).join("")}</ul>`;
    grid.appendChild(el);
  });
}

function renderCompare() {
  const R = S.R;
  const rows = R.summary.map((s) => `<tr><td>${esc(s.method)}</td><td class="n">${s.clusters}</td><td class="n">${(s.noise * 100).toFixed(0)}%</td>
    <td class="n">${fmt(s.silhouette)}</td><td class="n">${fmt(s.stability)}</td><td class="n">${s.smallest} / ${s.largest}</td></tr>`).join("");
  $("#cmp-table").innerHTML = `<table><thead><tr><th>Method</th><th>Groups</th><th>Noise</th><th>Silhouette</th><th>Stability (ARI)</th><th>Smallest / largest</th></tr></thead><tbody>${rows}</tbody></table>`;
  $("#fig-list").innerHTML = R.figures.map((n) =>
    `<figure><img loading="lazy" src="${api()}/api/figures/${esc(n)}" alt="${esc(n)}"><figcaption>${CAPTIONS[n] || ""}</figcaption></figure>`).join("");
}

function renderNoise() {
  const el = $("#noise-list");
  if (!S.R.noise.length) { el.innerHTML = "<p class='muted'>No documents were left unplaced at the selected setting.</p>"; return; }
  el.innerHTML = S.R.noise.map((n) => {
    const why = n.missing_description ? "No description at all; only the title to go on."
      : (n.text || "").length < 45 ? "Very little text to judge from." : "Not near enough to other scans to join a group.";
    return `<article><h4>${esc(n.title)}</h4><p>${esc((n.text || "(no text)").slice(0, 110))}</p><p class="why">${why}</p></article>`;
  }).join("");
}

function renderLimits() {
  $("#limits-body").innerHTML = LIMITS.map(([h, t]) => `<p><b>${esc(h)}.</b> ${esc(t)}</p>`).join("");
}

/* ---------- explorer ---------- */
async function recluster() {
  const body = S.method === "hierarchical"
    ? { method: "hierarchical", linkage: $("#c-link").value, k: +$("#c-k").value }
    : { method: "dbscan", eps: +$("#c-eps").value, min_pts: +$("#c-mp").value };
  const stamp = (S.pending = Math.random());
  const r = await fetch(api() + "/api/cluster", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) });
  const data = await r.json();
  if (stamp !== S.pending) return;           // a newer request superseded this one
  if (!r.ok) { $("#live").textContent = data.error; return; }
  S.labels = data.labels; S.last = data;
  $("#live").innerHTML = `<b>${data.n_clusters}</b> groups · <b>${(data.noise_frac * 100).toFixed(0)}%</b> noise<br>silhouette <b>${fmt(data.silhouette)}</b>` +
    (S.method === "dbscan" ? `<br><span class="muted">computed on placed documents only</span>` : "");
  drawPlot();
}

function drawPlot() {
  const svg = $("#plot"), W = 800, H = 540, pad = 28;
  const xs = S.docs.map((d) => d.x), ys = S.docs.map((d) => d.y);
  const [x0, x1, y0, y1] = [Math.min(...xs), Math.max(...xs), Math.min(...ys), Math.max(...ys)];
  const px = (x) => pad + ((x - x0) / (x1 - x0 || 1)) * (W - 2 * pad);
  const py = (y) => H - pad - ((y - y0) / (y1 - y0 || 1)) * (H - 2 * pad);
  let out = "";
  S.docs.forEach((d, i) => {
    const l = S.labels[i], cx = px(d.x), cy = py(d.y);
    const cls = (S.sel === i ? "sel " : "") + (S.hits.has(i) ? "hit" : "");
    if (l === -1) {
      out += `<path data-i="${i}" class="${cls}" d="M${cx - 4.5} ${cy - 4.5}L${cx + 4.5} ${cy + 4.5}M${cx - 4.5} ${cy + 4.5}L${cx + 4.5} ${cy - 4.5}" stroke="${NOISE}" stroke-width="1.6" fill="none" style="cursor:pointer"/>`;
    } else {
      out += `<circle data-i="${i}" class="${cls}" cx="${cx}" cy="${cy}" r="${S.hits.has(i) ? 8 : 5.5}" fill="${col(l)}" fill-opacity=".85"/>`;
    }
  });
  svg.innerHTML = out;
  const ids = [...new Set(S.labels)].sort((a, b) => a - b);
  const names = {};
  (S.last?.clusters || []).forEach((c) => (names[c.id] = c.top_terms.slice(0, 2).join(", ")));
  $("#legend").innerHTML = ids.map((l) =>
    `<span><i style="background:${col(l)}"></i>${l === -1 ? "noise" : esc(names[l] || "group " + (l + 1))}</span>`).join("");
}

function showDetail(i) {
  const d = S.docs[i]; S.sel = i;
  const best = S.R.best_hierarchical;
  $("#detail").innerHTML = `<h4>${esc(d.title)}</h4>
    <div class="ocr">${esc(d.description || "(no description in catalogue)")}</div>
    <dl><dt>Language</dt><dd>${esc(d.language || "unknown")}</dd><dt>Year</dt><dd>${d.year ? Math.round(d.year) : "unknown"}</dd>
    <dt>Pages</dt><dd>${d.pages ? Math.round(d.pages) : "unknown"}</dd><dt>Collection</dt><dd>${esc(d.collection || "unknown")}</dd>
    <dt>Catalogue subject</dt><dd>${esc(d.reference_topic || "none")}</dd>
    <dt>Current label</dt><dd>${S.labels[i] === -1 ? "noise" : "group " + (S.labels[i] + 1)}</dd><dt>Record</dt><dd>${esc(d.doc_id)}</dd></dl>`;
  drawPlot();
}

function wire() {
  $("#shelf-toggle").addEventListener("click", (e) => {
    const m = e.target.dataset.mode; if (!m || !S.R) return;
    document.querySelectorAll("#shelf-toggle button").forEach((b) => b.classList.toggle("on", b === e.target));
    renderShelves(m);
  });
  $("#c-method").addEventListener("change", (e) => {
    S.method = e.target.value;
    $("#h-ctl").hidden = S.method !== "hierarchical"; $("#d-ctl").hidden = S.method !== "dbscan";
    recluster();
  });
  let t;
  const deb = () => { clearTimeout(t); t = setTimeout(recluster, 120); };
  [["#c-k", "#o-k", 0], ["#c-eps", "#o-eps", 2], ["#c-mp", "#o-mp", 0]].forEach(([i, o, d]) =>
    $(i).addEventListener("input", (e) => { $(o).textContent = (+e.target.value).toFixed(d); deb(); }));
  $("#c-link").addEventListener("change", recluster);
  const plot = $("#plot"), tip = $("#tip");
  plot.addEventListener("mousemove", (e) => {
    const i = e.target.dataset?.i;
    if (i === undefined) { tip.hidden = true; return; }
    const box = plot.parentElement.getBoundingClientRect();
    tip.hidden = false; tip.textContent = S.docs[i].title;
    tip.style.left = Math.min(e.clientX - box.left + 12, box.width - 200) + "px"; tip.style.top = e.clientY - box.top + 12 + "px";
  });
  plot.addEventListener("mouseleave", () => (tip.hidden = true));
  plot.addEventListener("click", (e) => { const i = e.target.dataset?.i; if (i !== undefined) showDetail(+i); });
  let st;
  $("#q").addEventListener("input", (e) => {
    clearTimeout(st);
    st = setTimeout(async () => {
      const q = e.target.value.trim(); S.hits = new Set();
      if (q.length < 3) { $("#hits").innerHTML = ""; drawPlot(); return; }
      const res = await get("/api/search?q=" + encodeURIComponent(q));
      const idx = new Map(S.docs.map((d, i) => [d.doc_id, i]));
      res.forEach((r) => S.hits.add(idx.get(r.doc_id)));
      $("#hits").innerHTML = res.map((r) => `<li data-i="${idx.get(r.doc_id)}">${esc(r.title)}</li>`).join("") || "<li class='muted'>No close matches.</li>";
      drawPlot();
    }, 250);
  });
  $("#hits").addEventListener("click", (e) => { const i = e.target.dataset?.i; if (i !== undefined) showDetail(+i); });
  $("#upload").addEventListener("submit", async (e) => {
    e.preventDefault();
    const file = $("#file").files[0]; if (!file) { $("#up-msg").textContent = "Choose a CSV first."; return; }
    const fd = new FormData(); fd.append("file", file);
    $("#up-msg").textContent = "Analysing… this can take up to a minute.";
    const r = await fetch(api() + "/api/upload", { method: "POST", body: fd });
    const j = await r.json();
    $("#up-msg").textContent = r.ok ? `Done: ${j.n_docs} documents analysed.` : j.error;
    if (r.ok) { S.sel = null; S.hits = new Set(); await init(); }
  });
  $("#api").addEventListener("change", init);
}

wire();
init();

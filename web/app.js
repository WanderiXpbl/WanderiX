// WanderiX web page. All planning happens in C++ (server.cpp + Planner.h);
// this file only sends the form to the server and shows what comes back.

let lastState = null;   // last plan sent by the server
let heapIndex = 0;      // which max-heap step is shown
let playTimer = null;
let graph = null;       // nodes and roads for the map (from /api/graph)
let mapFrom = null;     // first place clicked on the map
let mapDist = null;     // fastest minutes from mapFrom to every node
let mapPath = [];       // fastest road between the two clicked places

const $ = (id) => document.getElementById(id);

// a random id for this browser, so the server keeps a separate plan for every visitor
const visitorId = (() => {
  let id = null;
  try { id = localStorage.getItem("wanderixId"); } catch (e) { /* storage blocked: use a new id */ }
  if (!id) {
    id = window.crypto && crypto.randomUUID ? crypto.randomUUID() : Date.now().toString(36) + Math.random().toString(36).slice(2);
    try { localStorage.setItem("wanderixId", id); } catch (e) { /* fine, the id lasts until reload */ }
  }
  return id;
})();

// ---------- talking to the C++ server ----------

async function api(path, params = {}) {
  const url = path + "?" + new URLSearchParams({ ...params, sid: visitorId }).toString();
  let response;
  try {
    response = await fetch(url);
  } catch (e) {
    throw new Error("Can't reach the WanderiX server. Check your connection and reload the page.");
  }
  const data = await response.json();
  if (!data.ok) throw new Error(data.error || "Something went wrong.");
  return data;
}

function esc(text) {
  return String(text).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
}

function showMessage(text, bad = false) {
  const m = $("message");
  m.textContent = text;
  m.classList.toggle("bad", bad);
  m.hidden = !text;
}

function foodGap(m) {
  return m % 60 === 0 ? m / 60 + " h" : Math.floor(m / 60) + " h " + (m % 60) + " min";
}

function minutes(m) {
  return m >= 60 ? Math.floor(m / 60) + " h " + (m % 60) + " min" : m + " min";
}

// ---------- the form ----------

const weightNames = ["mood", "rating", "cost", "time", "near"];

function updateSliders() {
  let sum = 0;
  for (const w of weightNames) {
    $("o_" + w).textContent = $("w_" + w).value;
    sum += Number($("w_" + w).value);
  }
  $("o_repeat").textContent = $("w_repeat").value;
  $("sliderNote").textContent = sum === 0
    ? "Move at least one slider above 0."
    : "These add up to " + sum + "; they are scaled to 100% when you plan.";
}

function readForm() {
  const budget = Number($("budget").value);
  const start = $("start").value;
  const end = $("end").value;
  if ($("budget").value === "" || budget < 0 || budget > 100000) return { error: "Enter a budget between 0 and 100000." };
  if (!start || !end) return { error: "Enter a start and an end time." };
  if (end <= start) return { error: "End time must be after the start time." };

  const params = {
    budget: Math.round(budget),
    day: $("day").value,
    start: start,
    end: end,
    mood: $("mood").value,
    style: $("style").value,
    startPlace: $("startPlace").value,
    returnHome: $("returnHome").checked ? 1 : 0,
    mealGap: $("mealGap").value,
  };

  if (params.style === "5") {
    let sum = 0;
    for (const w of weightNames) sum += Number($("w_" + w).value);
    if (sum === 0) return { error: "Move at least one weight slider above 0." };
    for (const w of weightNames) params["w_" + w] = (Number($("w_" + w).value) / sum).toFixed(3);
    params.w_repeat = (Number($("w_repeat").value) / 100).toFixed(2);
  }
  return { params };
}

async function makePlan(event) {
  event.preventDefault();
  const form = readForm();
  $("formError").hidden = !form.error;
  if (form.error) {
    $("formError").textContent = form.error;
    return;
  }
  $("planBtn").disabled = true;
  $("planBtn").textContent = "Planning…";
  try {
    render(await api("/api/plan", form.params));
    showTab("plan");
  } catch (e) {
    showMessage(e.message, true);
  } finally {
    $("planBtn").disabled = false;
    $("planBtn").textContent = "Make my plan";
  }
}

// ---------- showing a plan ----------

function render(state) {
  lastState = state;
  showMessage(state.message, state.stops.length === 0);

  const has = state.stops.length > 0;
  $("planEmpty").hidden = has;
  $("planBody").hidden = !has;
  if (!has) {
    $("planEmpty").innerHTML = "<h3>No places fit</h3><p>Try a bigger budget, a longer time window, another day or another mood.</p>";
  }

  renderSummary(state);
  renderInside(state);
  renderTimeline(state);
  $("undoBtn").disabled = !state.canUndo;
  $("redoBtn").disabled = !state.canRedo;

  heapIndex = 0;
  renderExplain(state);
  renderMap();
}

// where each data structure was used for this plan (real numbers from the C++ side)
function renderInside(state) {
  const st = state.stats;
  const box = (value, label, code) => `<div><b>${value}</b><span>${label}</span><br><code>${code}</code></div>`;
  $("inside").innerHTML =
      box(st.dijkstraRuns, "Dijkstra runs at start-up (one per node)", "Graph.h · min-heap")
    + box(st.steps + " steps", st.compared + " places compared in the max-heap", "Route.h · max-heap loop")
    + box(state.explain.steps.reduce((n, s) => n + s.skipped.length, 0), "times a place was skipped for breaking a rule", "Route.h · rule checks")
    + box(st.listLength + " nodes", "stops in the itinerary", "Itinerary.h · linked list")
    + box(st.undoSize + " / " + st.redoSize, "changes on the undo / redo stacks", "Planner.h · two stacks");
}

function renderSummary(state) {
  const t = state.totals;
  const trip = state.trip;
  const stat = (label, value) => `<div class="stat"><span>${label}</span><b>${value}</b></div>`;
  let html = stat("Cost", "Rs." + t.cost + " / " + t.budget)
           + stat("Travel", minutes(t.travel))
           + stat("Visiting", minutes(t.visit))
           + stat("Free time", minutes(Math.max(0, t.free)));
  html += `<div class="badges">
      <span class="badge">${esc(trip.day)}, ${esc(trip.start)} – ${esc(trip.end)}</span>
      <span class="badge">Scoring: ${esc(trip.style)}</span>
      <span class="badge">Food every ${foodGap(trip.mealGap)}</span>
      ${trip.edited ? '<span class="badge cache">Edited by you</span>' : ""}
    </div>`;
  $("summary").innerHTML = html;
}

function renderTimeline(state) {
  const trip = state.trip;
  const colors = { Food: "var(--food)", Nature: "var(--nature)", Culture: "var(--culture)", Shopping: "var(--shopping)" };
  let html = `<li><span class="time">${esc(trip.start)}</span><span class="rail"></span>
      <div class="body"><span class="name">Start, ${esc(trip.startName)}</span></div></li>`;

  state.stops.forEach((s, i) => {
    const wait = s.wait > 0 ? ` · <span class="wait">waits ${s.wait} min for opening</span>` : "";
    html += `<li style="--dot:${colors[s.category]}">
      <span class="time">${esc(s.start)}</span><span class="rail"></span>
      <div class="body">
        <span class="name">${i + 1}. ${esc(s.name)}</span><span class="tag ${esc(s.category)}">${esc(s.category)}</span>
        <div class="meta">${s.rating}★ · Rs.${s.cost} · ${s.travel} min travel · ${s.visit} min visit, leave ${esc(s.leave)}${wait}</div>
        <div class="stopActions">
          <button type="button" data-why="${s.id}" aria-expanded="false">Why this place?</button>
          <button type="button" data-remove="${s.id}">Remove</button>
        </div>
        <div class="card-why" id="why-${s.id}" hidden></div>
      </div></li>`;
  });

  const t = state.totals;
  const endText = trip.returnHome
    ? `Back at ${esc(trip.startName)} (${t.back} min travel)`
    : "Trip ends";
  html += `<li class="ends"><span class="time">${esc(t.finish)}</span><span class="rail"></span>
      <div class="body"><span class="name">${endText}</span>
      <div class="meta">${minutes(Math.max(0, t.free))} free before ${esc(trip.end)}</div></div></li>`;
  $("timeline").innerHTML = html;
}

function scoreCardHtml(card, weights) {
  const parts = [["Mood", "mood"], ["Rating", "rating"], ["Low cost", "cost"], ["Short visit", "time"], ["Nearby", "near"]];
  let html = "";
  for (const [label, key] of parts) {
    const w = weights[key];
    const share = w > 0 ? Math.max(0, Math.min(1, card[key] / w)) : 0;
    html += `<div class="bar"><span>${label} (${Math.round(w * 100)}%)</span>
      <span class="track"><span class="fill" style="width:${(share * 100).toFixed(0)}%"></span></span>
      <span class="val">${card[key] < 0 ? "" : "+"}${card[key].toFixed(2)}</span></div>`;
  }
  if (card.bonus > 0) html += `<div class="bar"><span>Explorer bonus</span><span></span><span class="val">+${card.bonus.toFixed(2)}</span></div>`;
  html += `<div class="bar"><b>Score</b><span></span><b class="val">${card.total.toFixed(2)}</b></div>`;
  return html;
}

function toggleWhy(id, button) {
  const box = $("why-" + id);
  if (!box.hidden) {
    box.hidden = true;
    button.setAttribute("aria-expanded", "false");
    return;
  }
  const card = lastState.explain.scores.find((c) => c.id === id);
  box.innerHTML = card
    ? scoreCardHtml(card, lastState.weights) + '<p class="hint">Points before any repeat penalty. Each bar shows how much of that part it earned.</p>'
    : '<p class="hint">You added this place yourself, so it was not scored for this trip.</p>';
  box.hidden = false;
  button.setAttribute("aria-expanded", "true");
}

async function edit(path, params = {}) {
  try {
    render(await api(path, params));
  } catch (e) {
    showMessage(e.message, true);
  }
}

// ---------- add a place (trie search) ----------

let searchTimer = null;

function onSearchInput() {
  clearTimeout(searchTimer);
  searchTimer = setTimeout(async () => {
    const q = $("search").value.trim();
    const box = $("suggestions");
    if (!q) { box.hidden = true; return; }
    try {
      const data = await api("/api/search", { q });
      if (data.results.length === 0) {
        box.innerHTML = `<li class="note">No place starts with “${esc(q)}”.</li>`;
      } else {
        box.innerHTML = data.results.map((p) => `<li><button type="button" data-add="${p.id}" ${p.inPlan ? "disabled" : ""}>
            <span>${esc(p.name)} <span class="tag ${esc(p.category)}">${esc(p.category)}</span></span>
            <span>${p.inPlan ? "in plan" : "Rs." + p.cost}</span></button></li>`).join("");
      }
      box.hidden = false;
    } catch (e) {
      showMessage(e.message, true);
    }
  }, 150);
}

// ---------- download ----------

function downloadPlan() {
  if (!lastState) return;
  const s = lastState, t = s.totals;
  const lines = [
    `WanderiX plan: ${s.trip.day}, ${s.trip.start} - ${s.trip.end}`,
    `Scoring: ${s.trip.style}   Food at least every ${foodGap(s.trip.mealGap)}`,
    "",
    `${s.trip.start}  Start - ${s.trip.startName}`,
    ...s.stops.map((x, i) => `${x.start}  ${i + 1}. ${x.name} (${x.category}) - Rs.${x.cost} - ${x.travel} min travel - ${x.visit} min visit`),
    `${t.finish}  ${s.trip.returnHome ? "Back at " + s.trip.startName : "Trip ends"}`,
    "",
    `Total cost: Rs.${t.cost} / Rs.${t.budget}`,
    `Total travel: ${t.travel} min`,
  ];
  const blob = new Blob([lines.join("\n")], { type: "text/plain" });
  const link = document.createElement("a");
  link.href = URL.createObjectURL(blob);
  link.download = "wanderix-plan.txt";
  link.click();
  URL.revokeObjectURL(link.href);
}

// ---------- how it was planned ----------

function renderExplain(state) {
  const ex = state.explain;
  const has = ex.filter.length > 0;
  $("explainEmpty").hidden = has;
  $("explainBody").hidden = !has;
  if (!has) return;

  $("filterList").innerHTML = ex.filter.map((f) => f.kept
    ? `<li><b>${esc(f.name)}</b> · kept</li>`
    : `<li class="dropped"><b>${esc(f.name)}</b> · ${esc(f.reason)}</li>`).join("");

  const w = state.weights;
  $("formulaText").textContent = `score = ${w.mood} × mood + ${w.rating} × rating + ${w.cost} × low cost + `
    + `${w.time} × short visit + ${w.near} × nearby (from where you are now) − ${w.repeat} × (same category already picked)`;

  const cards = [...ex.scores].sort((a, b) => b.total - a.total);
  $("scoreTable").innerHTML = `<tr><th>Place</th><th>Category</th><th class="num">Mood</th><th class="num">Rating</th>
      <th class="num">Cost</th><th class="num">Visit</th><th class="num">Near</th><th class="num">Bonus</th><th class="num">Total</th></tr>`
    + cards.map((c) => `<tr><td>${esc(c.name)}</td><td>${esc(c.category)}</td>
      <td class="num">${c.mood.toFixed(2)}</td><td class="num">${c.rating.toFixed(2)}</td><td class="num">${c.cost.toFixed(2)}</td>
      <td class="num">${c.time.toFixed(2)}</td><td class="num">${c.near.toFixed(2)}</td><td class="num">${c.bonus.toFixed(2)}</td>
      <td class="num"><b>${c.total.toFixed(3)}</b></td></tr>`).join("");

  renderStep();
}

function renderStep() {
  const steps = lastState ? lastState.explain.steps : [];
  if (steps.length === 0) {
    $("heapStep").innerHTML = '<p class="hint">No place followed every rule, so no stop was chosen.</p>';
    $("stepCount").textContent = "";
    return;
  }
  const st = steps[heapIndex];
  const from = heapIndex === 0 ? lastState.trip.startName : steps[heapIndex - 1].name;
  let html = `<div class="result taken">Step ${heapIndex + 1}: at ${esc(from)}, ${esc(st.at)}. ${st.considered} place${st.considered === 1 ? "" : "s"}
      followed every rule. Chose <b>${esc(st.name)}</b> (score ${st.score.toFixed(3)}), visit starts ${esc(st.start)}.</div>`;
  html += `<p class="hint">Top of the max-heap at this step:</p>`;
  html += st.top.map((t) => `<div class="heaprow"><span>${esc(t.name)}</span>
        <span class="track"><span class="fill ${esc(t.category)}" style="width:${Math.max(0, Math.min(100, t.score * 100)).toFixed(0)}%"></span></span>
        <span class="val">${t.score.toFixed(3)}</span></div>`).join("");
  if (st.skipped.length > 0) {
    html += `<p class="hint">Skipped at this step (broke a rule):</p><ul class="skipped">`
      + st.skipped.map((s) => `<li><b>${esc(s.name)}</b>: ${esc(s.reason)}</li>`).join("") + "</ul>";
  }
  $("heapStep").innerHTML = html;
  $("stepCount").textContent = `step ${heapIndex + 1} of ${steps.length}`;
  $("prevStep").disabled = heapIndex === 0;
  $("nextStep").disabled = heapIndex === steps.length - 1;
}

function moveStep(delta) {
  const steps = lastState.explain.steps;
  heapIndex = Math.max(0, Math.min(steps.length - 1, heapIndex + delta));
  renderStep();
}

function togglePlay() {
  if (playTimer) {
    clearInterval(playTimer);
    playTimer = null;
    $("playSteps").textContent = "Play";
    return;
  }
  $("playSteps").textContent = "Pause";
  playTimer = setInterval(() => {
    if (!lastState || heapIndex >= lastState.explain.steps.length - 1) {
      togglePlay();
      return;
    }
    moveStep(1);
  }, 1200);
}

// ---------- all places ----------

async function loadPlaces(sortBy = 1) {
  const data = await api("/api/places", { sort: sortBy });
  $("placesTable").innerHTML = `<tr><th>ID</th><th>Name</th><th>Category</th><th class="num">Rating</th>
      <th class="num">Cost</th><th class="num">Visit</th><th>Open</th><th>Closed</th></tr>`
    + data.places.map((p) => `<tr><td>${p.id}</td><td>${esc(p.name)}</td><td><span class="tag ${esc(p.category)}">${esc(p.category)}</span></td>
      <td class="num">${p.rating}</td><td class="num">Rs.${p.cost}</td><td class="num">${p.visit} min</td>
      <td>${esc(p.open)} – ${esc(p.close)}</td><td>${p.closed === "-" ? "" : esc(p.closed)}</td></tr>`).join("");
  return data.places;
}

// ---------- map: the road graph ----------

const MAP_W = 1000, MAP_H = 660, PAD_X = 120, PAD_Y = 60;   // room for names at the edges

function mapPoint(node, box) {
  const sx = (MAP_W - 2 * PAD_X) / (box.maxX - box.minX || 1);
  const sy = (MAP_H - 2 * PAD_Y) / (box.maxY - box.minY || 1);
  return { x: PAD_X + (node.x - box.minX) * sx, y: MAP_H - PAD_Y - (node.y - box.minY) * sy };   // y goes up on the map
}

function roadKey(a, b) {
  return a < b ? a + "-" + b : b + "-" + a;
}

// roads used by a list of node ids, e.g. [0, 13, 10] -> "0-13", "10-13"
function pathRoads(path, set) {
  for (let i = 1; i < path.length; i++) set.add(roadKey(path[i - 1], path[i]));
}

function renderMap() {
  const svg = $("mapSvg");
  if (!graph || graph.nodes.length === 0) {
    svg.innerHTML = `<text x="40" y="60" class="nlabel">No map data (data/coords.txt is missing).</text>`;
    return;
  }
  const box = {
    minX: Math.min(...graph.nodes.map((n) => n.x)), maxX: Math.max(...graph.nodes.map((n) => n.x)),
    minY: Math.min(...graph.nodes.map((n) => n.y)), maxY: Math.max(...graph.nodes.map((n) => n.y)),
  };
  const pos = {};
  graph.nodes.forEach((n) => { pos[n.id] = mapPoint(n, box); });

  // which roads to highlight
  const routeRoads = new Set();
  const stopNumber = {};
  if (lastState && lastState.stops.length > 0) {
    lastState.stops.forEach((s, i) => { pathRoads(s.path, routeRoads); stopNumber[s.id] = i + 1; });
    pathRoads(lastState.totals.backPath, routeRoads);
  }
  const pickedRoads = new Set();
  pathRoads(mapPath, pickedRoads);

  const colors = { Home: "#14233a", Food: "#e07a2e", Nature: "#0e8a7a", Culture: "#2f5d9a", Shopping: "#8a5bb5" };
  let html = "";
  for (const r of graph.roads) {
    if (!pos[r.a] || !pos[r.b]) continue;
    const key = roadKey(r.a, r.b);
    const cls = pickedRoads.has(key) ? "road sel" : routeRoads.has(key) ? "road on" : "road";
    html += `<line class="${cls}" x1="${pos[r.a].x}" y1="${pos[r.a].y}" x2="${pos[r.b].x}" y2="${pos[r.b].y}"></line>`;
  }
  // minutes only on the roads that matter (your route, the road you picked), so the map stays readable
  for (const r of graph.roads) {
    if (!pos[r.a] || !pos[r.b]) continue;
    const key = roadKey(r.a, r.b);
    if (!routeRoads.has(key) && !pickedRoads.has(key)) continue;
    const mx = (pos[r.a].x + pos[r.b].x) / 2, my = (pos[r.a].y + pos[r.b].y) / 2;
    html += `<rect class="minsBg" x="${mx - 11}" y="${my - 9}" width="22" height="17" rx="4"></rect>
             <text class="mins" x="${mx}" y="${my + 4}" text-anchor="middle">${r.minutes}</text>`;
  }
  // names only for Home, your stops and the places on the road you picked; hover any dot for its name
  for (const n of graph.nodes) {
    const p = pos[n.id];
    const picked = n.id === mapFrom || (mapPath.length > 0 && n.id === mapPath[mapPath.length - 1]);
    const showName = n.id === 0 || picked;   // stops show a number; names are listed under the map
    const big = n.id === 0 || stopNumber[n.id] || picked;
    const r = big ? 15 : 10;
    const shape = n.id === 0
      ? `<rect x="${p.x - r}" y="${p.y - r}" width="${2 * r}" height="${2 * r}" rx="6" fill="${colors.Home}"></rect>`
      : `<circle cx="${p.x}" cy="${p.y}" r="${r}" fill="${colors[n.category]}"></circle>`;
    const num = stopNumber[n.id] ? `<text class="num" x="${p.x}" y="${p.y + 5}" text-anchor="middle">${stopNumber[n.id]}</text>` : "";
    const minsFrom = mapDist && mapDist[n.id] !== undefined && n.id !== mapFrom
      ? (mapDist[n.id] < 0 ? "no road" : mapDist[n.id] + " min") : "";
    const d = minsFrom && showName ? `<text class="dist" x="${p.x}" y="${p.y - r - 7}" text-anchor="middle">${minsFrom}</text>` : "";
    const tip = esc(n.name) + (minsFrom ? " · " + minsFrom + " from " + esc(graph.nodes.find((m) => m.id === mapFrom).name) : "");
    html += `<g class="node ${picked ? "picked" : ""}" data-node="${n.id}" tabindex="0" role="button" aria-label="${esc(n.name)}">
        <title>${tip}</title>${shape}${num}
        ${showName ? `<text class="nlabel" x="${p.x}" y="${p.y + r + 17}" text-anchor="middle">${esc(n.name)}</text>` : ""}${d}</g>`;
  }
  svg.innerHTML = html;

  // numbered list of the route under the map (names would overlap in the busy centre)
  $("mapRoute").innerHTML = lastState && lastState.stops.length > 0
    ? "<b>Your route:</b> " + esc(lastState.trip.startName) + " → "
      + lastState.stops.map((s, i) => `<span class="stopNo">${i + 1}</span> ${esc(s.name)}`).join(" → ")
      + (lastState.trip.returnHome ? " → " + esc(lastState.trip.startName) : "")
    : "";
}

async function clickMapNode(id) {
  const name = graph.nodes.find((n) => n.id === id).name;
  try {
    if (mapFrom === null || mapPath.length > 0 || id === mapFrom) {
      // first click (or start again): distances from this place
      const data = await api("/api/distances", { from: id });
      mapFrom = id;
      mapPath = [];
      mapDist = {};
      data.distances.forEach((d) => { mapDist[d.id] = d.minutes; });
      $("mapStatus").innerHTML = `Fastest times from <b>${esc(name)}</b> (one row of the Dijkstra table) are shown on your stops; hover any dot to see its time. Now click a second place.`;
    } else {
      // second click: the fastest road between the two
      const data = await api("/api/path", { from: mapFrom, to: id });
      mapPath = data.path;
      $("mapStatus").innerHTML = data.reachable
        ? `Fastest road: <b>${data.names.map(esc).join(" → ")}</b> = <b>${data.minutes} min</b>. `
          + (data.path.length > 2 ? "There is no direct road, so Dijkstra goes through other places. " : "")
          + "Click any place to start again."
        : `There is no road from ${esc(graph.nodes.find((n) => n.id === mapFrom).name)} to ${esc(name)}.`;
    }
    renderMap();
  } catch (e) {
    showMessage(e.message, true);
  }
}

// ---------- tabs ----------

function showTab(name) {
  document.querySelectorAll(".tabs button").forEach((b) => b.classList.toggle("active", b.dataset.tab === name));
  document.querySelectorAll(".tab").forEach((t) => { t.hidden = t.id !== "tab-" + name; });
}

// ---------- start-up ----------

async function start() {
  updateSliders();
  try {
    graph = await api("/api/graph");
    renderMap();
  } catch (e) {
    showMessage(e.message, true);
  }
  try {
    const places = await loadPlaces(1);
    const select = $("startPlace");
    const byName = [...places].sort((a, b) => a.name.localeCompare(b.name));
    for (const p of byName) select.add(new Option(p.name, p.id));
  } catch (e) {
    showMessage(e.message, true);
  }
}

$("tripForm").addEventListener("submit", makePlan);
$("style").addEventListener("change", () => { $("sliders").hidden = $("style").value !== "5"; });
document.querySelectorAll(".sliders input").forEach((s) => s.addEventListener("input", updateSliders));
document.querySelectorAll(".tabs button").forEach((b) => b.addEventListener("click", () => showTab(b.dataset.tab)));

$("timeline").addEventListener("click", (e) => {
  const why = e.target.closest("[data-why]");
  const remove = e.target.closest("[data-remove]");
  if (why) toggleWhy(Number(why.dataset.why), why);
  if (remove) edit("/api/remove", { id: remove.dataset.remove });
});
$("undoBtn").addEventListener("click", () => edit("/api/undo"));
$("redoBtn").addEventListener("click", () => edit("/api/redo"));
$("downloadBtn").addEventListener("click", downloadPlan);
$("search").addEventListener("input", onSearchInput);
$("suggestions").addEventListener("click", (e) => {
  const add = e.target.closest("[data-add]");
  if (!add) return;
  $("suggestions").hidden = true;
  $("search").value = "";
  edit("/api/add", { id: add.dataset.add });
});
$("mapSvg").addEventListener("click", (e) => {
  const node = e.target.closest("[data-node]");
  if (node) clickMapNode(Number(node.dataset.node));
});
$("mapSvg").addEventListener("keydown", (e) => {
  const node = e.target.closest("[data-node]");
  if (node && (e.key === "Enter" || e.key === " ")) { e.preventDefault(); clickMapNode(Number(node.dataset.node)); }
});
$("prevStep").addEventListener("click", () => moveStep(-1));
$("nextStep").addEventListener("click", () => moveStep(1));
$("playSteps").addEventListener("click", togglePlay);
$("sortBy").addEventListener("change", () => loadPlaces($("sortBy").value).catch((e) => showMessage(e.message, true)));

start();

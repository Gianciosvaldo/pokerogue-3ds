// Attribute a Node CPU profile of the benchmark to original source files via the bundle's source map.
const fs = require("fs");
const { SourceMapConsumer } = require("/home/claude/pokerogue/node_modules/.pnpm/source-map-js@1.2.1/node_modules/source-map-js");
const prof = JSON.parse(fs.readFileSync(process.argv[2], "utf8"));
const smc = new SourceMapConsumer(JSON.parse(fs.readFileSync(process.argv[3], "utf8")));
const byId = new Map(prof.nodes.map(n => [n.id, n]));
const parent = new Map();
for (const n of prof.nodes) for (const c of n.children ?? []) parent.set(c, n.id);
const cache = new Map();
function loc(n) {
  if (cache.has(n.id)) return cache.get(n.id);
  const cf = n.callFrame;
  let file = cf.url.includes("bench.mjs") ? null : cf.url || cf.functionName;
  let name = cf.functionName || "(anon)";
  if (!file) {
    const p = smc.originalPositionFor({ line: cf.lineNumber + 1, column: cf.columnNumber });
    file = (p.source || "?").replace(/^.*?(node_modules\/\.pnpm\/[^/]+\/node_modules\/)/, "nm/").replace(/^(\.\.\/)+/, "");
    if (p.name && name === "(anon)") name = p.name;
  }
  const r = { file, name, key: `${name} @ ${file}` };
  cache.set(n.id, r);
  return r;
}
function cat(file) {
  if (/^\((garbage|program|idle|root)/.test(file)) return file;
  if (/^(test\/)?(framework|mocks|helpers|utils)\//.test(file) && !file.startsWith("src/")) return "HARNESS upstream test harness + mocks";
  if (/^(shims\/|entry\.ts)/.test(file) || file.includes("bench-host")) return "HARNESS bench shims/driver";
  if (file.startsWith("node:inspector") || file.includes("prof.mjs")) return "PROFILER";
  if (file.includes("nm/phaser3-rex")) return "rex plugins";
  if (file.includes("nm/phaser")) return "Phaser";
  if (file.includes("nm/i18next")) return "i18next";
  if (file.startsWith("nm/")) return "other npm: " + file.split("/")[1];
  if (file.startsWith("src/")) {
    const parts = file.split("/");
    return "game " + (parts[1] === "data" && parts.length > 3 ? parts.slice(1, 3).join("/") : parts[1].replace(/\.ts$/, ""));
  }
  if (file.startsWith("node:") || file === "") return "node internals";
  return "other: " + file;
}
const self = new Map(), selfCat = new Map(), incl = new Map(), inclCat = new Map();
let total = 0;
for (let i = 0; i < prof.samples.length; i++) {
  const dt = prof.timeDeltas[i] ?? 0;
  total += dt;
  let id = prof.samples[i];
  const n = byId.get(id);
  const l = loc(n);
  self.set(l.key, (self.get(l.key) ?? 0) + dt);
  const c = cat(l.file);
  selfCat.set(c, (selfCat.get(c) ?? 0) + dt);
  const seen = new Set(), seenC = new Set();
  while (id !== undefined) {
    const nn = byId.get(id);
    const ll = loc(nn);
    if (!seen.has(ll.key)) { seen.add(ll.key); incl.set(ll.key, (incl.get(ll.key) ?? 0) + dt); }
    const cc = cat(ll.file);
    if (!seenC.has(cc)) { seenC.add(cc); inclCat.set(cc, (inclCat.get(cc) ?? 0) + dt); }
    id = parent.get(id);
  }
}
const pct = x => ((100 * x) / total).toFixed(1).padStart(5) + "%";
const top = (m, n, f = () => true) => [...m].filter(([k]) => f(k)).sort((a, b) => b[1] - a[1]).slice(0, n);
console.log(`total sampled ${(total / 1000).toFixed(0)} ms`);
console.log("\n== SELF time by area");
for (const [k, v] of top(selfCat, 30)) console.log(pct(v), k);
console.log("\n== INCLUSIVE time by area (a sample counts once per area on its stack)");
for (const [k, v] of top(inclCat, 30)) console.log(pct(v), k);
// Who is responsible for Phaser/i18next/other library time: nearest game-or-harness frame above the leaf.
const owner = new Map();
for (let i = 0; i < prof.samples.length; i++) {
  const dt = prof.timeDeltas[i] ?? 0;
  let id = prof.samples[i];
  const leafCat = cat(loc(byId.get(id)).file);
  let own = "(none)";
  while (id !== undefined) {
    const f = loc(byId.get(id)).file;
    const c = cat(f);
    if (c.startsWith("game") || c.startsWith("HARNESS")) { own = c.startsWith("HARNESS") ? c : "game: " + f; break; }
    id = parent.get(id);
  }
  if (!leafCat.startsWith("game") && !leafCat.startsWith("HARNESS")) {
    const k = `${leafCat}  <-  ${own}`;
    owner.set(k, (owner.get(k) ?? 0) + dt);
  }
}
console.log("\n== library time by nearest caller");
for (const [k, v] of top(owner, 30)) console.log(pct(v), k);
console.log("\n== top SELF functions");
for (const [k, v] of top(self, 40)) console.log(pct(v), k);
console.log("\n== top INCLUSIVE game functions");
for (const [k, v] of top(incl, 60, k => k.includes("@ src/"))) console.log(pct(v), k);
// Stacks through Phaser removeListener/destroy: which harness frame leads there.
const via = new Map();
for (let i = 0; i < prof.samples.length; i++) {
  const dt = prof.timeDeltas[i] ?? 0;
  let id = prof.samples[i];
  if (cat(loc(byId.get(id)).file) !== "Phaser") continue;
  const chain = [];
  while (id !== undefined && chain.length < 12) { const l = loc(byId.get(id)); chain.push(`${l.name}@${l.file.split("/").pop()}`); id = parent.get(id); }
  const k = chain.slice(0, 6).join(" < ");
  via.set(k, (via.get(k) ?? 0) + dt);
}
console.log("\n== Phaser stacks");
for (const [k, v] of top(via, 8)) console.log(pct(v), k);

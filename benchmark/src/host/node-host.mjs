// Runs the same bench.mjs under Node, with the same virtual-time loop as host.c.
import { pathToFileURL } from "node:url";
if (process.env.HEAPDIFF) await import("./heapdiff.mjs");
if (process.env.PROF) await import("./prof.mjs");
const realImmediate = setImmediate;
let done = false;
let peak = 0;
globalThis.__host = {
  print: s => console.log(s),
  gc: () => { globalThis.gc?.(); return 0; },
  now: () => performance.now(),
  mem: () => { const u = process.memoryUsage().heapUsed; if (u > peak) peak = u; return { cur: u, peak }; },
  done: () => { done = true; },
  platform: "Node " + process.version,
};
const realLog = console.log.bind(console);
globalThis.__host.print = s => realLog(s);
const perfNow = performance.now.bind(performance);
globalThis.__host.now = perfNow;
for (const k of ["fetch", "Headers", "Response", "navigator", "crypto", "localStorage", "sessionStorage", "Intl", "performance"]) {
  if (k === "Intl" && process.env.KEEP_INTL) continue;
  if (!delete globalThis[k] || k in globalThis) Object.defineProperty(globalThis, k, { value: undefined, writable: true, configurable: true });
}
globalThis.__t0 = perfNow();
if (process.env.DEBUG) globalThis.__debug = true;
if (process.argv[3]) globalThis.__turns = +process.argv[3];
import(pathToFileURL(process.argv[2]).href).catch(e => { realLog("import failed", e); done = true; });
while (!done) {
  await new Promise(r => realImmediate(r));
  if (done) break;
  if (!globalThis.__runTimers?.()) await new Promise(r => realImmediate(r));
}
process.exit(0);

// Reference stream from Phaser itself. Usage: node rng_ref.cjs <pokerogue checkout> <seed> <shift>
const path = require("path");
const R = require(path.join(process.argv[2], "node_modules/phaser/src/math/random-data-generator/RandomDataGenerator.js"));
const seed = process.argv[3] ?? "abc", shift = +(process.argv[4] ?? 0);
let s = ""; for (const ch of seed) s += String.fromCharCode(ch.charCodeAt(0) + shift);
const r = new R([s]);
const st = r.state().split(",").slice(1).map(Number);
const f = x => { let t = x.toPrecision(17); return String(Number(t)) === String(x) ? t : t; };
const out = [st.map(v => v.toPrecision(17)).join(" ")];
const randSeedInt = (range, min) => range <= 1 ? min : r.integerInRange(min, range - 1 + min);
for (let i = 0; i < 2000; i++) {
  if (i % 3 === 0) out.push(String(randSeedInt(100 + i, -i)));
  else if (i % 3 === 1) out.push(r.frac().toPrecision(17));
  else out.push(String(r.integerInRange(85, 100)));
}
console.log(out.join("\n"));

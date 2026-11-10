// Producer for fixtures/dream-anim.txt: upstream's particle loops (src/animations.ts at 8555c08: doSpiralUpwardParticle, doArcDownParticle,
// doCircleInwardParticle) and doCycle's duration arithmetic, copied statement for statement with the Phaser objects replaced by plain
// {x, y, scale}. The C test (dream_anim_test.c) compares game/source/dream_anim.c's closed forms with them.
// Run: node game/tests/dream_anim.producer.mjs > game/tests/fixtures/dream-anim.txt
const sin = (index, amplitude) => amplitude * Math.sin(index * (Math.PI / 128));
const cos = (index, amplitude) => amplitude * Math.cos(index * (Math.PI / 128));

function spiral(trigIndex, xOffset, yOffset) {
  const initialX = 160 + xOffset;
  const particle = { x: initialX, y: 0, scale: 1, alive: true };
  const frames = [];
  let f = 0;
  let amp = 48;
  const updateParticle = () => {
    if (!f || particle.y > 8) {
      particle.x = initialX;
      particle.y = 88 - (f * f) / 80 + yOffset;
      particle.y += sin(trigIndex, amp) / 4;
      particle.x += cos(trigIndex, amp);
      particle.scale = 1 - f / 80;
      trigIndex += 4;
      if (f % 2 === 1) amp--;
      frames.push({ x: particle.x, y: particle.y, scale: particle.scale });
      f++;
      return true;
    }
    return false;
  };
  while (updateParticle());
  return frames;
}

function arc(trigIndex, xOffset, yOffset) {
  const initialX = 160 + xOffset;
  const particle = { x: initialX, y: 0, scale: 0.5 };
  const frames = [];
  let f = 0;
  let amp = 8;
  const updateParticle = () => {
    if (!f || particle.y < 88) {
      particle.x = initialX;
      particle.y = 8 + (f * f) / 5 + yOffset;
      particle.y += sin(trigIndex, amp) / 4;
      particle.x += cos(trigIndex, amp);
      amp = 8 + sin(f * 4, 40);
      frames.push({ x: particle.x, y: particle.y, scale: particle.scale });
      f++;
      return true;
    }
    return false;
  };
  while (updateParticle());
  return frames;
}

function circle(trigIndex, speed, xOffset, yOffset) {
  const initialX = 160 + xOffset;
  const initialY = 90 + yOffset;
  const particle = { x: initialX, y: initialY, scale: 1 };
  const frames = [];
  let amp = 120;
  const updateParticle = () => {
    if (amp > 8) {
      particle.x = initialX;
      particle.y = initialY;
      particle.y += sin(trigIndex, amp);
      particle.x += cos(trigIndex, amp);
      amp -= speed;
      trigIndex += 4;
      frames.push({ x: particle.x, y: particle.y, scale: particle.scale });
      return true;
    }
    return false;
  };
  while (updateParticle());
  return frames;
}

const SAMPLES = [0, 1, 2, 5, 10, 20, 40];
const fmt = n => n.toFixed(4);
function dump(kind, tag, frames) {
  const picks = [...new Set([...SAMPLES.filter(f => f < frames.length), frames.length - 1])];
  console.log(`${kind} ${tag} ${frames.length} ${picks.length}`);
  for (const f of picks) console.log(`${f} ${fmt(frames[f].x)} ${fmt(frames[f].y)} ${fmt(frames[f].scale)}`);
}

for (const [pos, xo, yo] of [[0, 0, 0], [1, -100, -15]]) {
  for (let cycle = 0; cycle < 8; cycle++)
    for (let i = 0; i < 4; i++) dump("S", `${pos} ${cycle} ${i}`, spiral(16 * cycle + i * 64, xo, yo));
  for (let i = 0; i < 9; i++) dump("A", `${pos} ${i} 0`, arc(i * 16, xo, yo));
  for (const speed of [4, 8]) for (let i = 0; i < 16; i++) dump("C", `${pos} ${speed} ${i}`, circle(i * 16, speed, xo, yo));
}

// doCycle(1.5, 6): each step runs 500 / c ms, forth and back unless it is the last
let total = 0;
for (let c = 1.5; c <= 6; c += 0.5) total += c === 6 ? 500 / c : (500 / c) * 2;
console.log(`T ${total.toFixed(4)}`);

// How many sparkles live at a moment of a sequence: spiral and arc particles come every lap of their counters (repeat 8 of getFrameMs(8),
// repeat 6 of one frame) from the moment the sequence reaches them, the circle in two batches 32 frames apart
const F = 1000 / 60;
const ce = 4700 + total;
const lives = { S: {}, A: {}, C: {} };
for (let cycle = 0; cycle < 8; cycle++) for (let i = 0; i < 4; i++) lives.S[`${cycle},${i}`] = spiral(16 * cycle + i * 64, 0, 0).length;
for (let i = 0; i < 9; i++) lives.A[i] = arc(i * 16, 0, 0).length;
for (const speed of [4, 8]) lives.C[speed] = circle(0, speed, 0, 0).length;
for (const t of [2100, 2300, 2700, 3000, 3600, 3720, 3800, 4000, 5000, 7800, 7900, 8000, 8200, 8500, 9000, 12000]) {
  let n = 0;
  for (let cycle = 0; cycle < 8; cycle++) for (let i = 0; i < 4; i++) {
    const born = 2000 + (cycle + 1) * 8 * F;
    if (t >= born && Math.floor((t - born) / F) < lives.S[`${cycle},${i}`]) n++;
  }
  for (let rep = 0; rep < 6; rep++) for (let i = 0; i < 9; i++) {
    const born = 3700 + (rep + 1) * F;
    if (t >= born && Math.floor((t - born) / F) < lives.A[i]) n++;
  }
  for (const [b, speed] of [[0, 4], [1, 8]]) {
    const born = ce + b * 32 * F;
    if (t >= born && Math.floor((t - born) / F) < lives.C[speed]) n += 16;
  }
  console.log(`N ${t} ${n}`);
}

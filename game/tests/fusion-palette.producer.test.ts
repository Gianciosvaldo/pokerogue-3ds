/* The oracle for game/source/fusion_palette.c: the pinned upstream's real Pokemon.updateFusionPalette (QuantizerCelebi, the hue merge, the
 * nearest-colour blend) run on random sprite frames, plus the sprite shader's fusion branch worked out as the GLSL has it. Nothing of the
 * function is replaced but what it reads from the browser: textures.get(key) answers a one-frame texture whose "source image" carries the
 * case's pixels, and a canvas' getContext("2d") gives them back through drawImage/getImageData. A shiny variant's colour map goes through
 * the real variantColorCache.
 * Per case: the four frames (RGBA hex), the variant maps, spriteColors and fusionSpriteColors as upstream left them in the sprite's
 * pipelineData (or S -1 when it recoloured nothing), and the colour each of the first 32 spriteColors is drawn as.
 * FP_N=300 FP_SEED=7 FP_OUT=/abs/fusion-palette.txt, Node 22. */
import { speciesDataRegistry } from "#app/globals/global-species-data-registry";
import { SpeciesId } from "#enums/species-id";
import { Pokemon } from "#field/pokemon";
import { variantColorCache } from "#sprites/variant";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import { expect, it, vi } from "vitest";

// the framework replaces this with `() => null` when it builds its game; the real one is read before that
const realUpdateFusionPalette = Pokemon.prototype.updateFusionPalette;
const N = Number(process.env.FP_N ?? 300), SEED = Number(process.env.FP_SEED ?? 7);
const OUT = process.env.FP_OUT ?? "/tmp/fusion-palette.txt";
let seed = SEED;
const rnd = (n: number) => { seed = (Math.imul(seed, 1103515245) + 12345) >>> 0; return (seed >>> 8) % n; };
const hex2 = (n: number) => n.toString(16).padStart(2, "0");

function hsvToRgb(h: number, s: number, v: number): number[] {
  const f = (n: number) => { const k = (n + h / 60) % 6; return v - v * s * Math.max(Math.min(k, 4 - k, 1), 0); };
  return [Math.round(f(5) * 255), Math.round(f(3) * 255), Math.round(f(1) * 255)];
}

/* a sprite frame: a few colours (some of one hue, to make the hue merge work), transparent surround, now and then a black outline, a
 * half-transparent pixel, or exactly equal counts of colours (ties in the sort) */
function frame(w: number, h: number): { data: Uint8ClampedArray; colors: number[][] } {
  const k = 1 + rnd(rnd(3) === 0 ? 14 : 6);
  const base = rnd(360), colors: number[][] = [];
  for (let i = 0; i < k; i++) {
    const hue = rnd(2) ? (base + rnd(50) - 25 + 360) % 360 : rnd(360);
    colors.push(hsvToRgb(hue, rnd(11) / 10, 0.15 + rnd(86) / 100));
  }
  if (rnd(4) === 0) colors.push([0, 0, 0]);
  const data = new Uint8ClampedArray(w * h * 4);
  const tie = rnd(5) === 0;
  const margin = rnd(4);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const inside = x >= margin && y >= margin && x < w - margin && y < h - margin && (rnd(10) > 1 || tie);
      if (!inside) continue;
      const ci = tie ? (x + y * w) % colors.length : rnd(colors.length);
      const [r, g, b] = colors[ci];
      const i = (y * w + x) * 4;
      data[i] = r; data[i + 1] = g; data[i + 2] = b;
      data[i + 3] = !tie && rnd(60) === 0 ? 40 + rnd(180) : 255;
    }
  }
  return { data, colors };
}

const phaserGame = new Phaser.Game({ type: Phaser.HEADLESS });
const lines: string[] = [`V 1 ${N}`];

it("starts on Node 22", () => {
  expect(process.versions.node.split(".")[0]).toBe("22");
});

it("fusion palettes", async () => {
  const game = new GameManager(phaserGame);
  game.override.battleStyle("single").startingWave(5).enemySpecies(SpeciesId.MAGIKARP);
  await game.classicMode.startBattle(SpeciesId.BULBASAUR);
  const scene = game.scene as any;
  const all = speciesDataRegistry.getAllSpecies();
  // the four canvases of one call, in the order the function makes them
  let frames: Array<{ data: Uint8ClampedArray; w: number; h: number }> = [];
  let made = 0;
  const textureOf = (c: number) => ({
    firstFrame: "f",
    frames: { f: { width: frames[c].w, height: frames[c].h, cutX: 0, cutY: 0 } },
    getSourceImage: () => ({ pixels: frames[c].data }),
  });
  let nextTexture = 0;
  vi.spyOn(scene.textures, "get").mockImplementation((() => textureOf(nextTexture++ % 4)) as any);
  vi.spyOn(HTMLCanvasElement.prototype, "getContext").mockImplementation((function (this: any) {
    const n = made++;
    return {
      drawImage(img: any) { this.pixels = img.pixels; },
      getImageData(_x: number, _y: number, w: number, h: number) { expect(this.pixels.length).toBe(w * h * 4); return { data: this.pixels }; },
      n,
    };
  }) as any);
  let cases = 0, recoloured = 0, withMaps = 0, none = 0;
  for (let ci = 0; ci < N; ci++) {
    Phaser.Math.RND.sow([`fusion-palette-${SEED}-${ci}`]);
    const sp = all[rnd(all.length)];
    let fsp = all[rnd(all.length)];
    if (fsp === sp) fsp = all[(all.indexOf(sp) + 1) % all.length];
    const mon = scene.addPlayerPokemon(sp, 5);
    mon.fusionSpecies = fsp;
    mon.fusionFormIndex = 0;
    mon.shiny = rnd(3) === 0; mon.variant = mon.shiny ? rnd(3) : 0;
    mon.fusionShiny = rnd(3) === 0; mon.fusionVariant = mon.fusionShiny ? rnd(3) : 0;
    const pd: { pipelineData: Record<string, any> } = { pipelineData: {} };
    vi.spyOn(mon, "getSprite").mockImplementation((() => pd) as any);
    vi.spyOn(mon, "getTintSprite").mockImplementation((() => null) as any);
    const sf = mon.getSpeciesForm(), ff = mon.getFusionSpeciesForm();
    const keyA = sf.getSpriteKey(mon.getGender() === 1, sf.formIndex, mon.shiny, mon.variant);
    const keyB = ff.getSpriteKey(mon.getFusionGender() === 1, ff.formIndex, mon.fusionShiny, mon.fusionVariant);
    const keys = [keyA, keyA.replace("pkmn__", "pkmn__back__"), keyB, keyB.replace("pkmn__", "pkmn__back__")];   // A front, A back, B front, B back
    // frame sizes and pixels
    frames = [0, 1, 2, 3].map(() => { const w = 6 + rnd(24), h = 6 + rnd(24); const f = frame(w, h); return { data: f.data, w, h }; });
    if (rnd(25) === 0) frames[2].data.fill(0);   // a fusion half with nothing to see: "Failed to create fusion palette"
    // variant maps (the real cache), for sheets that recolour: some of the frame's colours to others
    const maps: string[][] = [[], [], [], []];
    for (const k of Object.keys(variantColorCache)) delete (variantColorCache as any)[k];
    [[0, mon.shiny, mon.variant], [1, mon.shiny, mon.variant], [2, mon.fusionShiny, mon.fusionVariant], [3, mon.fusionShiny, mon.fusionVariant]].forEach(([i, shiny, variant]) => {
      if (rnd(3) === 0) return;   // this sheet has no colour map
      const picks: string[] = [];
      const d = frames[i as number].data;
      const entries: Record<string, string> = {};
      for (let t = 0, n = 1 + rnd(5); t < n; t++) {
        const px = rnd(d.length / 4) * 4;
        if (!d[px + 3]) continue;
        const from = hex2(d[px]) + hex2(d[px + 1]) + hex2(d[px + 2]);
        entries[from] = hex2(rnd(256)) + hex2(rnd(256)) + hex2(rnd(256));
      }
      const cache: any = (variantColorCache as any)[keys[i as number]] ?? {};
      cache[variant as number] = entries;
      (variantColorCache as any)[keys[i as number]] = cache;
      if (shiny) {
        for (const [f, t] of Object.entries(entries)) picks.push(`${f} ${t}`);
        maps[i as number] = picks;
      }
    });
    made = 0;
    nextTexture = 0;
    realUpdateFusionPalette.call(mon);
    expect(made).toBe(4);
    cases++;
    lines.push(`C ${ci}`);
    frames.forEach(f => lines.push(`P ${f.w} ${f.h} ${Buffer.from(f.data).toString("hex")}`));
    maps.forEach(m => lines.push(`M ${m.length} ${m.join(" ")}`.trimEnd()));
    if (maps.some(m => m.length)) withMaps++;
    const sc: number[][] | undefined = pd.pipelineData.spriteColors, fc: number[][] | undefined = pd.pipelineData.fusionSpriteColors;
    if (!sc || !fc) {
      none++;
      lines.push("S -1", "E");
      continue;
    }
    recoloured++;
    lines.push(`S ${sc.length} ${sc.map(c => c.join(" ")).join(" ")}`.trimEnd());
    lines.push(`F ${fc.length} ${fc.map(c => c.join(" ")).join(" ")}`.trimEnd());
    // the shader: the first 32 spriteColors, bg = their grey, fg = the fusion colour, overlay (GLSL: mix(1 - 2(1-bg)(1-fg), 2 bg fg, step(bg, .5)))
    const to: number[] = [];
    for (let i = 0; i < Math.min(32, sc.length); i++) {
      const gray = (sc[i][0] / 255 + sc[i][1] / 255 + sc[i][2] / 255) / 3;
      for (let c = 0; c < 3; c++) {
        const fg = fc[i][c] / 255, bg = gray;
        const v = bg <= 0.5 ? 2 * bg * fg : 1 - 2 * (1 - bg) * (1 - fg);
        to.push(Math.round(v * 255));
      }
    }
    lines.push(`T ${to.length / 3} ${to.join(" ")}`.trimEnd(), "E");
  }
  lines.push(`# cases ${cases} recoloured ${recoloured} none ${none} withMaps ${withMaps}`);
  expect(recoloured).toBeGreaterThan(N * 0.8);
  fs.writeFileSync(OUT, lines.join("\n") + "\n");
  console.log(lines[lines.length - 1]);
}, 1_800_000);

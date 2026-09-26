// Bundles the English locales and the handful of data JSONs the game fetches at runtime.
const g: any = globalThis;
const locales = import.meta.glob("../../../locales/en/**/*.json", { eager: true, import: "default" });
const data = import.meta.glob(
  [
    "../../../assets/exp-sprites.json",
    "../../../assets/starter-colors.json",
    "../../../assets/biome-bgm-loop-points.json",
    "../../../assets/battle-anims/tackle.json",
    "../../../assets/images/pokemon/variant/_masterlist.json",
    "../../../assets/images/pokemon/variant/_exp_masterlist.json",
  ],
  { eager: true, import: "default" },
);
for (const [k, v] of Object.entries({ ...locales, ...data })) {
  g.__files[k.replace("../../../", "").replace(/^assets\//, "")] = v;
}
export {};

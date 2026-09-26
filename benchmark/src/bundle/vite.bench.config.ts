import { resolve } from "node:path";
import { defineConfig } from "vite";
import { sharedConfig } from "../../vite.config.ts";

const shim = (f: string) => resolve(__dirname, "shims", f);

export default defineConfig(async config => {
  const base = await sharedConfig({ ...config, mode: "production" });
  // The upstream vite config's static-shell plugin injects HTML; not wanted for a library bundle.
  base.plugins = (base.plugins as any[]).filter(p => !String(p?.name ?? "").includes("static-shell"));
  return {
    ...base,
    mode: "production",
    define: {
      "import.meta.env.VITE_BYPASS_LOGIN": JSON.stringify("1"),
    },
    resolve: {
      ...base.resolve,
      alias: [
        { find: /^vitest$/, replacement: shim("vitest.ts") },
        { find: /^chalk$/, replacement: shim("chalk.ts") },
        { find: /^(node:)?util$/, replacement: shim("util.ts") },
        { find: /^(node:)?fs$/, replacement: shim("fs.ts") },
        { find: /^(node:)?path$/, replacement: shim("path.ts") },
        { find: /^jsdom$/, replacement: shim("empty.ts") },
        { find: /^(node:)?inspector$/, replacement: shim("inspector.ts") },
      ],
    },
    build: {
      ...base.build,
      outDir: resolve(__dirname, process.env.PROFILE ? "dist-prof" : "dist"),
      ...(process.env.PROFILE ? { minify: false } : {}),
      emptyOutDir: true,
      sourcemap: !!process.env.PROFILE,
      manifest: false,
      target: "es2022",
      lib: { entry: resolve(__dirname, "entry.ts"), formats: ["es"], fileName: () => "bench.mjs" },
      rolldownOptions: {
        ...base.build.rolldownOptions,
        output: {
          ...base.build.rolldownOptions.output,
          codeSplitting: false,
          ...(process.env.PROFILE ? { minify: false } : {}),
        },
      },
    },
  };
});

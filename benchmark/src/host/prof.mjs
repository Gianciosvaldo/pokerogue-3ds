// Node inspector CPU profiling hooks for the benchmark (__profStart/__profStop).
import inspector from "node:inspector";
import fs from "node:fs";
const session = new inspector.Session();
session.connect();
session.post("Profiler.enable");
session.post("Profiler.setSamplingInterval", { interval: 200 });
globalThis.__profStart = () => session.post("Profiler.start");
globalThis.__profStop = () =>
  session.post("Profiler.stop", (err, { profile }) => {
    fs.writeFileSync(process.env.PROF_OUT ?? "/tmp/bench.cpuprofile", JSON.stringify(profile));
    process.stdout.write("profile written\n");
  });

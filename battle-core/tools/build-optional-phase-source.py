from pathlib import Path
import sys
s=Path(sys.argv[1]).read_text()
def replace(a,b):
 global s
 assert s.count(a)==1,a
 s=s.replace(a,b,1)
replace('it("turntest producer", async () => {', 'it("optional phase typed trainer probe", async () => {\n  if (!process.versions.node.startsWith("22.")) throw new Error("Node22 required");\n  for (const [k,v] of Object.entries(process.env)) if(k.startsWith("TURN_") && v==="1" && !["TURN_N","TURN_SEED","TURN_OUT","TURN_TRAINER"].includes(k)) throw new Error("unapproved mode "+k);\n  const wanted=Number(process.env.TURN_N??2000), requestSeed=Number(process.env.TURN_SEED??4242);\n  if(!Number.isSafeInteger(wanted)||wanted<120||wanted>100000||!Number.isSafeInteger(requestSeed)||requestSeed<0||requestSeed>0xffffffff||process.env.TURN_TRAINER!=="1")throw new Error("invalid owning request");')
replace('  const queueLines = () => {','''  const optionalIndex = (ph: any) => {
    if (!("battlerIndex" in ph)) return "A";
    const v = ph.battlerIndex;
    if (v === undefined) return "U";
    if (v === null) return "N";
    if (typeof v !== "number" || !Number.isSafeInteger(v)) throw new Error("invalid phase index");
    return `I${v}`;
  };
  const queueLines = () => {''')
replace('q.queue.map((ph: any) => ph.battlerIndex).join("/")','q.queue.map(optionalIndex).join("/")')
replace('  const out: string[] = [];','  const out: string[] = [`# OPTIONAL_PHASE 1 ${N}`];')
replace('  fs.writeFileSync(OUT, out.join("\\n") + "\\n");','  if (Object.keys(thrown).length) throw new Error("owning optional probe requires complete source cases");\n  out.push(`# OPTIONAL_COMPLETE ${N}`);\n  fs.writeFileSync(OUT, out.join("\\n") + "\\n");')
Path(sys.argv[2]).write_text(s)

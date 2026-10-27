#!/usr/bin/env python3
"""Transport native complete movesets on the independently validated power inputs."""
from pathlib import Path
import json,subprocess,sys,tempfile
assert len(sys.argv)==4,'focused.json constructors.json output.inc'
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='fused-boss-transport-')as td:
 subprocess.run([sys.executable,str(root/'game-core/tools/build-fusion-power-proof.py'),*sys.argv[1:3],td+'/power.inc'],check=True)
 assert Path(td+'/power.inc').read_bytes()==(root/'game-core/tests/fixtures/fusion-power.native.inc').read_bytes(),'power input identity changed'
def a(v):return '{'+','.join(map(str,v))+'}'
def rng(s):return a(s.split(',')[1:]+['0'])
lines=['/* Complete native selected moves, PokemonMove metadata and generator finish RNG. */','typedef struct{int n,moves[4][4];pr_rng after;}native_fused_boss_result;','static const native_fused_boss_result native_fused_boss_results[]={']
for name in sys.argv[1:3]:
 for r in json.loads(Path(name).read_text())['rows']:
  g=r['generation'];moves=[a([t[k]for k in ['move','used','up','max']])for t in g['moves']]
  lines.append('{'+str(len(moves))+',{'+','.join(moves)+'},'+rng(g['after'])+'},')
lines+=['};'];Path(sys.argv[3]).write_text('\n'.join(lines)+'\n')

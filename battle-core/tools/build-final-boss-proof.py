#!/usr/bin/env python3
"""Encode complete actual controller captures without computing expected gameplay."""
import json,sys
from pathlib import Path
rows=Path(sys.argv[1]).read_text().splitlines();h=rows[0].split();n=int(h[2]);assert h[:2]==['FINAL_BOSS','1'] and rows[-1]==f'FINAL_BOSS_COMPLETE {n}' and len(rows)==n+2
out=[f'FINALBOSS 1 {n}']
for i,line in enumerate(rows[1:-1]):
 r=json.loads(line);assert r['i']==i
 out.append('C '+' '.join(map(str,[i,r['enemy'],r['boss'],r['form'],r['segment'],r['double'],r['method'],r['mode'],r['wave']])))
 out.append('PARTY '+' '.join(str(v) for p in r['inputParty'] for v in p))
 out.append('R '+r['rngBefore'].removeprefix('!rnd,').replace(',',' '))
 for key,prefix in [('inputMoves','I'),('moves','O')]:
  out.append(f'{prefix} {len(r[key])}')
  out += ['M '+' '.join(map(str,m)) for m in r[key]]
 out.append(f"MODS {len(r['mods'])}")
 for m in r['mods']:
  assert m[0]=='TurnHeldItemTransferModifier';out.append('MOD '+' '.join(map(str,m[1:])))
 out.append("LEVELS "+str(len(r["queueLevels"]))+" "+" ".join(map(str,r["queueLevels"])))
 out.append(f"Q {len(r['queue'])}")
 for q in r['queue']:out.append('P '+' '.join(map(str,q)))
 out.append(f"DYNAMIC {len(r['dynamic'])}")
 for q in r['dynamic']:out.append('D '+' '.join(map(str,q)))
 for key,prefix in [('rngAfter','RO')]:out.append(prefix+' '+r[key].removeprefix('!rnd,').replace(',',' '))
 out.append('E')
out.append(f'FINALBOSS_COMPLETE {n}');Path(sys.argv[2]).write_text('\n'.join(out)+'\n')

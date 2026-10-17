#!/usr/bin/env python3
"""Add owning stat turns while retaining the complete doubles comparator/floors."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1])
subprocess.run([sys.executable,root/'tools/build-wide-combat-proof.py',out],check=True)
s=out.read_text()
def put(old,new):
 global s
 assert s.count(old)==1,old[:120]
 s=s.replace(old,new,1)
put('  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");','''  const observed=[MoveId.SPEED_SWAP,MoveId.POWER_SHIFT,MoveId.TOPSY_TURVY].map((m,k)=>vi.spyOn(allMoves[m].getAttrs(["SwapStatAttr","ShiftStatAttr","InvertStatsAttr"][k] as any)[0],"apply"));''')
put('moves=[MoveId.TACKLE,MoveId.DRAGON_PULSE,MoveId.SEISMIC_TOSS,MoveId.DRAGON_RAGE,MoveId.FISSURE,MoveId.ERUPTION,MoveId.HEAT_CRASH,MoveId.STORED_POWER]','moves=[MoveId.SPEED_SWAP,MoveId.POWER_SHIFT,MoveId.TOPSY_TURVY]')
put('      if(i%3!==0){','''      for(let stat=1;stat<=7;stat++)p.setStatStage(stat,((i+k*3+stat)%13)-6);
      if(i%16<13){p.summonData.stats[Stat.ATK]=[0,1,2147483648,1099511627776][(Math.floor(i/16)+k)%4];p.summonData.stats[Stat.DEF]=[0,2,10000,1073741824][(Math.floor(i/64)+k)%4];p.summonData.stats[Stat.SPD]=[0,1,20000000,1099511627776][(Math.floor(i/256)+k)%4];}
      if(i%3!==0){''')
old='''  const results=rawDamage.mock.results.filter(x=>x.type==="return").map(x=>(x.value as any).damage),wide=results.filter(x=>x>2147483647).length;
  if(threwCount||wide<100)throw Error(`wide native combat missing: exceptions ${threwCount}, wide ${wide}`);
  console.log(`WIDE_COMBAT raw=${results.length} aboveInt32=${wide} max=${Math.max(...results)}`);'''
put(old,'''  const applied=observed.map(s=>s.mock.results.filter(x=>x.type==="return"&&x.value===true).length);
  if(threwCount||applied.some(n=>n<100))throw Error(`stat owning turns missing: exceptions ${threwCount}, applied ${applied}`);
  console.log(`STAT_REARRANGE_TURNS applied=${applied}`);''')
put('it("wide native combat doubles turns"','it("native stat rearrangement doubles turns"')
out.write_text(s)

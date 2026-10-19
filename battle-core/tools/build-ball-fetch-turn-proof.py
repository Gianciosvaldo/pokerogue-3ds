#!/usr/bin/env python3
"""Actual PostTurn ownership and shared global inventory; retains wide controls."""
from pathlib import Path
import subprocess,sys
r=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);subprocess.run([sys.executable,r/'tools/build-wide-combat-proof.py',out],check=True);s=out.read_text()
def put(a,b):
 global s
 assert s.count(a)==1,a[:100]
 s=s.replace(a,b,1)
put('  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");','''  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");
  const fetch=vi.spyOn(allAbilities[AbilityId.BALL_FETCH].getAttrs("FetchBallAbAttr")[0],"apply");''')
put('    battle.playerParticipantIds.add(A0.id);','''    scene.pokeballCounts=Object.fromEntries(Array.from({length:8},(_,k)=>[k,[0,1,98,99,255,65535,2147483647,9007199254740991][(Math.floor(i/64)+k)%8]]));
    battle.lastUsedPokeball=i<4096?Math.floor(i/8)%6:null;
    if(i<4096)for(let k=0;k<4;k++){
      const p=[A0,A1,D0,D1][k];p.summonData.tags=[];p.level=50;p.customPokemonData.ability=i%2&&k===0?AbilityId.NONE:AbilityId.BALL_FETCH;p.summonData.ability=p.customPokemonData.ability;p.passive=!!(i%2&&k===0);p.customPokemonData.passive=p.passive?AbilityId.BALL_FETCH:AbilityId.NONE;p.calculateStats();p.hp=p.getMaxHp();p.status=null;p.moveset=[new PokemonMove(MoveId.SPLASH)];
      if(Math.floor(i/16)%4===0&&k===0)p.waveData.abilitiesApplied.add(AbilityId.BALL_FETCH);
    }
    battle.playerParticipantIds.add(A0.id);''')
put('        `gm=${scene.gameMode.modeId}`,','        `gm=${scene.gameMode.modeId} balls=${Array.from({length:8},(_,k)=>scene.pokeballCounts[k]).join(",")}`,')
put('  const out: string[] = [`# DOUBLES_CONTRACT 1 ${N}`];','''  const fd=fs.openSync(OUT,"w");
  const out={push:(...lines:string[])=>fs.writeSync(fd,lines.join("\\n")+"\\n")};
  out.push(`# DOUBLES_CONTRACT 1 ${N}`);''')
put('  fs.writeFileSync(OUT, out.join("\\n") + "\\n");','  fs.closeSync(fd);')
put('  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);','''  if(fetch.mock.calls.length<100)throw Error(`Fetch caller coverage ${fetch.mock.calls.length}`);
  console.log(`BALLFETCH_TURNS applied=${fetch.mock.calls.length}`);
  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);''')
put('it("wide native combat doubles turns"','it("native Ball Fetch inventory doubles turns"')
out.write_text(s)

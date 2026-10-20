#!/usr/bin/env python3
"""Real Baton Pass switches -> PostSummon ally callbacks; retain wide controls."""
from pathlib import Path
import subprocess,sys
r=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);subprocess.run([sys.executable,r/'tools/build-wide-combat-proof.py',out],check=True);s=out.read_text()
def put(a,b):
 global s
 assert s.count(a)==1,a[:100]
 s=s.replace(a,b,1)
put('  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");','''  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");
  const entryCalls=[AbilityId.HOSPITALITY,AbilityId.CURIOUS_MEDICINE].map(id=>vi.spyOn(allAbilities[id].getAttrs("PostSummonAbAttr")[0],"apply"));''')
put('    for (const b of playerBench.slice(0, rnd(playerBench.length + 1))) {','    for (const b of playerBench.slice(0, i<4096?playerBench.length:rnd(playerBench.length+1))) {')
put('      b.customPokemonData.ability = -1;','      b.customPokemonData.ability = -1;b.passive=false;b.customPokemonData.passive=AbilityId.NONE;')
put('    battle.playerParticipantIds.add(A0.id);','''    if(i<4096){
      for(let k=0;k<4;k++){
        const p=actors[k];p.summonData.tags=[];p.level=[50,256,1000000][Math.floor(i/32)%3];p.customPokemonData.ability=AbilityId.BALL_FETCH;p.summonData.ability=AbilityId.BALL_FETCH;p.status=null;p.isTerastallized=false;p.calculateStats();p.hp=p.getMaxHp();p.summonData.statStages=[2,-3,4,-5,6,-6,1];p.summonData.statStages[Stat.SPD-1]=k===0?6:-6;p.moveset=[new PokemonMove(k===0?MoveId.BATON_PASS:MoveId.SPLASH)];
      }
      A1.hp=Math.floor(A1.getMaxHp()/2)||1;
      for(let k=0;k<playerBench.length;k++){
        const p=playerBench[k],ab=i%2?AbilityId.CURIOUS_MEDICINE:AbilityId.HOSPITALITY,passive=!!(Math.floor(i/2)%2);p.species=speciesDataRegistry.getSpecies([SpeciesId.BLISSEY,SpeciesId.SHUCKLE,SpeciesId.BULBASAUR,SpeciesId.CHARMANDER][k]);p.formIndex=0;p.fusionSpecies=null;p.level=[1,50,256,1000000][Math.floor(i/4)%4];p.customPokemonData.ability=passive?AbilityId.BALL_FETCH:ab;p.summonData.ability=p.customPokemonData.ability;p.passive=passive;p.customPokemonData.passive=passive?ab:AbilityId.NONE;p.status=null;p.isTerastallized=false;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.calculateStats();p.generateName();p.hp=p.getMaxHp();
      }
    }
    battle.playerParticipantIds.add(A0.id);''')
put('  const out: string[] = [`# DOUBLES_CONTRACT 1 ${N}`];','''  const fd=fs.openSync(OUT,"w");
  const out={push:(...rows:string[])=>fs.writeSync(fd,rows.join("\\n")+"\\n")};
  out.push(`# DOUBLES_CONTRACT 1 ${N}`);''')
put('  fs.writeFileSync(OUT, out.join("\\n") + "\\n");','  fs.closeSync(fd);')
put('  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);','''  if(entryCalls.some(c=>c.mock.calls.length<1000))throw Error(`Entry ownership coverage ${entryCalls.map(c=>c.mock.calls.length)}`);
  console.log(`ENTRY_ALLIES_TURNS calls=${entryCalls.map(c=>c.mock.calls.length)}`);
  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);''')
put('it("wide native combat doubles turns"','it("native ally callbacks through Baton Pass switches"')
out.write_text(s)

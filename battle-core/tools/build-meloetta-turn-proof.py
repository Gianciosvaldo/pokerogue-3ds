#!/usr/bin/env python3
"""Actual Relic Song owning phases, primary/passive Sheer Force and wide turns."""
from pathlib import Path
import subprocess,sys
r=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);subprocess.run([sys.executable,r/'tools/build-wide-combat-proof.py',out],check=True);s=out.read_text()
def put(a,b):
 global s
 assert s.count(a)==1,a[:100]
 s=s.replace(a,b,1)
put('  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");','''  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");
  const formSpy=vi.spyOn(scene,"triggerPokemonFormChange");''')
put('moves=[MoveId.TACKLE,MoveId.DRAGON_PULSE,MoveId.SEISMIC_TOSS,MoveId.DRAGON_RAGE,MoveId.FISSURE,MoveId.ERUPTION,MoveId.HEAT_CRASH,MoveId.STORED_POWER]','moves=[MoveId.RELIC_SONG,MoveId.TACKLE,MoveId.RELIC_SONG,MoveId.DRAGON_PULSE]')
put('      if(i%3!==0){','''      p.species=speciesDataRegistry.getSpecies(SpeciesId.MELOETTA);p.formIndex=Math.floor(i/256)%2;p.customPokemonData.ability=(Math.floor(i/32)+k)%3===0?AbilityId.SHEER_FORCE:AbilityId.BALL_FETCH;p.summonData.ability=p.customPokemonData.ability;p.passive=i%7===0;p.customPokemonData.passive=p.passive?AbilityId.SHEER_FORCE:AbilityId.NONE;p.calculateStats();p.hp=p.getMaxHp();p.generateName();
      if(i%3!==0){''')
put('  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);','''  const applied=formSpy.mock.results.filter((x:any,k:number)=>x.type==="return"&&x.value===true&&formSpy.mock.calls[k][1].name==="SpeciesFormChangePostMoveTrigger").length;
  if(applied<100)throw Error(`Meloetta owning form changes missing ${applied}`);
  console.log(`MELOETTA_TURNS applied=${applied} calls=${formSpy.mock.calls.length}`);
  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);''')
put('it("wide native combat doubles turns"','it("native Meloetta Relic Song doubles turns"')
out.write_text(s)

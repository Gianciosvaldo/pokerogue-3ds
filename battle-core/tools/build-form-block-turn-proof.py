#!/usr/bin/env python3
"""Native owning turns: Disguise/Ice Face before subsequent hits and actions."""
from pathlib import Path
import subprocess,sys
r=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);subprocess.run([sys.executable,r/'tools/build-wide-combat-proof.py',out],check=True);s=out.read_text()
def put(a,b):
 global s
 assert s.count(a)==1,a[:100]
 s=s.replace(a,b,1)
put('  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");','''  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");
  const formSpies=[AbilityId.DISGUISE,AbilityId.ICE_FACE].map(a=>vi.spyOn(allAbilities[a].getAttrs("FormBlockDamageAbAttr")[0],"apply"));''')
put('moves=[MoveId.TACKLE,MoveId.DRAGON_PULSE,MoveId.SEISMIC_TOSS,MoveId.DRAGON_RAGE,MoveId.FISSURE,MoveId.ERUPTION,MoveId.HEAT_CRASH,MoveId.STORED_POWER]','moves=[MoveId.DRAGON_CLAW,MoveId.DRAGON_PULSE,MoveId.BULLET_SEED,MoveId.BITE,MoveId.WATER_SHURIKEN,MoveId.SURF,MoveId.DOUBLE_HIT,MoveId.HYPER_VOICE]')
put('      if(i%3!==0){','''      p.species=speciesDataRegistry.getSpecies(k%2?SpeciesId.EISCUE:SpeciesId.MIMIKYU);p.formIndex=Math.floor(i/256)%2;p.customPokemonData.ability=k%2?AbilityId.ICE_FACE:AbilityId.DISGUISE;p.summonData.ability=p.customPokemonData.ability;p.calculateStats();p.hp=p.getMaxHp();p.generateName();
      if(i%3!==0){''')
put('  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);','''  const hits=formSpies.map(x=>x.mock.calls.length);
  if(hits.some(n=>n<100))throw Error(`Form block owning callbacks missing ${hits}`);
  console.log(`FORM_BLOCK_TURNS calls=${hits}`);
  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);''')
put('it("wide native combat doubles turns"','it("native form-block doubles turns"')
out.write_text(s)

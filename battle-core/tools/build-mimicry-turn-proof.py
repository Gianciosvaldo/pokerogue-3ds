#!/usr/bin/env python3
"""Actual terrain moves and Mimicry callbacks; preserve 4096 wide controls."""
from pathlib import Path
import subprocess,sys
r=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);subprocess.run([sys.executable,r/'tools/build-wide-combat-proof.py',out],check=True);s=out.read_text()
def put(a,b):
 global s
 assert s.count(a)==1,a[:100]
 s=s.replace(a,b,1)
put('  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");','''  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");
  const mimicry=vi.spyOn(allAbilities[AbilityId.MIMICRY].getAttrs("TerrainEventTypeChangeAbAttr")[0],"apply");''')
put('    battle.playerParticipantIds.add(A0.id);','''    if(i<4096){
      const caster=i%4,primary=Math.floor(i/4)%4,passive=Math.floor(i/16)%4;
      for(let k=0;k<4;k++){
        const p=actors[k];p.summonData.tags=[];p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.KELDEO,SpeciesId.RATTATA,SpeciesId.CHARMANDER][k]);p.formIndex=0;p.level=[50,256,1000000][Math.floor(i/64)%3];p.customPokemonData.ability=k===primary?AbilityId.MIMICRY:AbilityId.NONE;p.summonData.ability=p.customPokemonData.ability;p.passive=k===passive;p.customPokemonData.passive=p.passive?AbilityId.MIMICRY:AbilityId.NONE;p.status=null;p.isTerastallized=Math.floor(i/256)%4===k;p.teraType=[0,4,11,18][Math.floor(i/1024)%4];p.summonData.types=[7,11];p.summonData.addedType=Math.floor(i/4)%19;p.calculateStats();p.generateName();p.hp=p.getMaxHp();p.summonData.statStages.fill(0);p.summonData.statStages[Stat.SPD-1]=k===caster?6:-6;p.moveset=[new PokemonMove(k===caster?[MoveId.ELECTRIC_TERRAIN,MoveId.MISTY_TERRAIN,MoveId.GRASSY_TERRAIN,MoveId.PSYCHIC_TERRAIN][Math.floor(i/64)%4]:MoveId.SPLASH)];
      }
    }
    battle.playerParticipantIds.add(A0.id);''')
put('  const out: string[] = [`# DOUBLES_CONTRACT 1 ${N}`];','''  const fd=fs.openSync(OUT,"w");
  const out={push:(...rows:string[])=>fs.writeSync(fd,rows.join("\\n")+"\\n")};
  out.push(`# DOUBLES_CONTRACT 1 ${N}`);''')
put('  fs.writeFileSync(OUT, out.join("\\n") + "\\n");','  fs.closeSync(fd);')
put('  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);','''  if(mimicry.mock.calls.length<1000)throw Error(`Mimicry ownership coverage ${mimicry.mock.calls.length}`);
  console.log(`MIMICRY_TURNS applied=${mimicry.mock.calls.length}`);
  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);''')
put('it("wide native combat doubles turns"','it("native Mimicry terrain owning doubles turns"')
out.write_text(s)

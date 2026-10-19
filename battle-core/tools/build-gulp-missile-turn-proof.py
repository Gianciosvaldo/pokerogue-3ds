#!/usr/bin/env python3
"""Native owning Surf/Dive and loaded Gulp retaliation; retain 4096 wide controls."""
from pathlib import Path
import subprocess,sys
r=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);subprocess.run([sys.executable,r/'tools/build-wide-combat-proof.py',out],check=True);s=out.read_text()
def put(a,b):
 global s
 assert s.count(a)==1,a[:100]
 s=s.replace(a,b,1)
put('  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");','''  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");
  const gulp=vi.spyOn(allMoves[MoveId.SURF].getAttrs("GulpMissileTagAttr")[0],"apply");''')
put('    battle.playerParticipantIds.add(A0.id);','''    if(i<4096){
      const holder=Math.floor(i/8)%4,opponent=holder^2,loaded=i>=2048;
      for(let k=0;k<4;k++){
        const p=actors[k];p.summonData.tags=[];p.species=speciesDataRegistry.getSpecies(k===holder?SpeciesId.CRAMORANT:SpeciesId.BULBASAUR);p.formIndex=k===holder&&loaded?1+(i%2):0;p.level=[50,100,256,1000000][Math.floor(i/32)%4];p.customPokemonData.ability=k===holder?AbilityId.GULP_MISSILE:k===opponent?[AbilityId.NONE,AbilityId.MAGIC_GUARD,AbilityId.LIMBER,AbilityId.NONE][Math.floor(i/128)%4]:AbilityId.NONE;p.summonData.ability=p.customPokemonData.ability;p.passive=false;p.customPokemonData.passive=AbilityId.NONE;p.nature=0;p.ivs=Array(6).fill(15);p.calculateStats();p.generateName();p.hp=k===holder&&i%2?Math.max(1,Math.floor(p.getMaxHp()/3)):p.getMaxHp();p.status=null;p.summonData.statStages.fill(0);p.summonData.statStages[Stat.SPD-1]=k===holder?6:-6;p.moveset=[new PokemonMove(k===holder?(loaded?MoveId.SPLASH:i%4<2?MoveId.SURF:MoveId.DIVE):k===opponent?MoveId.TACKLE:MoveId.SPLASH)];
        if(k===holder&&loaded)p.summonData.tags.push(getBattlerTag(i%2?BattlerTagType.GULP_MISSILE_PIKACHU:BattlerTagType.GULP_MISSILE_ARROKUDA,0,MoveId.SURF));
      }
    }
    battle.playerParticipantIds.add(A0.id);''')
put('  const out: string[] = [`# DOUBLES_CONTRACT 1 ${N}`];','''  const fd=fs.openSync(OUT,"w");
  const out={push:(...lines:string[])=>fs.writeSync(fd,lines.join("\\n")+"\\n")};
  out.push(`# DOUBLES_CONTRACT 1 ${N}`);''')
put('  fs.writeFileSync(OUT, out.join("\\n") + "\\n");','  fs.closeSync(fd);')
put('  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);','''  const applied=gulp.mock.results.filter(x=>x.type==="return"&&x.value).length;
  if(applied<100)throw Error(`Gulp caller coverage ${applied}`);
  console.log(`GULP_TURNS applied=${applied}`);
  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);''')
put('it("wide native combat doubles turns"','it("native Gulp Missile owning doubles turns"')
out.write_text(s)

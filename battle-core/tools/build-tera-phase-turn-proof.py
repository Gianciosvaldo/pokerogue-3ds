#!/usr/bin/env python3
"""Native pre-command Tera ownership and actual quiet forms, retaining wide controls."""
from pathlib import Path
import subprocess,sys
r=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);subprocess.run([sys.executable,r/'tools/build-wide-combat-proof.py',out],check=True);s=out.read_text()
def put(a,b):
 global s
 assert s.count(a)==1,a[:80]
 s=s.replace(a,b,1)
put('  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");','''  const rawDamage=vi.spyOn(Pokemon.prototype,"getAttackDamage");
  let teraPhases=0;''')
put('      trace.push(phaseTag(ph));','      if(ph.phaseName==="TeraPhase")teraPhases++;\n      trace.push(phaseTag(ph));')
put('    battle.playerParticipantIds.add(A0.id);','''    scene.arena.playerTerasUsed=[0,1,2,3][Math.floor(i/4)%4];
    if(i<4096){
      const user=i%4,sp=[SpeciesId.BULBASAUR,SpeciesId.OGERPON,SpeciesId.TERAPAGOS][Math.floor(i/16)%3];
      for(let k=0;k<4;k++){
        const p=actors[k];p.summonData.tags=[];p.species=speciesDataRegistry.getSpecies(k===user?sp:SpeciesId.BULBASAUR);p.formIndex=k===user?Math.floor(i/48)%Math.max(1,p.species.forms.length):0;p.level=[50,256,1000000][Math.floor(i/384)%3];p.customPokemonData.ability=AbilityId.NONE;p.summonData.ability=AbilityId.NONE;p.passive=false;p.customPokemonData.passive=AbilityId.NONE;p.status=null;p.isTerastallized=false;p.teraType=Math.floor(i/4)%19;p.summonData.addedType=i%2?7:11;p.calculateStats();p.generateName();p.hp=p.getMaxHp();p.summonData.statStages.fill(0);p.moveset=[new PokemonMove(k===user?MoveId.TACKLE:MoveId.SPLASH)];
      }
    }
    battle.playerParticipantIds.add(A0.id);''')
put('    let before: string[];','''    if(i<4096)battle.preTurnCommands[i%4]={command:Command.TERA,skip:false};
    let before: string[];''')
put('` biome=${scene.arena.biomeId}`,','` biome=${scene.arena.biomeId} teras=${scene.arena.playerTerasUsed}`,')
put('  const out: string[] = [`# DOUBLES_CONTRACT 1 ${N}`];','''  const fd=fs.openSync(OUT,"w");
  const out={push:(...xs:string[])=>fs.writeSync(fd,xs.join("\\n")+"\\n")};
  out.push(`# DOUBLES_CONTRACT 1 ${N}`);''')
put('  fs.writeFileSync(OUT, out.join("\\n") + "\\n");','  fs.closeSync(fd);')
put('  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);','''  if(teraPhases!==4096)throw Error(`Tera ownership coverage ${teraPhases}`);
  console.log(`TERA_TURNS phases=${teraPhases}`);
  out.push(`# DOUBLES_COMPLETE ${written} ${threwCount}`);''')
put('it("wide native combat doubles turns"','it("native Tera owning doubles turns"')
out.write_text(s)

#!/usr/bin/env python3
"""Real Baton Pass entry with Costar's stage and critical tag interactions."""
from pathlib import Path
import subprocess,sys,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'entry.ts';subprocess.run([sys.executable,r/'tools/build-entry-allies-turn-proof.py',p],check=True);s=p.read_text()
def put(old,new):
 global s
 assert s.count(old)==1,old[:120]
 s=s.replace(old,new,1)
put('import { getBattlerTag } from "#data/battler-tags";','import { getBattlerTag, CritBoostTag } from "#data/battler-tags";\nimport { PokemonType } from "#enums/pokemon-type";')
put('  // The party screen a SwitchPhase opens:', '  const benchBase=playerBench.map((p:any)=>({level:p.level,teraType:p.teraType}));\n  // The party screen a SwitchPhase opens:')
put('    for (const b of playerBench) {', '    for (const b of playerBench) {\n      const base=benchBase[playerBench.indexOf(b)];b.level=base.level;b.isTerastallized=false;b.teraType=base.teraType;')
put('const entryCounts=[[0,0],[0,0]];','const entryCounts=[[0,0]];let firstClassCases=0;')
put('[AbilityId.HOSPITALITY,AbilityId.CURIOUS_MEDICINE].map((id,index)=>','[AbilityId.COSTAR].map((id,index)=>')
put('entryCounts[index][+!!params.passive]++;return nativeApply.call(this,params);','entryCounts[index][+!!params.passive]++;if(params.pokemon.getAlly()?.getTag(BattlerTagType.DRAGON_CHEER)&&params.pokemon.getTag(CritBoostTag)?.tagType===BattlerTagType.CRIT_BOOST)firstClassCases++;return nativeApply.call(this,params);')
put('const p=actors[k];p.summonData.tags=[];p.level=','const p=actors[k];p.summonData.tags=[];if(k<2)p.species=speciesDataRegistry.getSpecies(Math.floor(i/(1024*(k+1)))%2?SpeciesId.DRATINI:SpeciesId.BULBASAUR);p.formIndex=0;p.level=')
put('p.calculateStats();p.hp=p.getMaxHp();p.summonData.statStages=[2,-3,4,-5,6,-6,1];','p.calculateStats();p.generateName();p.hp=p.getMaxHp();p.summonData.statStages=[2,-3,4,-5,6,-6,1];')
put('ab=i%2?AbilityId.CURIOUS_MEDICINE:AbilityId.HOSPITALITY,passive=!!(Math.floor(i/2)%2)','ab=AbilityId.COSTAR,passive=!!(Math.floor(i/128)%2)')
put('[SpeciesId.BLISSEY,SpeciesId.SHUCKLE,SpeciesId.BULBASAUR,SpeciesId.CHARMANDER]','[SpeciesId.DRATINI,SpeciesId.BLISSEY,SpeciesId.BULBASAUR,SpeciesId.CHARMANDER]')
put('p.status=null;p.isTerastallized=false;p.moveset=[new PokemonMove(MoveId.SPLASH)];','p.status=null;p.isTerastallized=Math.floor(i/256)%3!==0;p.teraType=Math.floor(i/256)%3===1?PokemonType.STELLAR:PokemonType.DRAGON;p.moveset=[new PokemonMove(MoveId.SPLASH)];')
put('    }\n    battle.playerParticipantIds.add(A0.id);','''      const tags=[BattlerTagType.CRIT_BOOST,BattlerTagType.DRAGON_CHEER,BattlerTagType.ALWAYS_CRIT],moves=[MoveId.FOCUS_ENERGY,MoveId.DRAGON_CHEER,MoveId.LASER_FOCUS],order=Math.floor(i/64)%2?[2,1,0]:[0,1,2];
      for(const [p,mask] of [[A0,i%8],[A1,Math.floor(i/8)%8]] as any[])for(const j of order)if(mask&(1<<j))p.addTag(tags[j],0,moves[j],p.id);
      // Only presentation queued while constructing the pre-turn fixture is cleared.
      // All tag messages from the actual Costar entry remain in the turn trace.
      pm.clearAllPhases();
    }
    battle.playerParticipantIds.add(A0.id);''')
put('entryCounts.flat().some(n=>n<1024)','entryCounts.flat().some(n=>n<2048)||firstClassCases<100')
put('console.log(`ENTRY_ALLIES_TURNS calls=${entryCalls.map(c=>c.mock.calls.length)} primaryPassive=${JSON.stringify(entryCounts)}`);','console.log(`COSTAR_TURNS primaryPassive=${JSON.stringify(entryCounts)} firstClassCases=${firstClassCases}`);')
put('it("native ally callbacks through Baton Pass switches"','it("native Costar critical tags through Baton Pass entry"')
Path(sys.argv[1]).write_text(s)

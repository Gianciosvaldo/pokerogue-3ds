#!/usr/bin/env python3
"""Native Costar dispatcher, tag order and inherited critical boost behavior."""
from pathlib import Path
import subprocess,sys,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'entry.ts';subprocess.run([sys.executable,r/'tools/build-entry-allies-proof.py',p],check=True);s=p.read_text()
def put(old,new):
 global s
 assert s.count(old)==1,old[:120]
 s=s.replace(old,new,1)
put('const N=49152,seed=Number(process.env.ENTRY_ALLIES_SEED??777)','const N=65536,seed=Number(process.env.COSTAR_SEED??777)')
s=s.replace('process.env.ENTRY_ALLIES_OUT??"/tmp/entry-allies.txt"','process.env.COSTAR_OUT??"/tmp/costar.txt"').replace('ENTRY_ALLIES 2','COSTAR 1').replace('ENTRY_ALLIES_COMPLETE','COSTAR_COMPLETE')
put('import { applyAbAttrs, applyOnGainAbAttrs }','import { CritBoostTag } from "#data/battler-tags";\nimport { PokemonType } from "#enums/pokemon-type";\nimport { applyAbAttrs, applyOnGainAbAttrs }')
put(' const calls=[AbilityId.HOSPITALITY,AbilityId.CURIOUS_MEDICINE].map(id=>vi.spyOn(allAbilities[id].getAttrs("PostSummonAbAttr")[0],"apply"));',''' vi.spyOn(Pokemon.prototype,"updateInfo").mockImplementation(()=>Promise.resolve());
 const counts=[0,0];let firstClassCases=0;const attr=allAbilities[AbilityId.COSTAR].getAttrs("PostSummonAbAttr")[0],nativeApply=attr.apply;
 const calls=[vi.spyOn(attr,"apply").mockImplementation(function(this:any,params:any){counts[+!!params.passive]++;if(!params.simulated&&params.pokemon.getAlly()?.getTag(BattlerTagType.DRAGON_CHEER)&&params.pokemon.getTag(CritBoostTag)?.tagType===BattlerTagType.CRIT_BOOST)firstClassCases++;return nativeApply.call(this,params);})];''')
put(' write([`COSTAR 1 ${N} ${seed}`]);let changed=0,heals=0;',''' const tags=(key:string)=>actors.flatMap((p,k)=>[`${key} ${k} ${p.summonData.tags.length}`,...p.summonData.tags.map((t:any)=>`TAG ${t.tagType} ${t.turnCount} ${t.sourceMove??0} ${t.sourceId??-1} ${t.critStages??0}`)]);
 write([`COSTAR 1 ${N} ${seed}`]);let changed=0;''')
put('const u=i%8,kind=Math.floor(i/8)%2,simulated=!!(Math.floor(i/16)%2),shape=Math.floor(i/32)%8,gain=!!(Math.floor(i/256)%2),single=!!(Math.floor(i/512)%2);','const u=i%8,simulated=!!(Math.floor(i/8)%2),shape=Math.floor(i/16)%8,gain=!!(Math.floor(i/128)%2),single=!!(Math.floor(i/256)%2),kind=Math.floor(i/32768)%2;')
s=s.replace('[SpeciesId.BULBASAUR,SpeciesId.BLISSEY,SpeciesId.SHUCKLE,SpeciesId.CHARMANDER]','[SpeciesId.DRATINI,SpeciesId.BULBASAUR,SpeciesId.SQUIRTLE,SpeciesId.CHARMANDER]').replace('[AbilityId.HOSPITALITY,AbilityId.CURIOUS_MEDICINE][kind]','AbilityId.COSTAR')
put('p.isTerastallized=false;p.calculateStats();','p.isTerastallized=k===u&&!!(Math.floor(i/2048)%2);p.teraType=Math.floor(i/8192)%2?PokemonType.DRAGON:PokemonType.STELLAR;p.calculateStats();')
put('  pm.clearAllPhases();pm.pushNew("MessagePhase","anchor");','''  const types=[BattlerTagType.CRIT_BOOST,BattlerTagType.DRAGON_CHEER,BattlerTagType.ALWAYS_CRIT],moves=[MoveId.FOCUS_ENERGY,MoveId.DRAGON_CHEER,MoveId.LASER_FOCUS];
  for(const [p,mask] of [[actors[u],Math.floor(i/512)%8],[ally,Math.floor(i/4096)%8]] as any[]){if(p)for(const j of (kind?[2,1,0]:[0,1,2]))if(mask&(1<<j))p.addTag(types[j],0,moves[j],p.id);}
  pm.clearAllPhases();pm.pushNew("MessagePhase","anchor");''')
put('...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`)]);','...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`),...tags("IT")]);')
put('  heals+=pm.phaseQueue.levels.flat().filter((p:any)=>p.phaseName==="PokemonHealPhase").length;changed+=','  changed+=')
put('...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`),...queues()','...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`),...tags("OT"),...queues()')
put('if(calls.some(c=>c.mock.calls.length<1000)||heals<1000||changed<1000)throw Error(`coverage ${calls.map(c=>c.mock.calls.length)}/${heals}/${changed}`);','if(counts.some(n=>n<1024)||changed<1000||firstClassCases<100)throw Error(`coverage ${counts}/${changed}/${firstClassCases}`);')
put('console.log(`ENTRY_ALLIES calls=${calls.map(c=>c.mock.calls.length)} heals=${heals} changed=${changed}`);','console.log(`COSTAR primaryPassive=${counts} changed=${changed} firstClassCases=${firstClassCases}`);')
put('it("native ally entry callbacks and gain filtering"','it("native Costar stage and critical tag copy with first class lookup"')
put('if(i>=32768&&Math.floor(i/32)%2)ally.switchOutStatus=true;}if(i>=32768&&Math.floor(i/64)%2)actors[u].switchOutStatus=true;', 'if(shape===6)ally.switchOutStatus=true;}if(shape===7)actors[u].switchOutStatus=true;')
Path(sys.argv[1]).write_text(s)

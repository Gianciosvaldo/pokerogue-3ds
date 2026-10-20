#!/usr/bin/env python3
"""Actual NewBiomeEncounterPhase.doEncounter gameplay prefix, retaining pending tween."""
from pathlib import Path
import subprocess,sys,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'base.ts';subprocess.run([sys.executable,r/'tools/build-biome-callback-proof.py',p],check=True);s=p.read_text()
def put(a,b):
 global s
 assert s.count(a)==1,a[:100]
 s=s.replace(a,b,1)
put('const N=26880,seed=Number(process.env.BIOME_CALLBACK_SEED??777)','const N=6720,seed=Number(process.env.BIOME_CALLBACK_SEED??777)')
put('BIOME_CALLBACK 1','BIOME_CALLBACK 2')
put(' const calls=abilities.map',''' vi.spyOn(audioManager,"playBgm").mockImplementation(()=>{});
 let tweens=0;scene.tweens.add=(c:any)=>{if(c.duration===2000&&c.x==="+=300")tweens++;return {};};
 const data=(key:string)=>actors.map((p,k)=>{const b=p.battleData.berriesEaten;return `${key} ${k} ${+p.battleData.hasEatenBerry} ${+p.waveData.endured} ${b.length} ${b[0]??0} ${b[1]??0}`;});
 const calls=abilities.map''')
put('const u=i%8,simulated=!!(Math.floor(i/8)%2),shape=Math.floor(i/16)%8,gain=!!(Math.floor(i/128)%2),v=variants[Math.floor(i/256)];','const u=i%8,simulated=false,shape=Math.floor(i/8)%8,gain=2,v=variants[Math.floor(i/64)];')
put('  pm.clearAllPhases();','''  for(const p of actors.slice(0,4))if(!p.isOnField())scene.field.add(p);
  if(shape===5)for(const p of actors.slice(0,2))scene.field.remove(p);
  if(shape===2){const other=actors[u===0?1:0];other.customPokemonData.ability=abilities[(abilities.indexOf(v.ab)+1)%abilities.length];}
  for(let k=0;k<8;k++){const p=actors[k];p.battleData.hasEatenBerry=true;p.battleData.berriesEaten=[0,1].slice(0,1+k%2);p.waveData.endured=true;p.waveData.abilitiesApplied.add(AbilityId.BALL_FETCH);p.summonData.abilitiesApplied.add(AbilityId.BALL_FETCH);p.tempSummonData.turnCount=7+k;p.tempSummonData.waveTurnCount=9+k;}
  pm.clearAllPhases();''')
put('pm.currentPhase=pm.create("MovePhase",actors[0],[2],new PokemonMove(MoveId.SPLASH),MoveUseMode.NORMAL);','pm.currentPhase=pm.create("NewBiomeEncounterPhase");')
put('...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`)]);','...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`),...data("ID")]);')
put('  if(gain)applyOnGainAbAttrs({pokemon:actors[u],simulated});else applyAbAttrs("PostBiomeChangeAbAttr",{pokemon:actors[u],simulated});','''  const oldTween=tweens;pm.currentPhase.doEncounter();if(tweens!==oldTween+1)throw Error("missing native encounter visual continuation");''')
put('`ENVO ${env()}`,...queues()','...data("OD"),`ENVO ${env()}`,...queues()')
put('if(calls.some(c=>c.mock.calls.length<32)||changed<1000)','if(calls.some(c=>c.mock.calls.length<8)||changed<100)')
put('it("native biome callbacks preserve immutable weather rejection and terrain replay"','it("native NewBiomeEncounter gameplay prefix before visual continuation"')
Path(sys.argv[1]).write_text(s)

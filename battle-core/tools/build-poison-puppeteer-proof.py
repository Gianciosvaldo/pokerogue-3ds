#!/usr/bin/env python3
"""Actual Poison Puppeteer dispatcher, tag immunity and Misty Terrain RNG."""
from pathlib import Path
import subprocess,sys,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'entry.ts';subprocess.run([sys.executable,r/'tools/build-entry-allies-proof.py',p],check=True);s=p.read_text()
def put(old,new):
 global s
 assert s.count(old)==1,old[:120]
 s=s.replace(old,new,1)
put('const N=49152,seed=Number(process.env.ENTRY_ALLIES_SEED??777)','const N=65536,seed=Number(process.env.PUPPETEER_SEED??777)')
s=s.replace('process.env.ENTRY_ALLIES_OUT??"/tmp/entry-allies.txt"','process.env.PUPPETEER_OUT??"/tmp/puppeteer.txt"').replace('ENTRY_ALLIES 2','PUPPETEER 1').replace('ENTRY_ALLIES_COMPLETE','PUPPETEER_COMPLETE')
put('import { applyAbAttrs, applyOnGainAbAttrs }','import { Terrain } from "#data/terrain";\nimport { TerrainType } from "#enums/terrain-type";\nimport { PokemonType } from "#enums/pokemon-type";\nimport { applyAbAttrs, applyOnGainAbAttrs }')
put(' const calls=[AbilityId.HOSPITALITY,AbilityId.CURIOUS_MEDICINE].map(id=>vi.spyOn(allAbilities[id].getAttrs("PostSummonAbAttr")[0],"apply"));',''' const applied=vi.spyOn(allAbilities[AbilityId.POISON_PUPPETEER].getAttrs("ConfusionOnStatusEffectAbAttr")[0],"apply");
 const rolls=vi.spyOn(Pokemon.prototype,"randBattleSeedIntRange");''')
a=s.index(' const queues=()=>');b=s.index(' for(let i=0;',a)
s=s[:a]+''' const queues=()=>{let rows:string[]=[];
 const emit=(prefix:string,l:number,j:number,p:any)=>{
  const name=p.phaseName;if(!["CommonAnimPhase","DynamicPhaseMarker","MessagePhase","ShowAbilityPhase","HideAbilityPhase"].includes(name))throw Error(`unexpected queue ${name}`);
  const show=name==="ShowAbilityPhase",anim=name==="CommonAnimPhase",marker=name==="DynamicPhaseMarker",text=name==="MessagePhase"?hex(p.text):show?hex(p.pokemonName??"")+"."+hex(p.abilityName??""):"-";
  if(marker&&p.phaseType!=="CommonAnimPhase")throw Error(`unexpected marker ${p.phaseType}`);
  rows.push(`${prefix} ${l} ${j} ${name} ${show||anim?p.battlerIndex:-1} ${show?+p.passive:-1} ${name==="MessagePhase"?+!!p.prompt:-1} ${text} ${marker?1:anim?p.anim:0} ${anim?p.targetIndex??-1:-1}`);
 };
 pm.phaseQueue.levels.forEach((lv:any[],l:number)=>lv.forEach((p:any,j:number)=>emit("P",l,j,p)));
 const regular=[`Q ${rows.length} ${pm.phaseQueue.levels.length} ${pm.phaseQueue.currentLevel} ${+pm.phaseQueue.deferredActive}`,...rows];rows=[];
 const dyn=[...pm.dynamicQueueManager.dynamicPhaseMap.entries()].sort(([a]:any,[b]:any)=>a.localeCompare(b)).flatMap(([,q]:any)=>q.queue);dyn.forEach((p:any,j:number)=>emit("D",0,j,p));
 return [...regular,`DQ ${dyn.length}`,...rows];};
 const tags=(key:string)=>actors.map((p,k)=>{const t=p.getTag(BattlerTagType.CONFUSED);return `${key} ${k} ${+!!t} ${t?.turnCount??0} ${t?.sourceMove??0} ${t?.sourceId??-1}`;});
 write([`PUPPETEER 1 ${N} ${seed}`]);let added=0,blockedDraws=0;
'''+s[b:]
a=s.index('  const u=i%8');b=s.index('\n  pm.clearAllPhases();',a)
s=s[:a]+'''  const u=i%8,t=Math.floor(i/8)%8,simulated=!!(Math.floor(i/64)%2),effect=Math.floor(i/128)%8,shape=Math.floor(i/1024)%16,terrain=Math.floor(i/16384)%2?TerrainType.MISTY:0,context=Math.floor(i/32768)%2;
  battle.double=true;scene.gameMode=getGameMode(GameModes.CLASSIC);scene.arena.terrain=terrain?new Terrain(terrain):null;scene.arena.weather=null;scene.arena.ignoreAbilities=false;scene.arena.ignoringEffectSource=null;
  for(let k=0;k<8;k++){
   const p=actors[k];p.resetSummonData();p.resetTurnData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies([SpeciesId.BULBASAUR,SpeciesId.PIDGEY,SpeciesId.DRATINI,SpeciesId.CHARMANDER][k%4]);p.formIndex=0;p.fusionSpecies=null;p.customPokemonData.ability=k===u&&shape!==1?AbilityId.POISON_PUPPETEER:AbilityId.BALL_FETCH;p.passive=k===u&&[1,2].includes(shape);p.customPokemonData.passive=p.passive?AbilityId.POISON_PUPPETEER:AbilityId.NONE;p.abilityIndex=0;p.level=[1,50,256,1000000][(Math.floor(i/1024)+k)%4];p.gender=-1;p.isTerastallized=false;p.teraType=PokemonType.STELLAR;p.calculateStats();p.hp=p.getMaxHp();p.status=null;p.moveset=[new PokemonMove(MoveId.SPLASH)];p.visible=false;p.switchOutStatus=false;p.summonData.abilitySuppressed=k===u&&shape===3;p.summonData.addedType=null;p.generateName();
   if(k===u&&shape===4)p.hp=0;
  }
  const target=actors[t];if(shape===5)target.hp=0;
  if([6,8,9].includes(shape)){if(shape===8){target.passive=true;target.customPokemonData.passive=AbilityId.OWN_TEMPO;}else target.customPokemonData.ability=AbilityId.OWN_TEMPO;if(shape===9)target.summonData.abilitySuppressed=true;}
  if([10,11].includes(shape)){target.customPokemonData.ability=AbilityId.LEVITATE;if(shape===11)target.summonData.abilitySuppressed=true;}
  if(shape===12||shape===13){target.isTerastallized=true;target.teraType=shape===12?PokemonType.FLYING:PokemonType.GROUND;}
  if(shape===14){actors[u].customPokemonData.ability=AbilityId.NONE;actors[u].passive=true;actors[u].customPokemonData.passive=AbilityId.POISON_PUPPETEER;}
  if(shape===15)actors[u].switchOutStatus=true;
  if(shape===7){const old=scene.arena.terrain;scene.arena.terrain=null;target.addTag(BattlerTagType.CONFUSED,3,MoveId.CONFUSE_RAY,target.id);scene.arena.terrain=old;}
'''+s[b:]
put('pm.currentPhase=pm.create("MovePhase",actors[0],[2],new PokemonMove(MoveId.SPLASH),MoveUseMode.NORMAL);','pm.currentPhase=context?pm.create("MoveEffectPhase",0,[2],allMoves[MoveId.TOXIC],MoveUseMode.NORMAL):pm.create("MovePhase",actors[0],[2],new PokemonMove(MoveId.SPLASH),MoveUseMode.NORMAL);')
s=s.replace('`entry-allies-${seed}-${i}`','`puppeteer-${seed}-${i}`')
a=s.index('  const before=actors.map');b=s.index('\n }\n if(',a)
s=s[:a]+'''  const before=target.getTag(BattlerTagType.CONFUSED),oldRolls=rolls.mock.calls.length;
  write([`C ${i} ${u} ${t} ${+simulated} ${effect} ${terrain} ${context}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`),...tags("IT")]);
  applyAbAttrs("ConfusionOnStatusEffectAbAttr",{pokemon:actors[u],opponent:target,effect,simulated});
  const after=target.getTag(BattlerTagType.CONFUSED);added+=+(!before&&!!after);blockedDraws+=+(rolls.mock.calls.length>oldRolls&&!after);
  write([...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`),...tags("OT"),...queues(),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E"]);
'''+s[b:]
a=s.index(' if(calls.some');b=s.index('\n},600000);',a)
s=s[:a]+''' const durations=rolls.mock.calls.flatMap((args,k)=>args[0]===2&&args[1]===5?[rolls.mock.results[k].value]:[]),hist=[2,3,4,5].map(v=>durations.filter(x=>x===v).length);
 if(applied.mock.calls.length<1000||added<1000||blockedDraws<100||hist.some(n=>n<100))throw Error(`coverage ${applied.mock.calls.length}/${added}/${blockedDraws}/${hist}`);
 write([`PUPPETEER_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`PUPPETEER applied=${applied.mock.calls.length} added=${added} blockedDraws=${blockedDraws} durations=${hist}`);
'''+s[b:]
put('it("native ally entry callbacks and gain filtering"','it("native Poison Puppeteer gates and spent Misty Terrain RNG"')
Path(sys.argv[1]).write_text(s)

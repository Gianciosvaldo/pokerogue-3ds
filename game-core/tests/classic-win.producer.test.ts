/* Real pinned Classic GameOverPhase bookkeeping and queued reward/unlock calls.
 * Graphics, time delivery and external history/session storage are bounded. */
import { audioManager } from "#app/global-audio-manager";
import { speciesDataRegistry } from "#app/global-species-data-registry";
import { AbilityId } from "#enums/ability-id";
import { MoveId } from "#enums/move-id";
import { SpeciesId } from "#enums/species-id";
import { PokemonMove } from "#moves/pokemon-move";
import { GameOverPhase } from "#phases/game-over-phase";
import { PokemonData } from "#system/pokemon-data";
import { achvs } from "#system/achv";
import { RibbonData } from "#system/ribbon-data";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import * as fs from "node:fs";
import {expect,it,vi} from "vitest";
it("actual Classic terminal profile caller",async()=>{
 const N=Number(process.env.GC_CLASSIC_N??2000),SEED=Number(process.env.GC_CLASSIC_SEED??777);
 expect(process.versions.node.split(".")[0]).toBe("22");expect(Number.isSafeInteger(N)&&N>=120&&N<=100000).toBe(true);expect(Number.isInteger(SEED)&&SEED>=0&&SEED<=0xffffffff).toBe(true);
 let seed=SEED;const rnd=(n:number)=>{seed=(Math.imul(seed,1103515245)+12345)>>>0;return(seed>>>8)%n;};
 const game=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 game.override.battleStyle("single").startingWave(200).ability(AbilityId.NONE).enemySpecies(SpeciesId.MAGIKARP).enemyMoveset(MoveId.SPLASH);
 await game.classicMode.startBattle(SpeciesId.BULBASAUR);
 const scene=game.scene as any,g=scene.gameData,party=scene.getPlayerParty(),pool=Array.from({length:6},()=>scene.addPlayerPokemon(party[0].species,100));
 const ids=[1,2,3,4,5,6,25,26,172,133,134,143,446];
 const names=["CLASSIC_VICTORY","UNEVOLVED_CLASSIC_VICTORY","_10_RIBBONS","_25_RIBBONS","_50_RIBBONS","_75_RIBBONS","_100_RIBBONS"];
 const achvIds=names.map(name=>(achvs as any)[name].id);
 const rng=()=>Phaser.Math.RND.state().replace(/^!rnd,/,"").split(",").map(Number);
 let queued:any[]=[],historyCalls=0,postCalls=0,phaseEnds=0;
 vi.spyOn(scene.time,"delayedCall").mockImplementation((_delay:any,callback:any)=>{callback();return {} as any;});
 vi.spyOn(scene.ui,"fadeOut").mockResolvedValue(undefined);vi.spyOn(scene.ui,"fadeIn").mockResolvedValue(undefined);
 vi.spyOn(scene.ui,"clearText").mockImplementation(()=>{});vi.spyOn(scene.ui,"setMode").mockResolvedValue(undefined);
 vi.spyOn(scene.ui,"shouldSkipDialogue").mockReturnValue(true);vi.spyOn(scene.ui,"showText").mockImplementation((_text:any,_delay:any,callback:any)=>{callback?.();});
 vi.spyOn(scene.ui.achvBar,"showAchv").mockImplementation(()=>{});
 vi.spyOn(audioManager,"fadeOutBgm").mockImplementation(()=>{});vi.spyOn(audioManager,"playSound").mockImplementation(()=>{});
 vi.spyOn(scene,"setFieldScale").mockImplementation(()=>{});vi.spyOn(scene.arenaBg,"setVisible").mockImplementation(()=>scene.arenaBg);
 vi.spyOn(scene.phaseManager,"hideAbilityBar").mockImplementation(()=>{});
 vi.spyOn(scene.phaseManager,"shiftPhase").mockImplementation(()=>{phaseEnds++;});
 vi.spyOn(g,"getSession").mockResolvedValue(null);
 vi.spyOn(g,"saveRunHistory").mockImplementation((_history:any,_win:any)=>{historyCalls++;});
 const create=scene.phaseManager.create.bind(scene.phaseManager);
 vi.spyOn(scene.phaseManager,"unshiftNew").mockImplementation((name:any,...args:any[])=>{queued.unshift(create(name,...args));});
 vi.spyOn(scene.phaseManager,"unshiftPhase").mockImplementation((phase:any)=>{if(phase.phaseName!=="EndCardPhase")queued.unshift(phase);});
 vi.spyOn(scene.phaseManager,"pushNew").mockImplementation((name:any,...args:any[])=>{expect(name).toBe("PostGameOverPhase");postCalls++;});
 for(const p of pool){vi.spyOn(p,"hideInfo").mockImplementation(()=>{});vi.spyOn(p,"setVisible").mockImplementation(()=>p);}
 const scalars=()=>[g.gameStats.sessionsWon,g.gameStats.ribbonsOwned,...Array.from({length:4},(_,j)=>g.voucherCounts[j]),+Object.hasOwn(g.voucherUnlocks,achvIds[0]),...Array.from({length:4},(_,j)=>+!!g.unlocks[j]),...achvIds.map(id=>+Object.hasOwn(g.achvUnlocks,id))];
 const profile=()=>ids.map(id=>[id,g.starterData[id].classicWinCount,Number(BigInt("0x"+(g.dexData[id].ribbons.toJSON()||"0")))]);
 const opaque=()=>JSON.stringify({stats:Object.fromEntries(Object.entries(g.gameStats).filter(([k])=>!["sessionsWon","ribbonsOwned"].includes(k))),dex:Object.fromEntries(Object.entries(g.dexData).map(([id,data]:any)=>[id,ids.includes(Number(id))?{...data,ribbons:null}:data])),starters:Object.fromEntries(Object.entries(g.starterData).map(([id,data]:any)=>[id,ids.includes(Number(id))?{...data,classicWinCount:null}:data])),achievements:Object.fromEntries(Object.entries(g.achvUnlocks).filter(([k])=>!achvIds.includes(k))),vouchers:Object.fromEntries(Object.entries(g.voucherUnlocks).filter(([k])=>k!==achvIds[0]))},(_k,v)=>typeof v==="bigint"?v.toString():v);
 const lines=[`CLASSIC_WIN 1 ${N}`];let wins=0,defeats=0,fused=0,first=0,plusTotal=0,premiumTotal=0,rootExtra=0;
  const mon = (p: any) => [p.id, p.species.speciesId, p.formIndex, p.fusionSpecies?.speciesId ?? -1,
    p.fusionFormIndex ?? 0, p.level, p.exp, p.hp, p.status?.effect ?? 0, p.getNature(),
    p.nature, p.customPokemonData.nature, p.getAbility(true).id, p.getAbility().id,
    p.hasPassive() ? p.getPassiveAbility().id : 0, p.friendship, p.gender, p.abilityIndex,
    p.fusionAbilityIndex ?? -1, +p.shiny, +p.passive, p.variant, p.luck, +p.pauseEvolutions,
    p.teraType, p.getMoveset().length,
    ...Array.from({length:4}, (_,j) => {const m=p.getMoveset()[j];return m?[m.moveId,m.ppUsed,m.ppUp,m.getMovePp()]:[0,0,0,0];}).flat(),
    ...p.ivs, ...p.stats];

 for(let ci=0;ci<N;ci++){
  const count=1+rnd(6),victory=ci%4!==0,wave=ci%11===0?201:victory?200:1+rnd(200);
  party.length=0;scene.modifiers.length=0;queued=[];historyCalls=postCalls=phaseEnds=0;scene.currentBattle.waveIndex=wave;
  for(let j=0;j<count;j++){const p=pool[(j+ci%6)%6];p.resetSummonData();p.resetBattleAndWaveData();p.species=speciesDataRegistry.getSpecies(ids[rnd(ids.length)]);p.formIndex=0;p.fusionSpecies=ci%3===0&&j===0?speciesDataRegistry.getSpecies(ids[rnd(ids.length)]):null;p.fusionFormIndex=0;p.fusionAbilityIndex=0;p.abilityIndex=0;p.passive=false;p.pauseEvolutions=false;p.level=100;p.exp=rnd(1000000);p.ivs=Array.from({length:6},()=>rnd(32));p.nature=rnd(25);p.customPokemonData.nature=-1;p.moveset=[new PokemonMove(MoveId.TACKLE,rnd(4),rnd(4)),new PokemonMove(MoveId.GROWL,rnd(4),rnd(4))];party.push(p);p.calculateStats();p.hp=victory&&rnd(2)?p.getMaxHp():0;}
  for(const id of ids){g.starterData[id]={...g.starterData[id],classicWinCount:rnd(6)};g.dexData[id].ribbons=new RibbonData(rnd(4)?rnd(1024):RibbonData.CLASSIC);}
  g.gameStats.sessionsWon=ci%7===0?0:1+rnd(100);g.gameStats.ribbonsOwned=[0,9,24,49,74,99][ci%6];
  for(let j=0;j<4;j++){g.voucherCounts[j]=rnd(30);g.unlocks[j]=!!rnd(2);}
  g.achvUnlocks={};g.voucherUnlocks={};achvIds.forEach(id=>{if(rnd(2))g.achvUnlocks[id]=1;});if(rnd(2))g.voucherUnlocks[achvIds[0]]=1;
  scene.money=10000.25+ci;Phaser.Math.RND.sow([`classic-win-${SEED}-${ci}`]);const r=rng(),battleRng=scene.currentBattle.rngState;
  const before=scalars(),beforeOpaque=opaque(),beforeParty=JSON.stringify(party.map((p:any)=>new PokemonData(p))),beforeMoney=scene.money;
  lines.push(`C ${ci} ${count} ${+victory} ${wave}`,`R ${r.join(" ")}`,`U ${before.join(" ")}`,`I ${ids.length}`);
  profile().forEach(v=>lines.push(`H ${v.join(" ")}`));party.forEach((p:any)=>{const v=mon(p);expect(v.length).toBe(54);v.forEach((n:number)=>expect(Number.isFinite(n)).toBe(true));lines.push(`P ${v.join(" ")}`);});
  const phase=new GameOverPhase(victory);phase.start();
  for(let k=0;k<40&&!postCalls;k++)await Promise.resolve();expect(historyCalls).toBe(1);expect(postCalls).toBe(1);expect(phaseEnds).toBe(1);
  const terminalVictory=+(phase as any).isVictory;
  const unlocks=queued.filter(p=>p.phaseName==="UnlockPhase").length,plus=queued.filter(p=>p.phaseName==="RibbonModifierRewardPhase").length,premium=queued.filter(p=>p.phaseName==="GameOverModifierRewardPhase").length;
  for(const reward of queued){if(reward.phaseName==="UnlockPhase")reward.start();else{expect(["RibbonModifierRewardPhase","GameOverModifierRewardPhase"]).toContain(reward.phaseName);await reward.doReward();}}
  lines.push(`A ${terminalVictory} ${unlocks} ${plus} ${premium} ${historyCalls} ${postCalls}`,`V ${scalars().join(" ")}`,`J ${ids.length}`);profile().forEach(v=>lines.push(`K ${v.join(" ")}`));party.forEach((p:any)=>lines.push(`T ${mon(p).join(" ")}`));lines.push(`S ${rng().join(" ")}`,`E ${ci}`);
  expect(opaque()).toBe(beforeOpaque);expect(JSON.stringify(party.map((p:any)=>new PokemonData(p)))).toBe(beforeParty);expect(scene.money).toBe(beforeMoney);expect(rng()).toEqual(r);expect(scene.currentBattle.rngState).toBe(battleRng);
  wins+=terminalVictory;defeats+=!terminalVictory;fused+=+party.some((p:any)=>p.fusionSpecies);first+=+(scalars()[5]>before[5]);plusTotal+=plus;premiumTotal+=premium;rootExtra+=+party.some((p:any)=>p.species.getRootSpeciesId()!==p.species.getRootSpeciesId(true));
 }
 expect(wins&&defeats&&fused&&first&&plusTotal&&premiumTotal&&rootExtra).toBeGreaterThan(0);
 lines.push(`Z ${N} ${wins} ${defeats} ${fused} ${first} ${plusTotal} ${premiumTotal} ${rootExtra}`);fs.writeFileSync(process.env.GC_CLASSIC_OUT??"/tmp/game-classic-win.txt",lines.join("\n")+"\n");
 console.log(`Classic actual phase wins=${wins} defeats=${defeats} fused=${fused} firstgolden=${first} plus=${plusTotal} premium=${premiumTotal} distinctroots=${rootExtra}`);
},240000);

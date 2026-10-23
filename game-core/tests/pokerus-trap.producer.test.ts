import { BattleScene } from "#app/battle-scene";
import { globalScene } from "#app/global-scene";
import { CommandPhase } from "#phases/command-phase";
import { GameManager } from "#test/framework/game-manager";
import Phaser from "phaser";
import { expect,it,vi } from "vitest";
import fs from "node:fs";
it("native trap refusal text and Pokerus spreading in reordered parties",async()=>{
 const native=BattleScene.prototype.randBattleSeedInt;
 const g=new GameManager(new Phaser.Game({type:Phaser.HEADLESS}));
 await g.classicMode.startBattle(1,4,7);BattleScene.prototype.randBattleSeedInt=native;
 const party=globalScene.getPlayerParty(),pool=party.slice(),enemy=globalScene.getEnemyField()[0];
 const out=["POKERUS_TRAP 1 384 4",`E ${enemy.species.speciesId} ${enemy.formIndex}`];
 const resource=(file:string)=>JSON.parse(fs.readFileSync(`locales/en/${file}.json`,"utf8"));
 const pokemon=resource("pokemon"),ability=resource("ability"),battle=resource("battle"),trigger=resource("ability-trigger");
 const docs={pokemon:Object.fromEntries(Object.entries(pokemon).filter(([,v])=>v===enemy.species.getName())),
  ability:{shadowTag:ability.shadowTag,arenaTrap:ability.arenaTrap},battle:{wildPokemonWithAffix:battle.wildPokemonWithAffix,foePokemonWithAffix:battle.foePokemonWithAffix},"ability-trigger":{arenaTrap:trigger.arenaTrap}};
 for(const [file,doc] of Object.entries(docs))out.push(`L ${file} ${Buffer.from(JSON.stringify(doc)).toString("hex")}`);
 const original=Phaser.Math.RND.state();
 for(const order of [[0,1,2],[0,2,1],[1,0,2],[1,2,0],[2,0,1],[2,1,0]])for(let mask=0;mask<8;mask++)for(let wave=3;wave<11;wave++){
  const seed="pokerus-order-"+(wave%3);globalScene.seed=seed;globalScene.currentBattle.waveIndex=wave;
  party.splice(0,party.length,...order.map(i=>pool[i]));
  for(let i=0;i<3;i++)pool[i].pokerus=!!(mask&(1<<i));
  globalScene.trySpreadPokerus();
  const after=pool.reduce((mask,p,i)=>mask|(+p.pokerus<<i),0);
  expect(Phaser.Math.RND.state()).toBe(original);
  out.push(`S ${seed} ${wave} ${order.join("")} ${mask} ${after}`);
 }
 party.splice(0,party.length,...pool);
 const p=pool[0],phase=new CommandPhase(0) as any;phase.isSwitch=true;
 const messages:string[]=[];
 const mode=vi.spyOn(globalScene.ui,"setMode").mockResolvedValue(undefined as never);
 const show=vi.spyOn(globalScene.ui,"showText").mockImplementation((text:any)=>{messages.push(text);});
 for(const ability of [23,71])for(const ghost of [false,true]){
  p.summonData.types=[ghost?7:11];p.summonData.ability=1;enemy.summonData.ability=ability;
  enemy.hp=enemy.stats[0];messages.length=0;
  const ok=phase.tryLeaveField(2,false);await Promise.resolve();
  expect(Phaser.Math.RND.state()).toBe(original);
  out.push(`T ${ability} ${+ghost} ${+ok} ${messages.length?Buffer.from(messages[0]).toString("hex"):"-"}`);
 }
 show.mockRestore();mode.mockRestore();
 out.push("COMPLETE 384 4");fs.writeFileSync(process.env.POKERUS_TRAP_OUT!,out.join("\n")+"\n");
},120000);

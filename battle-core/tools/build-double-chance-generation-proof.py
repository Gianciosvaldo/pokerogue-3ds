#!/usr/bin/env python3
"""Actual native newBattle with a passthrough observer at its double-chance boundary."""
from pathlib import Path
import subprocess,sys,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'base.ts';subprocess.run([sys.executable,r/'tools/build-double-chance-proof.py',p],check=True);s=p.read_text()
s=s.replace('import { NumberHolder }','import { activeOverrides } from "#app/overrides";\nimport { NumberHolder }')
s=s.replace('const N=16384','const N=32768').replace('process.env.DOUBLE_CHANCE_OUT??"/tmp/double-chance.txt"','process.env.DOUBLE_GENERATION_OUT??"/tmp/double-generation.txt"')
s=s.replace('DOUBLE_CHANCE 1','DOUBLE_GENERATION 1').replace('DOUBLE_CHANCE_COMPLETE','DOUBLE_GENERATION_COMPLETE')
s=s.replace(' const abilities=',' vi.spyOn(activeOverrides,"FIELD_SIZE_OVERRIDE","get").mockReturnValue(null);\n const abilities=',1)
s=s.replace('let changed=0,offFieldApplications=0;','let invoked=0,bossGates=0,doubles=0,wideLevels=0;')
a=s.index('  const owner=');b=s.index('  scene.modifiers.length=0;',a)
s=s[:a]+'''  const u=i%8,kind=Math.floor(i/8)%4,shape=Math.floor(i/32)%8,single=!!(Math.floor(i/256)%2),nLures=Math.floor(i/512)%4,wave=[2,10,49,50,201,256,5850,1000000][Math.floor(i/2048)%8],mode=[GameModes.ENDLESS,GameModes.SPLICED_ENDLESS][Math.floor(i/16384)];
  scene.currentBattle=battle;battle.waveIndex=wave-1;battle.double=!single;scene.gameMode=getGameMode(mode);battle.gameMode=scene.gameMode;scene.seed=`doublegen${seed}case${i}`;scene.offsetGym=false;
  scene.arena.weather=null;scene.arena.terrain=null;scene.arena.ignoreAbilities=false;scene.arena.ignoringEffectSource=null;
'''+s[b:]
a=s.index('  const chance=new NumberHolder');b=s.index('\n }\n if(',a)
s=s[:a]+'''  write([`C ${i} ${u} ${+single} ${wave} ${nLures} ${mode} ${scene.seed}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`)]);
  let nc=0,observed:string[]=[];
  const original=BattleScene.prototype["getDoubleBattleChance"];
  const watch=vi.spyOn(scene,"getDoubleBattleChance").mockImplementation((w:number)=>{nc++;const value=original.call(scene,w);observed=[`V ${value}`,...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`),...queues(),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`];return value;});
  // No exceptions are discarded. The scene performs its real construction and cleanup.
  const next=scene.newBattle();watch.mockRestore();
  if(nc>1)throw Error(`unexpected callback count ${nc}`);
  write([`A ${nc}`,...observed,`G ${next.waveIndex} ${next.battleType} ${+next.double} ${next.enemyLevels.length} ${next.enemyLevels.join(" ")} ${next.battleSeed}`,`S ${rng(Phaser.Math.RND.state())}`,"E"]);
  invoked+=nc;bossGates+=+!nc;doubles+=+next.double;wideLevels+=next.enemyLevels.filter((v:number)=>v>255).length;
'''+s[b:]
a=s.index(' if(calls.some');b=s.index('\n},600000);',a)
s=s[:a]+''' if(calls.some(c=>c.mock.calls.length<100)||invoked!==20480||bossGates!==12288||doubles<1000||wideLevels<1000)throw Error(`coverage ${calls.map(c=>c.mock.calls.length)}/${invoked}/${bossGates}/${doubles}/${wideLevels}`);
 write([`DOUBLE_GENERATION_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`DOUBLE_GENERATION calls=${calls.map(c=>c.mock.calls.length)} invoked=${invoked} bossGates=${bossGates} doubles=${doubles} wideLevels=${wideLevels}`);
'''+s[b:]
s=s.replace('native double encounter chance dispatcher and scene getter','native newBattle double chance callback and generation')
Path(sys.argv[1]).write_text(s)

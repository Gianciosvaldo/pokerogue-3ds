#!/usr/bin/env python3
"""BattleEndPhase's actual allowed-party loop, observed before real money pickup."""
from pathlib import Path
import subprocess,sys,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'base.ts';subprocess.run([sys.executable,r/'tools/build-honey-money-proof.py',p],check=True);s=p.read_text()
s='import { BattleEndPhase } from "#phases/battle-end-phase";\n'+s
s=s.replace('const N=32768+5850','const N=1024').replace('HONEY_MONEY','HONEY_OWNER')
a=s.index(' const calls=');b=s.index(' const queues=',a)
s=s[:a]+''' const calls=vi.spyOn(allAbilities[AbilityId.HONEY_GATHER].getAttrs("MoneyAbAttr")[0],"apply");
 vi.spyOn(BattleEndPhase.prototype,"end").mockImplementation(()=>{});
 // Rendering teardown/continuation happens after the observed prefix.
 for(const p of actors)p.destroy=()=>{};
'''+s[b:]
a=s.index('  const payday=');b=s.index('  battle.double=',a)
s=s[:a]+'''  const payday=false,u=i%8,simulated=false,victory=!!(i&1),shape=Math.floor(i/2)%8;
  const wave=[1,200,256,3000,5849][Math.floor(i/16)%5],input=1234;
'''+s[b:]
# Rotate which two canonical players carry the ability; include reserve, faint, suppression and passive.
s=s.replace('k===u&&![1,6].includes(shape)','[u%6,(u+2)%6].includes(k)&&![1,6].includes(shape)')
s=s.replace('k===u&&[1,2,6].includes(shape)','[u%6,(u+2)%6].includes(k)&&[1,2,6].includes(shape)')
s=s.replace('p.customPokemonData.ability=k===u','p.customPokemonData.ability=k===u')
s=s.replace('  pm.clearAllPhases();pm.pushNew("MessagePhase","anchor");pm.currentPhase=pm.create("MovePhase",actors[0],[2],new PokemonMove(MoveId.SPLASH),MoveUseMode.NORMAL);','  pm.clearAllPhases();pm.currentPhase=pm.create("BattleEndPhase",victory);')
a=s.index('  const beforeCalls=');b=s.index('\n }\n if(',a)
s=s[:a]+'''  scene.money=0;scene.getPlayerParty().splice(0,4,...(i&16?[actors[5],actors[4],actors[1],actors[0]]:[actors[0],actors[1],actors[4],actors[5]]));
  const beforeCalls=calls.mock.calls.length;
  write([`C ${i} ${+victory} ${wave} ${input} ${+(!!(i&16))}`,`W ${scene.waveSeed}`,`R ${rng(Phaser.Math.RND.state())}`,`B ${rng(battle.battleSeedState)}`,...actors.map((p,k)=>`I ${k} ${snapshot(p).join(" ")}`)]);
  let observed=false;const original=battle.pickUpScatteredMoney;
  battle.pickUpScatteredMoney=function(){observed=true;write([`V ${battle.moneyScattered}`,...actors.map((p,k)=>`O ${k} ${snapshot(p).join(" ")}`),...queues(),`RO ${rng(Phaser.Math.RND.state())}`,`BO ${rng(battle.battleSeedState)}`,"E"]);return original.call(this);};
  new BattleEndPhase(victory).start();battle.pickUpScatteredMoney=original;if(!observed)throw Error('missing actual owner boundary');
  changed+=+(calls.mock.calls.length>beforeCalls);offFieldApplications+=+(shape===7&&calls.mock.calls.length>beforeCalls);wide+=+(battle.waveIndex>200);
'''+s[b:]
a=s.index(' if(calls.mock');b=s.index('\n},600000);',a)
s=s[:a]+''' if(calls.mock.calls.length<100||changed<100||offFieldApplications<10)throw Error(`owner coverage ${calls.mock.calls.length}/${changed}/${offFieldApplications}`);
 write([`HONEY_OWNER_COMPLETE ${N}`]);fs.closeSync(fd);console.log(`HONEY_OWNER calls=${calls.mock.calls.length} changed=${changed} offField=${offFieldApplications}`);
'''+s[b:]
s=s.replace('native Honey Gather dispatcher and Pay Day override','native BattleEndPhase Honey Gather prefix')
Path(sys.argv[1]).write_text(s)

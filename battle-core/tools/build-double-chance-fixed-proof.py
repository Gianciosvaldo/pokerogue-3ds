#!/usr/bin/env python3
"""Actual fixed-rival skips, both player genders and Classic/Challenge newBattle; no discarded exceptions."""
from pathlib import Path
import subprocess,sys,tempfile
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'base.ts';subprocess.run([sys.executable,r/'tools/build-double-chance-generation-proof.py',p],check=True);s=p.read_text()
s=s.replace('const N=32768','const N=32768').replace('process.env.DOUBLE_GENERATION_OUT??"/tmp/double-generation.txt"','process.env.DOUBLE_TRAINER_OUT??"/tmp/double-trainer.txt"').replace('DOUBLE_GENERATION 1','DOUBLE_GENERATION 4')
s=s.replace('import { activeOverrides }','import { settings } from "#app/global-settings-manager";\nimport i18next from "i18next";\nimport { activeOverrides }')
s=s.replace(' const abilities=',' vi.spyOn(activeOverrides,"DISABLE_STANDARD_TRAINERS_OVERRIDE","get").mockReturnValue(false);\n const nameIndex=(key:string|undefined)=>{if(!key)return -1;const cut=key.lastIndexOf(".");return Object.keys(i18next.t(key.slice(0,cut),{returnObjects:true}) as any).indexOf(key.slice(cut+1));};\n const abilities=',1)
s=s.replace('let invoked=0,bossGates=0,doubles=0,wideLevels=0;','let invoked=0,bossGates=0,doubles=0,wideLevels=0,trainers=0;')
s=s.replace('wave=[2,10,49,50,201,256,5850,1000000][Math.floor(i/2048)%8],mode=[GameModes.ENDLESS,GameModes.SPLICED_ENDLESS][Math.floor(i/16384)]','wave=[6,7,8,9][Math.floor(i/2048)%4],mode=[GameModes.CLASSIC,GameModes.CHALLENGE][Math.floor(i/8192)%2]')
s=s.replace('  scene.modifiers.length=0;','  settings.general.playerGender=Math.floor(i/16384)?2:1;\n  scene.modifiers.length=0;')
s=s.replace('${mode} ${scene.seed}', '${mode} ${scene.arena.biomeId} ${scene.arena.trainerChance} ${+settings.isPlayerFemale} ${scene.seed}')
s=s.replace('  write([`A ${nc}`,','  trainers+=+!!next.trainer;\n  write([`A ${nc}`,')
s=s.replace('...observed,`G ', '...observed,`T ${next.trainer?.config.trainerType??-1} ${next.trainer?.variant??-1} ${next.trainer?.partyTemplateIndex??-1} ${nameIndex(next.trainer?.nameKey)} ${nameIndex(next.trainer?.partnerNameKey)}`,`G ')
s=s.replace('  invoked+=nc;','  if(next.trainer)scene.field.remove(next.trainer,true);\n  invoked+=nc;')
s=s.replace('invoked!==20480||bossGates!==12288||doubles<1000||wideLevels<1000','invoked<1000||trainers<1000||doubles<1000')
s=s.replace('wideLevels=${wideLevels}', 'wideLevels=${wideLevels} trainers=${trainers}')
s=s.replace('native newBattle double chance callback and generation','native fixed-rival newBattle and both player genders')
Path(sys.argv[1]).write_text(s)

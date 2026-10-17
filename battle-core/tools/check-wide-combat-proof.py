#!/usr/bin/env python3
"""Rebuild real raw-damage/Substitute and save-migration faults."""
from pathlib import Path
import argparse,os,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('leaf',type=Path);p.add_argument('turns',type=Path);a=p.parse_args();a.leaf=a.leaf.resolve();a.turns=a.turns.resolve();r=Path(__file__).resolve().parents[2];env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
leaf=r/'battle-core/build/wide-substitute-test';turn=r/'battle-core/build/wide-combat-test';assert subprocess.run([leaf,a.leaf],env=env).returncode==0;assert subprocess.run([turn,a.turns],env=env).returncode==0
faults=[
 ('raw-int32','damage.c','static double to_dmg_value(double v)\n{\n    v = floor(v);\n    return v > 1 ? v : 1;','static int32_t to_dmg_value(double v)\n{\n    v = floor(v);\n    return v > 1 ? (int32_t)v : 1;'),
 ('ai-int32','ai_damage.c','static double to_dmg_value(double v)\n{\n    v = floor(v);\n    return v > 1 ? v : 1;','static int32_t to_dmg_value(double v)\n{\n    v = floor(v);\n    return v > 1 ? (int32_t)v : 1;'),
 ('round-up','damage.c','v = floor(v);','v = ceil(v);'),
 ('sub-int32','phases.c','sub->hp -= d.damage;','sub->hp -= (int32_t)d.damage;'),
 ('sub-clamp','phases.c','sub->hp -= d.damage;','sub->hp -= fmin(d.damage,sub->hp);'),
 ('sub-bookkeeping','phases.c','user->total_damage_dealt += (int32_t)(d.damage < sub->hp ? d.damage : sub->hp);','user->total_damage_dealt += 1;'),
 ('critical-message','phases.c','if (crit) pr_queue_msg(bt, e, pr_msg0("battle:hitResultCriticalHit"));','if (false) pr_queue_msg(bt, e, pr_msg0("battle:hitResultCriticalHit"));'),
]
flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(r/x) for x in ['battle-core/include','battle-core/gen/out','battle-core/tests','battle-core/build','ai-core/include','ai-core/src']]]
with tempfile.TemporaryDirectory(prefix='wide-combat-guards-',dir=r/'work') as tmp:
 tmp=Path(tmp)
 for name,file,old,new in faults:
  source=(r/('ai-core/src' if file=='ai_damage.c' else 'battle-core/src')/file).read_text();assert source.count(old)==1,(name,source.count(old));src=tmp/(name+'.c');src.write_text(source.replace(old,new,1));test=(r/'battle-core/tests/wide-substitute-test.c').read_text().replace('#include "../../ai-core/src/ai_internal.h"','#include "'+str(r/'ai-core/src/ai_internal.h')+'"').replace('#include "../src/phases.c"','#include "'+str(src if file=='phases.c' else r/'battle-core/src/phases.c')+'"');test_path=tmp/'test.c';test_path.write_text(test);objects=[]
  if file!='phases.c':
   obj=tmp/'fault.o';subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True);objects=[obj]
  binary=tmp/name;subprocess.run(['gcc',*flags,test_path,*objects,r/'battle-core/build/wide-ai/libai.a',r/'battle-core/build/libprcore.a','-lm','-o',binary],check=True,capture_output=True)
  result=subprocess.run([binary,a.leaf],env=env,capture_output=True);assert result.returncode==1,(name,result.returncode,result.stdout[-400:],result.stderr[-400:]);print(name,'caught',flush=True)
 # Save faults use actual old buffers and the production loader.
 gflags=[*flags,*['-I'+str(r/x) for x in ['game-core/include','game-core/src','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out']]]
 build=r/'game-core/build';libs=[build/'libgc.a',build/'reward-core/libreward.a',build/'ai-core/libai.a',build/'run-core/librun.a',build/'run-core/battle-core/libprcore.a']
 save_faults=[('drop-legacy','|| byte_level || sub_hp','|| byte_level'),('unsigned-legacy','int32_t old_hp;','uint32_t old_hp;'),('accept-nonfinite','|| !isfinite(b->tags[j].hp)\n                || b->tags[j].hp != floor(b->tags[j].hp)','')]
 for name,old,new in save_faults:
  s=(r/'game-core/src/gc_save.c').read_text();assert s.count(old)==1,(name,s.count(old));src=tmp/'save.c';src.write_text(s.replace(old,new));obj=tmp/'save.o';binary=tmp/name
  subprocess.run(['gcc',*gflags,'-c',src,'-o',obj],check=True,capture_output=True);subprocess.run(['gcc',*gflags,r/'game-core/tests/wide_combat_save_host_test.c',obj,*libs,'-lm','-o',binary],check=True,capture_output=True);result=subprocess.run([binary],cwd=r,env=env,capture_output=True);assert result.returncode==1,(name,result.returncode,result.stdout[-300:],result.stderr[-300:]);print(name,'caught',flush=True)
 rows=a.leaf.read_text().splitlines();bad={'footer':rows[:-1]+['WIDE_SUBSTITUTE_COMPLETE 1'],'missing-footer':rows[:-1],'tail':rows+['JUNK'],'version':[' '.join([rows[0].split()[0],'4',*rows[0].split()[2:]])]+rows[1:]}
 for name,prefix,col,value in [('case','C ',1,'0.5'),('user','C ',2,'4'),('target','C ',3,'4'),('id','I ',5,'4294967296'),('level','I ',6,'2147483648'),('hp','H ',1,'-1'),('queue','QCOUNT ',1,'33')]:
  index=next(i for i,row in enumerate(rows) if row.startswith(prefix));v=rows[index].split();v[col]=value;changed=rows.copy();changed[index]=' '.join(v);bad[name]=changed
 for name,lines in bad.items():
  path=tmp/'bad.txt';path.write_text('\n'.join(lines)+'\n');result=subprocess.run([leaf,path],env=env,capture_output=True);assert result.returncode==2,(name,result.returncode);print(name,'rejected',flush=True)
 turn_rows=a.turns.read_text().splitlines();row=next(i for i,s in enumerate(turn_rows) if s.startswith('MON '));assert ' lv=' in turn_rows[row]
 import re
 for value in ['0','-1','256.5','2147483648','4294967297','100junk']:
  changed=turn_rows.copy();changed[row]=re.sub(r' lv=[^ ]+',f' lv={value}',changed[row]);path=tmp/'bad-turn.txt';path.write_text('\n'.join(changed)+'\n');result=subprocess.run([turn,path],env=env,capture_output=True);assert result.returncode==2,(value,result.returncode);print('turn-level',value,'rejected',flush=True)
 print(f'{len(faults)+len(save_faults)} runtime faults caught; {len(bad)+6} malformed fixtures rejected')

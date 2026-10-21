#!/usr/bin/env python3
"""Compiled generation/owner faults and malformed native transport controls."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[2];gc=root/'game-core';trace=Path(sys.argv[1]).resolve()
runner=gc/'build/egg_hatch_diff_test'
subprocess.run([runner,trace],check=True)
faults=[
 ('egg-seed','gc_egg_hatch.c','(int)(egg->id&0xffffu)','0',False),
 ('hidden-rate','gc_egg_hatch.c','egg->source_type==3?8:192','egg->source_type==3?8:8',False),
 ('forced-hidden','gc_egg_hatch.c','egg->override_hidden_ability || egg_hidden','egg_hidden',False),
 ('iv-boost','gc_egg_hatch.c','if (value>out->b.ivs[s])','if (false)',False),
 ('egg-form','gc_egg_hatch.c','species,1,ability,-2,0','species,1,ability,0,0',False),
 ('old-zero','gc_egg_hatch.c','if (egg.hatch_waves>0)','if (egg.hatch_waves>=0)',True),
 ('egg-rewards','gc_egg_hatch.c','sys,&mon,true,true,false,false,NULL','sys,&mon,true,false,false,false,NULL',True),
 ('owner-hook','gc_phases.c','            gc_system_hatch_ready_eggs(run);','            (void)run;',True),
]
flags=['-O2','-std=c99','-ffp-contract=off']
for path in ['game-core/include','game-core/src','game-core/gen/out','game-core/tests','battle-core/include','battle-core/gen/out','run-core/include','run-core/gen/out','reward-core/include','reward-core/gen/out','ai-core/include','ai-core/gen/out']:
 flags+=['-I'+str(root/path)]
libs=[gc/'build/libgc.a',gc/'build/reward-core/libreward.a',gc/'build/ai-core/libai.a',gc/'build/run-core/librun.a',gc/'build/run-core/battle-core/libprcore.a']
with tempfile.TemporaryDirectory(prefix='egg-hatch-guards-',dir=root/'work') as tmp:
 tmp=Path(tmp)
 for name,file,old,new,host in faults:
  source=(gc/'src'/file).read_text();assert source.count(old)==1,(name,source.count(old))
  src=tmp/(name+'.c');obj=tmp/(name+'.o');binary=tmp/name
  src.write_text(source.replace(old,new))
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  test=gc/'tests'/('egg_hatch_host_test.c' if host else 'egg_hatch_diff_test.c')
  subprocess.run(['gcc',*flags,test,obj,*libs,'-lm','-o',binary],check=True,capture_output=True)
  r=subprocess.run([binary]+([] if host else [trace]),capture_output=True)
  assert r.returncode==1,(name,r.returncode,r.stdout[-1500:],r.stderr[-1500:])
  print(name,'caught',flush=True)
 rows=trace.read_text().splitlines()
 malformed={'footer':rows[:-1], 'extra':rows+['JUNK'], 'version':[rows[0].replace(' 2 ',' 3 ')]+rows[1:]}
 for name,col,value in [('case',1,'0.5'),('egg-id',2,'4294967296'),('species',3,'9999'),('source',4,'5'),('shiny',5,'2'),('variant',6,'3'),('move',7,'4'),('hidden',8,'2'),('result-id',9,'-1'),('nan',10,'nan'),('iv',18,'32')]:
  t=rows.copy();v=t[1].split();v[col]=value;t[1]=' '.join(v);malformed[name]=t
 for name,lines in malformed.items():
  path=tmp/'bad.txt';path.write_text('\n'.join(lines)+'\n')
  r=subprocess.run([runner,path],capture_output=True)
  assert r.returncode==2,(name,r.returncode)
  print(name,'rejected',flush=True)
 print(f'{len(faults)} compiled faults caught; {len(malformed)} malformed captures rejected')

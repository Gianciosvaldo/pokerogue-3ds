#!/usr/bin/env python3
"""Compile meaningful Baton owner faults and reject malformed native captures."""
from pathlib import Path
import os, subprocess, sys, tempfile
root=Path(__file__).resolve().parents[2]
trace=Path(sys.argv[1]).resolve();owner=Path(sys.argv[2]).resolve()
env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=root/'battle-core/build/baton-switch-test'
subprocess.run([runner,trace],env=env,check=True)
subprocess.run([root/'battle-core/build/doublestest',owner],env=env,check=True,stdout=subprocess.DEVNULL)
faults=[
 ('side-lookup','item_transfer.c','bt->n_mods[0]','bt->n_mods[last->is_player?0:1]'),
 ('recipient-owner','item_transfer.c','m->pokemon_id==(int64_t)incoming->id','m->pokemon_id==(int64_t)last->id'),
 ('donor-owner','item_transfer.c','m->pokemon_id==(int64_t)last->id','m->pokemon_id==(int64_t)incoming->id'),
 ('source-lock','item_transfer.c','if (!recipient_has_baton && baton>=0)','if (!recipient_has_baton && baton>=0 && bt->mods[0][baton].is_transferable)'),
 ('item-loss','item_transfer.c','incoming,1,false,false)','incoming,1,false,true)'),
 ('owner-hook','switching.c','pr_transfer_baton_on_switch(bt,e,last,in);','(void)in;'),
 ('switch-kind','switching.c','if (type == SWITCH_TYPE_BATON_PASS) {','if (true) {'),
]
flags=['-O2','-std=c99','-ffp-contract=off','-I'+str(root/'battle-core/include'),'-I'+str(root/'battle-core/gen/out')]
with tempfile.TemporaryDirectory(prefix='baton-guards-',dir=root/'work') as tmp:
 tmp=Path(tmp)
 for name,file,old,new in faults:
  source=(root/'battle-core/src'/file).read_text()
  cut=source.index('void pr_transfer_baton_on_switch(') if file=='item_transfer.c' else source.index('void pr_switch_summon_phase(')
  prefix=source[:cut];scope=source[cut:]
  assert scope.count(old)==1,(name,scope.count(old))
  if name=='side-lookup':
   scope=scope.replace('bt->mods[0]', 'bt->mods[last->is_player?0:1]').replace('bt,e,0,baton','bt,e,last->is_player?0:1,baton')
  src=tmp/(name+'.c');obj=tmp/(name+'.o');binary=tmp/name
  src.write_text(prefix+scope.replace(old,new))
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  test='baton-switch-test.c' if file=='item_transfer.c' else 'doublestest.c'
  subprocess.run(['gcc',*flags,root/'battle-core/tests'/test,obj,root/'battle-core/build/libprcore.a','-lm','-o',binary],check=True,capture_output=True)
  result=subprocess.run([binary,trace if file=='item_transfer.c' else owner],env=env,capture_output=True)
  assert result.returncode==1,(name,result.returncode,result.stdout[-2000:],result.stderr[-2000:])
  print(name,'caught',flush=True)
 rows=trace.read_text().splitlines()
 malformed={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],
  'version':[rows[0].replace(' 2 ',' 3 ',1)]+rows[1:],
  'count':[rows[0].replace('256','255')]+rows[1:],
  'footer':rows[:-1]+['BATONSWITCH_COMPLETE 1']}
 for name,prefix,col,value in [('fractional-case','C ',1,'0.5'),('side','C ',2,'2'),('field','C ',3,'2'),('kind','C ',4,'5'),('can','C ',5,'2'),('unknown-item','M ',1,'UNKNOWN'),('owner','M ',3,'6'),('negative-stack','M ',4,'-1'),('virtual','M ',5,'-1'),('lock','M ',6,'2'),('id','MON ',1,'4294967296'),('nan-actor','A ',1,'nan'),('tier','BATON_TIERS ',1,'6')]:
  i=next(i for i,r in enumerate(rows) if r.startswith(prefix));v=rows[i].split();v[col]=value;t=rows.copy();t[i]=' '.join(v);malformed[name]=t
 for name,lines in malformed.items():
  path=tmp/'bad.txt';path.write_text('\n'.join(lines)+'\n')
  result=subprocess.run([runner,path],env=env,capture_output=True)
  assert result.returncode==2,(name,result.returncode,result.stdout[-1000:],result.stderr[-1000:])
  print(name,'rejected',flush=True)
 print(f'{len(faults)} compiled faults caught; {len(malformed)} malformed captures rejected')

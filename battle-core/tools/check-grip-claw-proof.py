#!/usr/bin/env python3
"""Compiled callback/owner faults and malformed source-observation controls."""
from pathlib import Path
import os,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();owner=Path(sys.argv[2]).resolve()
env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=root/'battle-core/build/grip-claw-test'
subprocess.run([runner,trace],env=env,check=True)
subprocess.run([root/'battle-core/build/doublestest',owner],env=env,check=True,stdout=subprocess.DEVNULL)
faults=[
 ('target-range','item_transfer.c','(void)pr_battle_rand_seed_int(bt,1,0);','(void)pr_battle_rand_seed_int(bt,2,0);'),
 ('chance-stream','item_transfer.c','pr_rng_seed_float(&bt->rng)>modifier->mult','pr_rng_seed_float(&bt->battle_rng)>modifier->mult'),
 ('chance-boundary','item_transfer.c','pr_rng_seed_float(&bt->rng)>modifier->mult','pr_rng_seed_float(&bt->rng)>=modifier->mult'),
 ('chance-stack','item_transfer.c','modifier->mult*pr_mod_get_stack_count(modifier)','modifier->mult'),
 ('chance-virtual','item_transfer.c','modifier->mult*pr_mod_get_stack_count(modifier)','modifier->mult*modifier->stack_count'),
 ('stock-owner','item_transfer.c','m->pokemon_id==(int64_t)target->id','m->pokemon_id!=(int64_t)target->id'),
 ('stock-lock','item_transfer.c','&& m->is_transferable) indices[count++]=k;',' ) indices[count++]=k;'),
 ('stock-draw','item_transfer.c','indices[pr_battle_rand_seed_int(bt,count,0)]','indices[0]'),
 ('raw-hp','item_transfer.c','if (!target) return false;','if (!target || user->hp<=0) return false;'),
 ('no-target-rng','item_transfer.c','if (!target) return false;','if (!target) { (void)pr_rng_seed_float(&bt->rng); return false; }'),
 ('filtered-hp','item_transfer.c','if (!user || user->hp<=0) return;','if (!user) return;'),
 ('item-loss','item_transfer.c','pr_try_transfer_held_item(bt,e,side,index,user,1,false,true)','pr_try_transfer_held_item(bt,e,side,index,user,1,false,false)'),
 ('owner-hook','phases.c','pr_apply_contact_held_item_transfers(bt,e,user,target);','(void)target;'),
 ('noncontact-attacks','phases.c','if (pr_move_is_attack(move)) {\n        pr_apply_contact_held_item_transfers','if (pr_move_is_attack(move) && pr_moves[move].category==MOVE_CATEGORY_PHYSICAL) {\n        pr_apply_contact_held_item_transfers'),
]
flags=['-O2','-std=c99','-ffp-contract=off','-I'+str(root/'battle-core/include'),'-I'+str(root/'battle-core/gen/out')]
with tempfile.TemporaryDirectory(prefix='grip-guards-',dir=root/'work') as tmp:
 tmp=Path(tmp)
 for name,file,old,new in faults:
  source=(root/'battle-core/src'/file).read_text()
  if file=='item_transfer.c':cut=source.index('bool pr_apply_contact_held_item_transfer(');prefix=source[:cut];scope=source[cut:]
  else:prefix='';scope=source
  assert scope.count(old)==1,(name,scope.count(old))
  src=tmp/(name+'.c');obj=tmp/(name+'.o');binary=tmp/name
  src.write_text(prefix+scope.replace(old,new))
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  test='grip-claw-test.c' if file=='item_transfer.c' else 'doublestest.c'
  subprocess.run(['gcc',*flags,root/'battle-core/tests'/test,obj,root/'battle-core/build/libprcore.a','-lm','-o',binary],check=True,capture_output=True)
  result=subprocess.run([binary,trace if file=='item_transfer.c' else owner],env=env,capture_output=True)
  assert result.returncode==1,(name,result.returncode,result.stdout[-3000:],result.stderr[-3000:])
  print(name,'caught',flush=True)
 rows=trace.read_text().splitlines()
 malformed={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],
  'version':[rows[0].replace(' 1 ',' 2 ',1)]+rows[1:],
  'footer':rows[:-1]+['GRIPCLAW_COMPLETE 1']}
 for name,prefix,col,value in [('fractional-case','C ',1,'0.5'),('holder','C ',2,'4'),('target','C ',3,'-2'),('mode','C ',4,'2'),('result','C ',5,'2'),('unknown-item','M ',1,'UNKNOWN'),('owner','M ',3,'4'),('negative-stack','M ',4,'-1'),('virtual','M ',5,'-1'),('lock','M ',6,'2'),('id','MON ',1,'4294967296'),('nan-actor','A ',1,'nan'),('tier','GRIPCLAW_TIERS ',1,'6')]:
  i=next(i for i,r in enumerate(rows) if r.startswith(prefix));v=rows[i].split();v[col]=value;t=rows.copy();t[i]=' '.join(v);malformed[name]=t
 for name,lines in malformed.items():
  path=tmp/'bad.txt';path.write_text('\n'.join(lines)+'\n')
  result=subprocess.run([runner,path],env=env,capture_output=True)
  assert result.returncode==2,(name,result.returncode)
  print(name,'rejected',flush=True)
 print(f'{len(faults)} compiled faults caught; {len(malformed)} malformed captures rejected')

#!/usr/bin/env python3
"""Actual-source parity faults and malformed captures; production files untouched."""
from pathlib import Path
import argparse,os,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('trace',type=Path);a=p.parse_args();a.trace=a.trace.resolve()
r=Path(__file__).resolve().parents[2];env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
exe=r/'battle-core/build/berry-moves-test';assert subprocess.run([exe,a.trace],env=env).returncode==0
faults=[
 ('eat-hp','move_effects.c','if(!steal&&owner->hp<=0)return false;','if(false)return false;'),
 ('steal-hp','move_effects.c','if(!steal&&owner->hp<=0)return false;','if(owner->hp<=0)return false;'),
 ('sticky','move_effects.c','if(cancelled)return false;','if(false)return false;'),
 ('choice-rng','move_effects.c','indices[pr_battle_rand_seed_int(bt,count,0)]','indices[0]'),
 ('lock','move_effects.c','if(pr_mod_is(m,PR_MOD_BERRY)&&m->pokemon_id','if(m->is_transferable&&pr_mod_is(m,PR_MOD_BERRY)&&m->pokemon_id'),
 ('preserve','move_effects.c','<pr_mod_get_stack_count(pouch)*3','<0'),
 ('pouch-rng','move_effects.c','pr_battle_rand_seed_int(bt,10,0)','pr_rng_seed_int(&bt->rng,10,0)'),
 ('consume-preserved','move_effects.c','if(!preserve){pr_lose_held_item','if(true){pr_lose_held_item'),
 ('consumer','move_effects.c','pr_pokemon *consumer=steal?user:target;','pr_pokemon *consumer=owner;'),
 ('effect','move_effects.c','pr_berry_effect(bt,e,consumer,chosen.berry_type,NULL,NULL,NULL);','pr_berry_effect(bt,e,consumer,BERRY_TYPE_SITRUS,NULL,NULL,NULL);'),
 ('donor-stock','move_effects.c','pr_lose_held_item(bt,e,owner,&bt->mods[side][index]);','(void)index;'),
 ('virtual-refresh','move_effects.c','pr_refresh_berry_modifiers(bt,side);','(void)side;'),
 ('preserved-history','move_effects.c','steal?consumer==target:!preserve','steal?consumer==target:true'),
 ('stolen-history','move_effects.c','steal?consumer==target:!preserve','!preserve'),
 ('history-actor','move_effects.c','pr_record_eaten_berry(bt,consumer,chosen.berry_type','pr_record_eaten_berry(bt,owner,chosen.berry_type'),
 ('cheek-pouch','move_effects.c','callback.pokemon=consumer;pr_apply_ab_attrs(bt,PR_AC_HealFromBerryUseAbAttr,&callback);','callback.pokemon=consumer;(void)callback;'),
 ('loss-owner','move_effects.c','pr_pokemon *callback_owner=steal?target:consumer;','pr_pokemon *callback_owner=consumer;'),
 ('restriction-side','modifiers.c','int side=player?0:1;','int side=player?1:0;'),
 ('restriction-stack','modifiers.c','if(pr_mod_is(&bt->mods[side][k],PR_MOD_BERRY))return true;','if(pr_mod_is(&bt->mods[side][k],PR_MOD_BERRY)&&bt->mods[side][k].stack_count>1)return true;'),
 ('eat-dispatch','move_effects.c','pr_apply_eat_berry_move(bt,e,user,target,P(EatBerryAttr,a)->selfTarget,false);','(void)target;'),
 ('steal-dispatch','move_effects.c','pr_apply_eat_berry_move(bt,e,user,target,false,true);','(void)target;'),
]
flags=['-O2','-std=c99','-ffp-contract=off','-I'+str(r/'battle-core/include'),'-I'+str(r/'battle-core/gen/out')]
with tempfile.TemporaryDirectory(prefix='berry-move-guards-',dir=r/'work')as tmp:
 tmp=Path(tmp)
 for name,file,old,new in faults:
  s=(r/'battle-core/src'/file).read_text()
  if file=='move_effects.c' and name not in ('eat-dispatch','steal-dispatch'):
   start=s.index('bool pr_apply_eat_berry_move(');end=s.index('void pr_apply_move_effect(',start);scope=s[start:end];prefix=s[:start];suffix=s[end:]
  else:prefix='';scope=s;suffix=''
  assert scope.count(old)==1,(name,scope.count(old));source=tmp/(name+'.c');obj=tmp/(name+'.o');binary=tmp/name;source.write_text(prefix+scope.replace(old,new)+suffix)
  subprocess.run(['gcc',*flags,'-c',source,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,r/'battle-core/tests/berry-moves-test.c',obj,r/'battle-core/build/libprcore.a','-Wl,--wrap=pr_apply_ab_attrs','-lm','-o',binary],check=True,capture_output=True)
  result=subprocess.run([binary,a.trace],env=env,capture_output=True)
  assert result.returncode==1,(name,result.returncode,result.stdout[-500:],result.stderr[-500:]);print(name,'caught',flush=True)
 rows=a.trace.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'version':[rows[0].replace(' 3 ',' 4 ',1)]+rows[1:],'footer':rows[:-1]+['BERRYMOVES_COMPLETE 1']}
 for name,prefix,col,value in [('case','C ',1,'0.5'),('mode','C ',2,'4'),('call','C ',3,'2'),('user','C ',4,'4'),('target','C ',5,'4'),('id','I ',5,'4294967296'),('hp','I ',8,'-1'),('tag-mask','IX ',2,'32'),('history-length','IX ',22,'9'),('unknown-stock','M ',1,'UNKNOWN'),('owner','M ',2,'4'),('virtual','M ',5,'-1'),('lock','M ',6,'2'),('select','CHECK ',1,'2'),('condition','CHECK ',2,'2'),('restriction','CHECK ',3,'2'),('callback-count','CALLS ',1,'33'),('callback-kind','AB ',1,'4'),('callback-actor','AB ',2,'4')]:
  index=next(i for i,row in enumerate(rows)if row.startswith(prefix));v=rows[index].split();v[col]=value;malformed=rows.copy();malformed[index]=' '.join(v);bad[name]=malformed
 for prefix in ('CHECK ','CALLS ','QCOUNT ','RESULT '):
  index=next(i for i,row in enumerate(rows)if row.startswith(prefix));bad['missing-'+prefix.strip()]=rows[:index]+rows[index+1:]
 for name,lines in bad.items():
  path=tmp/'bad.txt';path.write_text('\n'.join(lines)+'\n');result=subprocess.run([exe,path],env=env,capture_output=True);assert result.returncode==2,(name,result.returncode);print(name,'rejected',flush=True)
 print(f'{len(faults)} compiled runtime faults caught; {len(bad)} malformed captures rejected')

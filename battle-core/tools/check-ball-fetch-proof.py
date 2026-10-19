#!/usr/bin/env python3
"""Compile real inventory, callback, history and owning-dispatch faults."""
from pathlib import Path
import os,sys,subprocess,tempfile
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();turns=Path(sys.argv[2]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=r/'battle-core/build/ball-fetch-test'
with turns.open() as f: assert sum(line.startswith('TRACE list=') and 'ShowAbilityPhase' in line for line in f)>=4096,'native owning activation floor'
subprocess.run([runner,trace],check=True,env=env);subprocess.run([r/'battle-core/build/wide-combat-test',turns],check=True,env=env)
s=(r/'battle-core/src/ab_attrs.c').read_text();a=s.index('    case PR_AC_FetchBallAbAttr: {');b=s.index('\n    /* ---- secondary effect chance',a);leaf=s[a:b];faults=[]
def fault(name,old,new):
 assert leaf.count(old)==1,(name,leaf.count(old));faults.append((name,'ab_attrs.c',s[:a]+leaf.replace(old,new,1)+s[b:],False))
fault('missing-simulation-gate','!pp->simulated && ','')
# A real scene still has its engine during simulation. Keep the mutant callable
# so this fault is checked for exact state differences, rather than a null crash.
name,file,changed,own=faults.pop();faults.append((name,file,changed.replace('pr_queue_msg(bt,c->e,pr_msg2("abilityTriggers:fetchBall",','pr_queue_msg(bt,bt->engine,pr_msg2("abilityTriggers:fetchBall",',1),own))
fault('missing-player-gate',' && p->is_player','')
fault('reject-pokeball-zero','bt->last_used_pokeball >= 0','bt->last_used_pokeball > 0')
fault('wrong-inventory-slot','bt->pokeball_counts[ball]++','bt->pokeball_counts[(ball+1)%6]++')
fault('missing-inventory-increment','bt->pokeball_counts[ball]++','(void)bt')
fault('double-increment','bt->pokeball_counts[ball]++','bt->pokeball_counts[ball]+=2')
fault('reward-cap','bt->pokeball_counts[ball]++','bt->pokeball_counts[ball]=fmin(99,bt->pokeball_counts[ball]+1)')
fault('byte-inventory','bt->pokeball_counts[ball]++','bt->pokeball_counts[ball]=(uint8_t)(fmod(bt->pokeball_counts[ball],256)+1)')
fault('missing-consumption','bt->last_used_pokeball=-1;','/* keep last used */')
fault('missing-message','pr_queue_msg(bt,c->e,pr_msg2("abilityTriggers:fetchBall",','if(false)pr_queue_msg(bt,c->e,pr_msg2("abilityTriggers:fetchBall",')
fault('wrong-message-ball','names[ball]','names[(ball+1)%6]')
fault('wrong-message-user','pr_arg_pokemon(bt,"pokemonNameWithAffix",p)','pr_arg_pokemon(bt,"pokemonNameWithAffix",bt->field[3])')
for name,file,old,new,own in [
 ('missing-once-history','hooks.c','if(p->ab_applied_wave[i]==ability)return true;','if(false)return true;',False),
 ('wrong-once-history','hooks.c','p->n_ab_applied_wave;i++)if(p->ab_applied_wave[i]','p->n_ab_applied_summon;i++)if(p->ab_applied_summon[i]',False),
 ('missing-wave-record','ab_attrs.c','set_add(bt, w->ab_applied_wave, &w->n_ab_applied_wave, ability);','/* omitted */',False),
 ('missing-summon-record','ab_attrs.c','set_add(bt, w->ab_applied_summon, &w->n_ab_applied_summon, ability);','/* omitted */',False),
 ('missing-owning-fetch','ab_attrs.c','bt->pokeball_counts[ball]++;','/* omitted */',True)]:
 original=(r/'battle-core/src'/file).read_text();assert original.count(old)==1,(name,original.count(old));faults.append((name,file,original.replace(old,new,1),own))
flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(r/x) for x in ['battle-core/include','battle-core/gen/out','battle-core/build','battle-core/tests']]]
with tempfile.TemporaryDirectory(prefix='ball-fetch-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,file,changed,own in faults:
  src=d/file;src.write_text(changed);obj=d/'fault.o';exe=d/'fault'
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  test=r/('battle-core/build/wide-combat-replay.c' if own else 'battle-core/tests/ball-fetch-test.c')
  subprocess.run(['gcc',*flags,test,obj,r/'battle-core/build/libprcore.a','-lm','-o',exe],check=True,capture_output=True)
  result=subprocess.run([exe,turns if own else trace],env=env,capture_output=True)
  assert result.returncode==1,(name,result.returncode,result.stdout[-500:],result.stderr[-500:]);print(name,'caught',flush=True)
 rows=trace.read_text().splitlines();bad={'footer':rows[:-1]+['BALLFETCH_COMPLETE 1'],'missing-footer':rows[:-1],'tail':rows+['JUNK'],'version':['BALLFETCH 2 16384 777']+rows[1:],'too-few':['BALLFETCH 1 8192 777']+rows[1:]}
 for name,prefix,col,value in [('case','C ',1,'0.5'),('user','C ',2,'4'),('only','C ',3,'3'),('simulated','C ',4,'2'),('buddy','C ',5,'2'),('last-ball','INV ',1,'6'),('negative-count','INV ',2,'-1'),('fractional-count','INV ',2,'0.5'),('nan','INV ',2,'NaN'),('count-overflow','INV ',2,'9007199254740994'),('level','I ',6,'2147483648'),('step','STEP ',1,'2'),('queue','Q ',1,'65'),('levels','Q ',2,'0')]:
  ix=next(i for i,row in enumerate(rows) if row.startswith(prefix));row=rows[ix].split();row[col]=value;changed=rows.copy();changed[ix]=' '.join(row);bad[name]=changed
 for name,changed in bad.items():
  f=d/'bad.txt';f.write_text('\n'.join(changed)+'\n');result=subprocess.run([runner,f],env=env,capture_output=True);assert result.returncode==2,(name,result.returncode);print(name,'rejected',flush=True)
 print(f'{len(faults)} runtime faults caught; {len(bad)} malformed captures rejected')

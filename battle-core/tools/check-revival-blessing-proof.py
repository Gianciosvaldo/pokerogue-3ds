#!/usr/bin/env python3
"""Compile real implementation faults and reject malformed native captures."""
from pathlib import Path
import os,sys,subprocess,tempfile
r=Path(__file__).resolve().parents[2]
trace=Path(sys.argv[1]).resolve();env=os.environ.copy()
assert env.get('TURNTEST_LOCALES')
turns=Path(sys.argv[2]).resolve()
with turns.open() as f: assert sum(line.startswith('TRACE list=') and 'RevivalBlessingPhase' in line and 'ResetStatusPhase' in line for line in f)>=4096, 'native player and trainer owning coverage'
subprocess.run([r/'battle-core/build/revival-turn-test',turns],check=True,env=env)
runner=r/'battle-core/build/revival-blessing-test'
subprocess.run([runner,trace],check=True,env=env)
s=(r/'battle-core/src/phases.c').read_text();a=s.index('bool pr_has_revivable_party(');b=s.index('\n/* Pokemon.cureStatus',a)
leaf=s[a:b];faults=[]
def fault(name,old,new):
 assert leaf.count(old)==1,(name,leaf.count(old))
 faults.append((name,'phases.c',s[:a]+leaf.replace(old,new,1)+s[b:]))
fault('extra-user-hp-gate','if(user->is_player) {','if(user->hp<=0)return false; if(user->is_player) {')
fault('extra-parent-condition-gate','if(user->is_player) {','if(!pr_has_revivable_party(bt,user))return false; if(user->is_player) {')
fault('trainer-gate','(!user->is_player && !user->has_trainer)','false')
fault('boss-filter','if(p->hp<=0&&!p->is_boss)fainted[n++]=p;','if(p->hp<=0)fainted[n++]=p;')
fault('field-only-party','if(p->hp<=0&&!p->is_boss)fainted[n++]=p;','if(i<2 && p->hp<=0&&!p->is_boss)fainted[n++]=p;')
name,filename,changed=faults.pop();faults.append((name,filename,changed.replace('pr_pokemon *p=fainted[','if(!n)return false; pr_pokemon *p=fainted[',1)))
fault('reverse-party-order','fainted[n++]=p;','fainted[n++]=bt->party[1][bt->n_party[1]-1-i];')
fault('skip-singleton-api','pr_battle_rand_seed_int(bt,n,0)','(n==1?0:pr_battle_rand_seed_int(bt,n,0))')
fault('wrong-rng','pr_battle_rand_seed_int(bt,n,0)','pr_rng_seed_int(&bt->rng,n,0)')
fault('temporary-hp','pr_get_stat(p,STAT_HP,true)','pr_get_stat(p,STAT_HP,false)')
fault('missing-minimum','to_dmg_value(0.5*mh)','floor(0.5*mh)')
fault('ceil-heal','to_dmg_value(0.5*mh)','ceil(0.5*mh)')
fault('wrong-heal-fraction','0.5*mh','0.25*mh')
fault('eager-enemy-status','pr_reset_status_phase(bt,e,p,true);','clear_status(bt,e,p,false);')
fault('missing-player-turn-reset','pr_reset_turn_data(p);clear_status','clear_status')
fault('deferred-player-status','clear_status(bt,e,p,false);revival_heal','pr_reset_status_phase(bt,e,p,true);revival_heal')
fault('enemy-plain-name','pr_arg_pokemon(bt,"pokemonName",p)','pr_arg_pokemon_name(bt,"pokemonName",p)')
fault('player-wrong-recipient','pr_arg_pokemon_name(bt,"pokemonName",p)','pr_arg_pokemon_name(bt,"pokemonName",user)')
fault('wrong-switch-removal-field','pr_remove_switch_summon_field(bt,e,selected);','pr_remove_switch_summon_field(bt,e,1);')
fault('missing-switch-removal','pr_remove_switch_summon_field(bt,e,selected);','/* omitted */')
fault('missing-cancel','pr_cancel_queued_move(e,p);','/* omitted */')
fault('wrong-selected-slot','s->sw_slot=(int8_t)slot','s->sw_slot=0')
fault('return-switch','s->sw_do_return=false','s->sw_do_return=true')
fault('wrong-player-side','s->sw_player=player','s->sw_player=!player')
fault('wrong-toggle','e->pool[ph].position_double=true','e->pool[ph].position_double=false')
fault('missing-player-active-slot-condition','selected<=1||ally->hp<=0','ally->hp<=0')
eng=(r/'battle-core/src/engine.c').read_text()
for name,old,new in [('missing-prompt','e->pool[ph].message_prompt=true','e->pool[ph].message_prompt=false'),('missing-cancel-result','p->cancelled=true;p->entry.result=MOVE_RESULT_FAIL;','p->cancelled=true;'),('cancel-all-cached','p->cancelled=true;p->entry.result=MOVE_RESULT_FAIL;\n        return;','p->cancelled=true;p->entry.result=MOVE_RESULT_FAIL;')]:
 assert eng.count(old)==1,(name,eng.count(old));faults.append((name,'engine.c',eng.replace(old,new,1)))
flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(r/x) for x in ['battle-core/include','battle-core/gen/out','battle-core/build','battle-core/tests']]]
with tempfile.TemporaryDirectory(prefix='revival-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,filename,changed in faults:
  src=d/filename;src.write_text(changed);obj=d/'fault.o';exe=d/'fault'
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,r/'battle-core/tests/revival-blessing-test.c',obj,r/'battle-core/build/libprcore.a','-Wl,--wrap=pr_battle_rand_seed_int','-Wl,--wrap=pr_revival_blessing','-lm','-o',exe],check=True,capture_output=True)
  result=subprocess.run([exe,trace],env=env,capture_output=True)
  assert result.returncode==1,(name,result.returncode,result.stdout[-500:],result.stderr[-500:])
  print(name,'caught',flush=True)
 # These faults require the real turn dispatcher and party callback to execute.
 for name,filename,old,new in [
  ('missing-owning-dispatch','move_effects.c','pr_revival_blessing(bt,e,user);','/* omitted */'),
  ('missing-owning-party-pause','phases.c','case PH_REVIVAL_BLESSING: e->awaiting_pick = true; break;','case PH_REVIVAL_BLESSING: break;'),
  ('missing-resume-choice','engine.c','pr_finish_revival_choice(bt, e, s->pokemon, slot);','/* omitted */')]:
  original=(r/'battle-core/src'/filename).read_text();assert original.count(old)==1
  src=d/filename;src.write_text(original.replace(old,new,1));obj=d/'own.o';exe=d/'own'
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,'-DTURNTEST_REVIVAL_FAINTED_PICK',r/'battle-core/build/wide-combat-replay.c',obj,r/'battle-core/build/libprcore.a','-lm','-o',exe],check=True,capture_output=True)
  result=subprocess.run([exe,turns],env=env,capture_output=True)
  assert result.returncode==1,(name,result.returncode,result.stderr[-500:]);print(name,'caught',flush=True)
 rows=trace.read_text().splitlines()
 bad={'footer':rows[:-1]+['REVIVAL_COMPLETE 1'],'missing-footer':rows[:-1],'tail':rows+['JUNK'],'version':['REVIVAL 5 8704 777']+rows[1:],'fractional-version':['REVIVAL 2.5 8704 777']+rows[1:],'too-few':['REVIVAL 4 8191 777']+rows[1:]}
 for name,prefix,col,value in [('case','C ',1,'0.5'),('mode','C ',2,'2'),('user','C ',3,'8'),('choice','C ',4,'6'),('level','I ',6,'2147483648'),('nan','I ',25,'NaN'),('status','SI ',2,'8'),('boss','IB ',2,'2'),('condition','COND ',1,'2'),('ui','UI ',1,'2'),('return','RET ',1,'2'),('calls','CALLS ',1,'17'),('queue-count','IQ ',1,'33'),('dynamic-count','IDC ',1,'17')]:
  ix=next(i for i,row in enumerate(rows) if row.startswith(prefix));row=rows[ix].split();row[col]=value;changed=rows.copy();changed[ix]=' '.join(row);bad[name]=changed
 for name,changed in bad.items():
  f=d/'bad.txt';f.write_text('\n'.join(changed)+'\n');result=subprocess.run([runner,f],env=env,capture_output=True)
  assert result.returncode==2,(name,result.returncode);print(name,'rejected',flush=True)
 print(f'{len(faults)+3} runtime faults caught; {len(bad)} malformed captures rejected')

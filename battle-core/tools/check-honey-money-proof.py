#!/usr/bin/env python3
"""Real compiled counterfactuals and strict capture corruption rejection."""
from pathlib import Path
import os,sys,subprocess,tempfile
r=Path(__file__).resolve().parents[2];b=r/'battle-core';g=r/'game-core';raw,owner,money,number=map(lambda p:Path(p).resolve(),sys.argv[1:5]);env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
traces={'raw':(b/'tests/honey-money-test.c',b/'build/honey-money-test',raw),'owner':(b/'tests/honey-owner-test.c',b/'build/honey-owner-test',owner),'money':(g/'tests/wide_money_test.c',g/'build/wide_money_test',money),'number':(b/'tests/money-number-test.c',b/'build/money-number-test',number),'host':(g/'tests/honey_money_host_test.c',g/'build/honey_money_host_test',None)}
for _,runner,trace in traces.values():subprocess.run([runner,*([trace] if trace else [])],env=env,check=True)
faults=[]
def variants(filename,start,end,changes,kind):
 s=(r/filename).read_text();a=s.index(start);z=s.index(end,a+len(start));part=s[a:z]
 for name,old,new in changes:
  assert part.count(old)==1,(name,part.count(old));faults.append((name,filename,s[:a]+part.replace(old,new,1)+s[z:],kind))
variants('battle-core/src/ab_attrs.c','    case PR_AC_MoneyAbAttr:','    case PR_AC_SyncEncounterNatureAbAttr:',[
 ('lost-money-effect','+= pr_wave_money_amount(bt->wave_index, 0.2)','+= 0'),('simulation-mutates','!pp->simulated && pp->victory','pp->victory'),('loss-mutates','!pp->simulated && pp->victory','!pp->simulated'),('lost-history','return 1;','return 0;'),('narrow-honey','+= pr_wave_money_amount(bt->wave_index, 0.2)','+= (int32_t)pr_wave_money_amount(bt->wave_index, 0.2)')],'raw')
variants('battle-core/src/move_effects.c','    case PR_AC_MoneyAttr:','    case PR_AC_RemoveHeldItemAttr:',[
 ('narrow-payday','+= pr_wave_money_amount(bt->wave_index, 0.2)','+= (int32_t)pr_wave_money_amount(bt->wave_index, 0.2)'),('wrong-payday-parent-gate','        bt->money_scattered','        if(!effect_can_apply(ef,user,target))return;\n        bt->money_scattered')],'raw')
variants('battle-core/src/ab_attrs.c','static void before_apply(','/* canApply(params)',[('lost-banner','c->shown = shows_ability(a);','c->shown = false;')],'raw')
variants('battle-core/src/ab_attrs.c','void pr_apply_ab_attrs(','\n}',[('duplicate-passive','(!p->passive || p->passive == p->ability)','(!p->passive)')],'raw')
variants('battle-core/src/engine.c','void pr_prepare_post_battle_money(','void pr_run_post_battle_money(',[
 ('missing-owner','        pr_apply_ab_attrs(bt, PR_AC_MoneyAbAttr, &pp);','        (void)pp;'),('only-lead-owner','k < bt->n_party[0]','k < 1'),('forbidden-physical-filter','if (!p || p->hp <= 0)','if (!p || p->hp <= 0 || !p->on_field || p->switch_out_status)')],'owner')
variants('game-core/src/gc_phases.c','static void battle_end(','/* ---- TrainerVictoryPhase', [('missing-actual-owner','    post_battle_money(run, victory);','    /* omitted */')],'host')
variants('game-core/src/gc_phases.c','void gc_pick_up_scattered_money(','/* BattleEndPhase\'s Honey',[
 ('narrow-pickup','double amount = bt->money_scattered;','double amount = (int32_t)bt->money_scattered;'),('missing-coin','amount += floor(amount * 0.2 * run->items[i].stack)','amount += 0'),('missing-happy','amount *= 2','amount *= 1'),('missing-reset','bt->money_scattered = 0;','/* no reset */'),('missing-prompt','message->prompt = true','message->prompt = false'),('narrow-message','pr_arg_money_en_us("moneyAmount", amount)','pr_arg_int("moneyAmount", (int32_t)amount)')],'money')
variants('game-core/src/gc_run.c','void gc_ev_money(','gc_event *gc_ev_msg(', [('lost-wide-event','ev->money_change = change;','ev->money_change = (int32_t)change;')],'money')
variants('game-core/src/gc_phases.c','static void money_reward(','/* ---- SelectModifierPhase', [('lost-reward-happy','pr_arena_has_tag(&run->bt, ARENA_TAG_TYPE_HAPPY_HOUR, ARENA_TAG_SIDE_BOTH)','false')],'money')
variants('battle-core/src/js_number.c','size_t pr_money_en_us(','\n}',[('missing-number-grouping',"text[n++] = ',';","text[n++] = '.';"),('missing-negative-sign',"text[n++] = '-';","text[n++] = '+';")],'number')
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(r/p) for p in ['battle-core/include','battle-core/gen/out','battle-core/build','battle-core/src','run-core/include','game-core/include','game-core/src','ai-core/include','reward-core/include','reward-core/gen/out']]]
with tempfile.TemporaryDirectory(prefix='honey-money-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,filename,changed,kind in faults:
  src=d/Path(filename).name;src.write_text(changed);obj=d/'fault.o';exe=d/'fault';subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  source,_,trace=traces[kind];libs=[b/'build/libprcore.a']
  if kind in ['money','host']:libs=[g/'build/libgc.a',g/'build/reward-core/libreward.a',g/'build/ai-core/libai.a',g/'build/run-core/librun.a',b/'build/libprcore.a']
  subprocess.run(['gcc',*flags,source,obj,*libs,'-lm','-o',exe],check=True,capture_output=True)
  p=subprocess.run([exe,*([trace] if trace else [])],env=env,capture_output=True);assert p.returncode==1,(name,p.returncode,p.stdout[-300:],p.stderr[-300:]);print(name,'caught',flush=True)
 malformed=0
 def reject(kind,name,data):
  global malformed
  file=d/'bad.txt';file.write_bytes(data);p=subprocess.run([traces[kind][1],file],env=env,capture_output=True);assert p.returncode==2,(kind,name,p.returncode);malformed+=1;print(kind,name,'rejected',flush=True)
 for kind in ['raw','owner','money','number']:
  data=traces[kind][2].read_bytes();first,rest=data.split(b'\n',1);cols=first.split();cols[1]=b'99';reject(kind,'version',b' '.join(cols)+b'\n'+rest)
  cols=first.split();cols[2]=str(int(cols[2])-1).encode();reject(kind,'count',b' '.join(cols)+b'\n'+rest)
  reject(kind,'trailing',data+b'JUNK\n');reject(kind,'footer',data[:data.rfind(b'\n',0,-1)+1])
 def rowmutate(data,key,col,value):
  a=data.index(b'\n'+key)+1;z=data.index(b'\n',a);row=data[a:z].split();row[col]=value;return data[:a]+b' '.join(row)+data[z:]
 for kind,key,col,value in [('raw',b'C ',2,b'8'),('raw',b'C ',3,b'2'),('raw',b'C ',4,b'2'),('raw',b'C ',5,b'2'),('raw',b'C ',6,b'5851'),('owner',b'C ',5,b'2'),('money',b'C ',5,b'3'),('money',b'O ',4,b'2'),('number',b'C ',1,b'0.5')]:reject(kind,'invalid-field',rowmutate(traces[kind][2].read_bytes(),key,col,value))
 print(f'{len(faults)} compiled faults caught; {malformed} malformed captures rejected',flush=True)

#!/usr/bin/env python3
"""Compiled native-contract faults and malformed Poison Puppeteer transport guards."""
from pathlib import Path
import os,sys,subprocess,tempfile
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=r/'battle-core/build/poison-puppeteer-test';subprocess.run([runner,trace],check=True,env=env)
source=(r/'battle-core/src/ab_attrs.c').read_text();a=source.index('    case PR_AC_ConfusionOnStatusEffectAbAttr: {');b=source.index('\n    case PR_AC_',a+1);body=source[a:b];faults=[]
def fault(name,old,new):
 assert body.count(old)==1,(name,body.count(old));faults.append((name,source[:a]+body.replace(old,new,1)+source[b:]))
fault('all-status-effects','int_set_has(P(ConfusionOnStatusEffectAbAttr, a)->effects, pp->effect)','true')
fault('poison-only','int_set_has(P(ConfusionOnStatusEffectAbAttr, a)->effects, pp->effect)','pp->effect == STATUS_EFFECT_POISON')
fault('toxic-only','int_set_has(P(ConfusionOnStatusEffectAbAttr, a)->effects, pp->effect)','pp->effect == STATUS_EFFECT_TOXIC')
fault('fainted-target','effect && o->hp > 0 && pr_can_add_tag','effect && pr_can_add_tag')
fault('ignored-tag-immunity','effect && o->hp > 0 && pr_can_add_tag(bt, (pr_pokemon *)o, BATTLER_TAG_TYPE_CONFUSED)','effect && o->hp > 0')
fault('extra-user-field-gate','GATE(effect &&','GATE(p->on_field && !p->switch_out_status && effect &&')
fault('extra-misty-canApply-gate','GATE(effect &&','GATE(bt->terrain != TERRAIN_MISTY && effect &&')
fault('simulation-mutates','if (!pp->simulated) {','if (true) {')
fault('wrong-duration-range','pr_battle_rand_seed_int(bt, 4, 2)','pr_battle_rand_seed_int(bt, 3, 2)')
fault('wrong-duration-minimum','pr_battle_rand_seed_int(bt, 4, 2)','pr_battle_rand_seed_int(bt, 4, 1)')
fault('fixed-duration-no-RNG','pr_battle_rand_seed_int(bt, 4, 2)','3')
fault('no-confusion-tag','pr_add_tag(bt, c->e, (pr_pokemon *)o, BATTLER_TAG_TYPE_CONFUSED, turns, 0, (int64_t)o->id);','(void)turns;')
fault('wrong-source-move','turns, 0, (int64_t)o->id','turns, MOVE_ID_TOXIC, (int64_t)o->id')
fault('wrong-recipient','(pr_pokemon *)o, BATTLER_TAG_TYPE_CONFUSED, turns','(pr_pokemon *)p, BATTLER_TAG_TYPE_CONFUSED, turns')
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(r/'battle-core'/p) for p in ['include','gen/out','build']]]
with tempfile.TemporaryDirectory(prefix='puppeteer-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,changed in faults:
  src=d/'ab_attrs.c';src.write_text(changed);obj=d/'fault.o';exe=d/'fault'
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,r/'battle-core/tests/poison-puppeteer-test.c',obj,r/'battle-core/build/libprcore.a','-lm','-o',exe],check=True,capture_output=True)
  p=subprocess.run([exe,trace],env=env,capture_output=True);assert p.returncode==1,(name,p.returncode,p.stdout[-500:],p.stderr[-500:]);print(name,'caught',flush=True)
 data=trace.read_bytes();first,rest=data.split(b'\n',1);bad=d/'bad.txt';count=0
 def reject(name,changed):
  global count
  bad.write_bytes(changed);p=subprocess.run([runner,bad],env=env,capture_output=True);assert p.returncode==2,(name,p.returncode);count+=1;print(name,'rejected',flush=True)
 reject('version',first.replace(b'PUPPETEER 1 ',b'PUPPETEER 2 ')+b'\n'+rest)
 reject('small',b'PUPPETEER 1 65535 777\n'+rest)
 reject('footer',data.rsplit(b'PUPPETEER_COMPLETE ',1)[0]+b'PUPPETEER_COMPLETE 1\n')
 reject('trailing',data+b'JUNK\n')
 for name,prefix,col,value in [('fractional-case',b'C ',1,b'0.5'),('user',b'C ',2,b'8'),('target',b'C ',3,b'8'),('simulation',b'C ',4,b'2'),('effect',b'C ',5,b'8'),('terrain',b'C ',6,b'2'),('context',b'C ',7,b'2'),('tag-present',b'IT ',2,b'2'),('tag-turn',b'IT ',3,b'1.5'),('tag-source',b'IT ',5,b'0'),('dynamic-animation',b'D ',8,b'65536')]:
  a=data.index(b'\n'+prefix)+1;b=data.index(b'\n',a);row=data[a:b].split();row[col]=value;reject(name,data[:a]+b' '.join(row)+data[b:])
 print(f'{len(faults)} compiled production faults caught; {count} malformed captures rejected')

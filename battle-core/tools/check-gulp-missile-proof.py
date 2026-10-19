#!/usr/bin/env python3
"""Compile actual Gulp callback/tag/form/phase faults and reject malformed transport."""
from pathlib import Path
import os,sys,subprocess,tempfile
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES');runner=r/'battle-core/build/gulp-missile-test'
subprocess.run([runner,trace],check=True,env=env)
faults=[]
def fault(name,file,old,new):
 s=(r/'battle-core/src'/file).read_text();assert s.count(old)==1,(name,s.count(old));faults.append((name,file,s.replace(old,new,1)))
fault('missing-hp-gate','battler_tags.c','if(user->hp<=0 || user->species','if(user->species')
fault('wrong-half-boundary','battler_tags.c','pr_get_hp_ratio(user,false)>=0.5','pr_get_hp_ratio(user,false)>0.5')
fault('wrong-callback-return','battler_tags.c','    pr_add_tag(bt,e,user,pr_get_hp_ratio','    return pr_add_tag(bt,e,user,pr_get_hp_ratio')
fault('wrong-source-move','battler_tags.c','(t->source_move==MOVE_ID_SURF||t->source_move==MOVE_ID_DIVE)','(t->source_move==MOVE_ID_SURF)')
fault('missing-form-gate','battler_tags.c','p->species==SPECIES_ID_CRAMORANT && p->form==0','p->species==SPECIES_ID_CRAMORANT')
fault('missing-underwater','battler_tags.c','if(pr_get_battler_tag(p,BATTLER_TAG_TYPE_UNDERWATER))return true;','/* omitted */')
fault('missing-substitute','battler_tags.c','if(pr_move_hits_substitute(bt,move,attacker,p))return true;','/* omitted */')
fault('magic-guard-secondary','battler_tags.c','        if(t->type==BATTLER_TAG_TYPE_GULP_MISSILE_ARROKUDA){','        if(cancelled)return false;\n        if(t->type==BATTLER_TAG_TYPE_GULP_MISSILE_ARROKUDA){')
fault('wrong-quarter-damage','battler_tags.c','pr_get_stat(attacker,STAT_HP,true)/4.0','pr_get_stat(attacker,STAT_HP,true)/3.0')
fault('wrong-stat-source','battler_tags.c','&stat,1,-1,p);','&stat,1,-1,attacker);')
fault('wrong-status-source','battler_tags.c','STATUS_EFFECT_PARALYSIS,p,-1,false,true','STATUS_EFFECT_PARALYSIS,attacker,-1,false,true')
fault('wrong-form-target','phases.c','pr_get_hp_ratio(mon,false)>=0.5?"gulping":"gorging"','pr_get_hp_ratio(mon,false)>=0.5?"gorging":"gulping"')
fault('missing-party-lookup','phases.c','pr_pokemon *p = pr_pokemon_by_id(bt, fx->user_id);','pr_pokemon *p = pr_field_pokemon_by_id(bt, fx->user_id);')
fault('id-before-slot','phases.c','if (fx->user_id <= 3) return slot(bt, (int)fx->user_id);','/* omitted native low-index boundary */')
fault('wrong-cached-status-message','phases.c','pr_status_msg(bt, effect, PR_ST_OBTAIN, slot(bt,q->battler))','pr_status_msg(bt, effect, PR_ST_OBTAIN, p)')
fault('cached-status-recipient','phases.c','    pr_pokemon *p = slot(bt, q->battler);\n    if (!p) { pr_note_unported(bt, PR_FEAT_PHASE_0 + PH_OBTAIN_STATUS_EFFECT);','    pr_pokemon *p = q->pokemon;\n    if (!p) { pr_note_unported(bt, PR_FEAT_PHASE_0 + PH_OBTAIN_STATUS_EFFECT);')
fault('missing-source-exception-refusal','phases.c','if (!slot(bt, PH(ph)->battler)) {','if (false) {')
flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(r/x) for x in ['battle-core/include','battle-core/gen/out','battle-core/build','battle-core/tests']]]
with tempfile.TemporaryDirectory(prefix='gulp-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,file,changed in faults:
  src=d/file;src.write_text(changed);obj=d/'fault.o';exe=d/'fault'
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,r/'battle-core/tests/gulp-missile-test.c',obj,r/'battle-core/build/libprcore.a','-Wl,--wrap=pr_gulp_missile_tag','-lm','-o',exe],check=True,capture_output=True)
  result=subprocess.run([exe,trace],env=env,capture_output=True)
  assert result.returncode==1,(name,result.returncode,result.stdout[-500:],result.stderr[-500:]);print(name,'caught',flush=True)
 data=trace.read_bytes();first,rest=data.split(b'\n',1)
 bad=[('version',b'GULP 4 16768 777\n'+rest),('too-few',b'GULP 3 16384 777\n'+rest),('footer',data.rsplit(b'GULP_COMPLETE ',1)[0]+b'GULP_COMPLETE 1\n'),('trailing',data+b'JUNK\n')]
 for name,prefix,col,value in [('case',b'C ',1,b'0.5'),('mode',b'C ',2,b'6'),('user',b'C ',3,b'4'),('attacker',b'C ',4,b'8'),('fx',b'C ',8,b'4'),('tags',b'IT ',2,b'65'),('nan',b'I ',7,b'NaN'),('queue-level',b'IQ ',2,b'0')]:
  start=data.index(b'\n'+prefix)+1;end=data.index(b'\n',start);row=data[start:end].split();row[col]=value;bad.append((name,data[:start]+b' '.join(row)+data[end:]))
 for name,changed in bad:
  f=d/'bad.txt';f.write_bytes(changed);result=subprocess.run([runner,f],env=env,capture_output=True);assert result.returncode==2,(name,result.returncode);print(name,'rejected',flush=True)
 print(f'{len(faults)} runtime faults caught; {len(bad)} malformed captures rejected')

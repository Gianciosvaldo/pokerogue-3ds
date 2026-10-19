#!/usr/bin/env python3
"""Real phase/owner mutants must fail; malformed raw captures must be refused."""
from pathlib import Path
import os,sys,subprocess,tempfile
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();turns=Path(sys.argv[2]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=r/'battle-core/build/tera-phase-test';own=r/'battle-core/build/wide-combat-test'
subprocess.run([runner,trace],check=True,env=env);subprocess.run([own,turns],check=True,env=env)
faults=[]
def fault(name,file,old,new,owning=False):
 s=(r/'battle-core/src'/file).read_text();assert s.count(old)==1,(name,s.count(old));faults.append((name,file,s.replace(old,new,1),owning))
fault('missing-tera-flag','phases.c','    p->terastallized=true;','    p->terastallized=false;')
fault('retained-added-type','phases.c','    p->added_type=0; /* null added type','    p->added_type=p->added_type; /* null added type')
fault('missing-form-trigger','phases.c','    form_change(bt,p,PR_FCT_SpeciesFormChangeTeraTrigger);','    /* missing form trigger */')
fault('wrong-owner-counter','phases.c','    if(p->is_player){\n        if(!bt->player_teras_used_known)','    if(!p->is_player){\n        if(!bt->player_teras_used_known)')
fault('capped-counter','phases.c','else bt->player_teras_used+=1;','else if(bt->player_teras_used<1)bt->player_teras_used+=1;')
fault('missing-achievement','phases.c','bt->achievement_check(bt,PR_ACHV_CHECK_TERASTALLIZE,bt->achievement_check_ctx);','/* missing achievement */')
fault('missing-stellar-check','phases.c','bt->achievement_check(bt,PR_ACHV_CHECK_STELLAR_TERASTALLIZE,bt->achievement_check_ctx);','(void)0;')
fault('missing-animation-identity','phases.c','e->events[before].actor_ids_known=true;','e->events[before].actor_ids_known=false;')
fault('wrong-tera-form','phases.c','!strcmp(key,"terastal"))to="stellar";','!strcmp(key,"terastal"))to="terastal";')
fault('missing-ogerpon-lapse','phases.c','bool tera=trigger_class==PR_FCT_SpeciesFormChangeTeraTrigger;','bool tera=true;')
fault('missing-owner-reference','phases.c','            if (t >= 0) PH(t)->pokemon = p; /* native TeraPhase constructor takes Pokemon */','            /* missing constructor owner */',True)
flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(r/x) for x in ['battle-core/include','battle-core/gen/out','battle-core/build','battle-core/tests']]]
with tempfile.TemporaryDirectory(prefix='tera-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,file,changed,owning in faults:
  src=d/file;src.write_text(changed);obj=d/'fault.o';exe=d/'fault'
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,r/('battle-core/build/wide-combat-replay.c' if owning else 'battle-core/tests/tera-phase-test.c'),obj,r/'battle-core/build/libprcore.a','-lm','-o',exe],check=True,capture_output=True)
  p=subprocess.run([exe,turns if owning else trace],env=env,capture_output=True)
  assert p.returncode==1,(name,p.returncode,p.stdout[-400:],p.stderr[-400:]);print(name,'caught',flush=True)
 data=trace.read_bytes();first,rest=data.split(b'\n',1)
 bad=[('version',first.replace(b'TERA 1 ',b'TERA 2 ')+b'\n'+rest),('too-few',b'TERA 1 1 777\n'+rest),('footer',data.rsplit(b'TERA_COMPLETE ',1)[0]+b'TERA_COMPLETE 1\n'),('trailing',data+b'JUNK\n')]
 for name,prefix,col,value in [('case',b'C ',1,b'0.5'),('mode',b'C ',2,b'4'),('user',b'C ',3,b'8'),('negative-count',b'A ',1,b'-1'),('fractional-count',b'A ',1,b'0.5'),('nan-count',b'A ',1,b'NaN'),('oversized-count',b'A ',1,b'9007199254740992'),('queue-level',b'IQ ',1,b'0')]:
  a=data.index(b'\n'+prefix)+1;b=data.index(b'\n',a);row=data[a:b].split();row[col]=value;bad.append((name,data[:a]+b' '.join(row)+data[b:]))
 for name,changed in bad:
  f=d/'bad.txt';f.write_bytes(changed);p=subprocess.run([runner,f],env=env,capture_output=True);assert p.returncode==2,(name,p.returncode);print(name,'rejected',flush=True)
 print(f'{len(faults)} runtime faults caught; {len(bad)} malformed captures rejected')

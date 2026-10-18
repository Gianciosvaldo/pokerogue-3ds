"""Compiled controller/form faults and strict complete-capture decoder guards."""
from pathlib import Path
import subprocess,os,sys
root=Path(sys.argv[1]).resolve();out=Path(sys.argv[2]).resolve();env=os.environ.copy();env['TURNTEST_LOCALES']=sys.argv[3]
faults=[
 ('no-double','phases.c','    bt->double_battle = true;','    bt->double_battle = false;','final-boss-test','owners.txt'),
 ('wrong-moves','phases.c','{MOVE_ID_DYNAMAX_CANNON, MOVE_ID_CROSS_POISON,','{MOVE_ID_ETERNABEAM, MOVE_ID_CROSS_POISON,','final-boss-test','owners.txt'),
 ('unsigned-pp','phases.c','p->pp_up[3] = -4;','p->pp_up[3] = 0;','final-boss-test','owners.txt'),
 ('transferable-item','phases.c','.pokemon_id=p->id,.stack_count=1,.is_transferable=false','.pokemon_id=p->id,.stack_count=1,.is_transferable=true','final-boss-test','owners.txt'),
 ('wrong-summon-slot','phases.c','PH(summon)->sw_field_index=1;','PH(summon)->sw_field_index=0;','final-boss-test','owners.txt'),
 ('wrong-wave','phases.c','&& bt->wave_index == 200;','&& bt->wave_index >= 200;','final-boss-test','owners.txt'),
 ('missing-status-end','phases.c','if (is_classic_final_boss(bt)) pr_init_final_boss_phase_two(bt,e,p);','if (0) pr_init_final_boss_phase_two(bt,e,p);','final-boss-test','owners.txt'),
 ('inactive-passive','phases.c','uint16_t passive_ability = pr_get_passive_ability(p);','uint16_t passive_ability = p->passive;','final-boss-form-test','form-final-777.txt'),
 ('missing-party-slot','engine.c','if (!p && b >= 0 && b < 4 && b % 2 < bt->n_party[b / 2])','if (0)','final-boss-test','owners.txt')]
for name,unit,a,b,test,trace in faults:
 source=(root/'src'/unit).read_text();assert a in source,name;f=out/f'mutant-{name}.c';f.write_text(source.replace(a,b,1));exe=out/f'mutant-{name}'
 subprocess.run(['gcc','-O2','-std=c99','-I'+str(root/'include'),'-I'+str(root/'gen/out'),str(root/'tests'/f'{test}.c'),str(f),str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True,capture_output=True)
 r=subprocess.run([str(exe),str(out/trace)],env=env,capture_output=True,text=True);(out/f'mutant-{name}.log').write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode,r.stdout,r.stderr);print(name,'caught',flush=True)
for trace,test,header in [('owners.txt','final-boss-test','FINALBOSS'),('form-final-777.txt','final-boss-form-test','FINALBOSSFORM')]:
 s=(out/trace).read_text();rows=s.splitlines(True);n=int(rows[0].split()[2]);c=next(i for i,l in enumerate(rows)if l.startswith('C '))
 malformed={'truncated':''.join(rows[:-1]),'wrong-version':s.replace(header+' 1 ',header+' 2 ',1),'wrong-footer':s.replace(f'COMPLETE {n}',f'COMPLETE {n-1}'),'extra':s+'extra\n','wrong-case':s.replace(rows[c],rows[c].replace('C 0 ','C 1 ',1),1),'fraction':s.replace(rows[c],rows[c].replace('C 0 ','C 0.5 ',1),1)}
 for name,text in malformed.items():
  f=out/f'malformed-{test}-{name}.txt';f.write_text(text);r=subprocess.run([str(root/'build'/test),str(f)],env=env,capture_output=True,text=True);assert r.returncode==2,(test,name,r.returncode);print(test,'malformed',name,'rejected',flush=True)
print('ALL final-boss guards passed')

#!/usr/bin/env python3
"""Compile real runtime faults and reject malformed complete source traces."""
from pathlib import Path
import argparse,os,subprocess
parser=argparse.ArgumentParser()
parser.add_argument('trace',type=Path)
parser.add_argument('--tera',type=Path)
args=parser.parse_args()
root=Path(__file__).resolve().parents[2]
out=root/'battle-core/build/reset-arena-guards';out.mkdir(parents=True,exist_ok=True)
env=os.environ.copy()
if not env.get('TURNTEST_LOCALES'):raise SystemExit('TURNTEST_LOCALES is required')
trace=args.trace.resolve();exe=root/'battle-core/build/reset-arena-test'
mutations={
 'missing-init':('engine.c','pr_apply_ab_attrs(bt,PR_AC_PostBattleInitAbAttr,&pp);','(void)pp;'),
 'early-summon-reset':('engine.c','        pr_reset_battle_data(p);','        pr_reset_summon_data(p);pr_reset_battle_data(p);'),
 'init-every-wave':('engine.c','    if (!reset_arena) return;','    (void)reset_arena;'),
 'wrong-return-priority':('engine.c','            pr_phase_push(bt,e,ph);','            pr_phase_unshift(bt,e,ph);'),
 'ignore-old-double':('engine.c','int field_count = bt->double_battle ? 2 : 1;','int field_count = 1;'),
 'retain-wave-abilities':('modifiers.c','    p->n_ab_applied_wave=0;','    p->n_ab_applied_wave=p->n_ab_applied_wave;'),
 'retain-wave-turn-count':('modifiers.c','    p->temp_wave_turn_count=1;','    p->temp_wave_turn_count=p->temp_wave_turn_count;'),
 'retain-wave-endurance':('modifiers.c','    p->wave_endured=false;','    p->wave_endured=p->wave_endured;'),
 'ordinary-switch-out-effects':('phases.c','    /* ReturnPhase.onEnd */','    pr_hook_ab(bt,p,PR_AC_PreSwitchOutAbAttr);\n    /* ReturnPhase.onEnd */'),
 'missing-return':('phases.c','case PH_RETURN: return_phase(bt,e,ph); break;','case PH_RETURN: break;'),
}
for name,(file,old,new) in mutations.items():
 s=(root/'battle-core/src'/file).read_text();assert s.count(old)==1,(name,s.count(old));mut=out/(name+'.c');obj=out/(name+'.o');binary=out/name;mut.write_text(s.replace(old,new));subprocess.run(['gcc','-O2','-std=c99','-ffp-contract=off','-I'+str(root/'battle-core/include'),'-I'+str(root/'battle-core/gen/out'),'-c',str(mut),'-o',str(obj)],check=True);subprocess.run(['gcc','-O2','-std=c99','-I'+str(root/'battle-core/include'),'-I'+str(root/'battle-core/gen/out'),str(root/'battle-core/tests/reset-arena-test.c'),str(obj),str(root/'battle-core/build/libprcore.a'),'-lm','-o',str(binary)],check=True);r=subprocess.run([str(binary),str(trace)],env=env,capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode);print(name,'caught',flush=True)
rows=trace.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'wrong-version':['RESETARENA 3 4000 777']+rows[1:],'wrong-footer':rows[:-1]+['RESETARENA_COMPLETE 1']}
ci=next(i for i,x in enumerate(rows) if x.startswith('C '));ii=next(i for i,x in enumerate(rows) if x.startswith('I '));qi=next(i for i,x in enumerate(rows) if x.startswith('Q ') and 'ReturnPhase' in x);ti=next(i for i,x in enumerate(rows) if x.startswith('T '));xi=next(i for i,x in enumerate(rows) if x.startswith('X '));fi=next(i for i,x in enumerate(rows) if x.startswith('Q ') and 'QuietFormChangePhase' in x)
for name,index,col,value in [('unknown-kind',ci,2,'4'),('fractional-case',ci,1,'0.5'),('species-wrap',ii,2,'4294967297'),('bad-initial-level',ii,6,'256'),('overflow-queue',qi,2,'4294967296'),('unknown-queue',qi,4,'UnknownPhase'),('unknown-executed-phase',xi,1,'UnknownPhase'),('bad-text',ti,1,'xyz'),('odd-text',ti,1,'abc'),('bad-form-target',fi,8,'F:256')]:
 r=rows.copy();v=r[index].split();v[col]=value;r[index]=' '.join(v);bad[name]=r
for name,r in bad.items():
 p=out/(name+'.txt');p.write_text('\n'.join(r)+'\n');x=subprocess.run([str(exe),str(p)],env=env,capture_output=True,text=True);assert x.returncode==2,(name,x.returncode,x.stdout,x.stderr);print(name,'rejected',flush=True)
if args.tera:
 s=(root/'battle-core/src/engine.c').read_text();old='p->terastallized=false;p->stellar_types_boosted=0;';assert s.count(old)==1
 mut=out/'retain-tera.c';obj=out/'retain-tera.o';binary=out/'retain-tera';mut.write_text(s.replace(old,'p->terastallized=p->terastallized;p->stellar_types_boosted=0;'))
 subprocess.run(['gcc','-O2','-std=c99','-ffp-contract=off','-I'+str(root/'battle-core/include'),'-I'+str(root/'battle-core/gen/out'),'-c',str(mut),'-o',str(obj)],check=True)
 subprocess.run(['gcc','-O2','-std=c99','-I'+str(root/'battle-core/include'),'-I'+str(root/'battle-core/gen/out'),str(root/'battle-core/tests/reset-arena-test.c'),str(obj),str(root/'battle-core/build/libprcore.a'),'-lm','-o',str(binary)],check=True)
 r=subprocess.run([str(binary),str(args.tera.resolve())],env=env,capture_output=True,text=True);(out/'retain-tera.log').write_text(r.stdout+r.stderr);assert r.returncode==1,('retain-tera',r.returncode);print('retain-tera caught',flush=True)
print(f'{10+bool(args.tera)} compiled mutants caught; 14 malformed traces rejected',flush=True)

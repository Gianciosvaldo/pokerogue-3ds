#!/usr/bin/env python3
"""Compiled native callback regressions and strict raw transport guards."""
from pathlib import Path
import os,sys,subprocess,tempfile
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=r/'battle-core/build/entry-allies-test';subprocess.run([runner,trace],check=True,env=env)
source=(r/'battle-core/src/ab_attrs.c').read_text();faults=[]
def fault(name,old,new):
 assert source.count(old)==1,(name,source.count(old));faults.append((name,source.replace(old,new,1)))
def in_case(name,cls,old,new):
 a=source.index('    case PR_AC_'+cls+': {');b=source.index('\n    case PR_AC_',a+1);part=source[a:b];assert part.count(old)==1,(name,part.count(old));faults.append((name,source[:a]+part.replace(old,new,1)+source[b:]))
heal='PostSummonAllyHealAbAttr';clear='PostSummonClearAllyStatStagesAbAttr'
gate='GATE(ally && ally->hp > 0 && ally->on_field && !ally->switch_out_status);'
in_case('Hospitality-fainted-ally',heal,gate,'GATE(ally && ally->on_field && !ally->switch_out_status);')
in_case('Curious-Medicine-fainted-ally',clear,gate,'GATE(ally && ally->on_field && !ally->switch_out_status);')
in_case('Hospitality-inactive-ally',heal,gate,'GATE(ally && ally->hp > 0);')
in_case('Curious-Medicine-inactive-ally',clear,gate,'GATE(ally && ally->hp > 0);')
in_case('Hospitality-extra-user-field-gate',heal,gate,'GATE(p->on_field && !p->switch_out_status && ally && ally->hp > 0 && ally->on_field && !ally->switch_out_status);')
in_case('Curious-Medicine-extra-user-field-gate',clear,gate,'GATE(p->on_field && !p->switch_out_status && ally && ally->hp > 0 && ally->on_field && !ally->switch_out_status);')
in_case('Hospitality-missing-simulation-gate',heal,'if (!pp->simulated) {','if (true) {')
in_case('Curious-Medicine-missing-simulation-gate',clear,'if (!pp->simulated) {','if (true) {')
fault('Hospitality-uses-target-max-HP','to_dmg_value(pr_get_stat(p, STAT_HP, true) / ratio)','to_dmg_value(pr_get_stat(ally, STAT_HP, true) / ratio)')
fault('Hospitality-wrong-heal-ratio','double ratio = x->healRatio ? x->healRatio : 4;','double ratio = 8;')
fault('Hospitality-skipped-animation','PR_HEAL_MESSAGE | (x->showAnim ? 0 : PR_HEAL_SKIP_ANIM)','PR_HEAL_MESSAGE | PR_HEAL_SKIP_ANIM')
in_case('Hospitality-extra-full-HP-gate',heal,gate,'GATE(ally && ally->hp > 0 && ally->on_field && !ally->switch_out_status && ally->hp < pr_get_stat(ally,STAT_HP,true));')
fault('Curious-Medicine-clears-user','memset(((pr_pokemon *)ally)->stat_stages, 0, sizeof ally->stat_stages);','memset(((pr_pokemon *)p)->stat_stages, 0, sizeof p->stat_stages);')
fault('Curious-Medicine-retains-negative-stages','memset(((pr_pokemon *)ally)->stat_stages, 0, sizeof ally->stat_stages);','for(int j=0;j<7;j++)if(ally->stat_stages[j]>0)((pr_pokemon *)ally)->stat_stages[j]=0;')
fault('Hospitality-wrong-user-affix','pr_arg_pokemon_name(bt, "pokemonName", &identity)','pr_arg_pokemon(bt, "pokemonName", p)')
flags=['-O2','-std=c99','-ffp-contract=off',*['-I'+str(r/'battle-core'/p) for p in ['include','gen/out','build']]]
with tempfile.TemporaryDirectory(prefix='entry-allies-guards-',dir=r/'work') as tmp:
 d=Path(tmp)
 for name,changed in faults:
  src=d/'ab_attrs.c';src.write_text(changed);obj=d/'fault.o';exe=d/'fault'
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,r/'battle-core/tests/entry-allies-test.c',obj,r/'battle-core/build/libprcore.a','-lm','-o',exe],check=True,capture_output=True)
  p=subprocess.run([exe,trace],env=env,capture_output=True);assert p.returncode==1,(name,p.returncode,p.stdout[-500:],p.stderr[-500:]);print(name,'caught',flush=True)
 data=trace.read_bytes();first,rest=data.split(b'\n',1);bad=d/'bad.txt';count=0
 def reject(name,changed):
  global count
  bad.write_bytes(changed);p=subprocess.run([runner,bad],env=env,capture_output=True);assert p.returncode==2,(name,p.returncode);count+=1;print(name,'rejected',flush=True)
 reject('version',first.replace(b'ENTRY_ALLIES 2 ',b'ENTRY_ALLIES 3 ')+b'\n'+rest)
 reject('small',b'ENTRY_ALLIES 2 49151 777\n'+rest)
 reject('footer',data.rsplit(b'ENTRY_ALLIES_COMPLETE ',1)[0]+b'ENTRY_ALLIES_COMPLETE 1\n')
 reject('trailing',data+b'JUNK\n')
 for name,prefix,col,value in [('fractional-case',b'C ',1,b'0.5'),('actor',b'C ',2,b'8'),('simulation',b'C ',3,b'2'),('gain',b'C ',4,b'2'),('single',b'C ',5,b'2'),('queue-HP-NaN',b'P ',8,b'NaN'),('queue-flags',b'P ',9,b'128')]:
  a=data.index(b'\n'+prefix)+1;b=data.index(b'\n',a);row=data[a:b].split();row[col]=value;reject(name,data[:a]+b' '.join(row)+data[b:])
 print(f'{len(faults)} compiled production faults caught; {count} malformed captures rejected')

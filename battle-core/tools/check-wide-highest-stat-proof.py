#!/usr/bin/env python3
"""Plant real highest-stat selection faults and reject malformed source transport."""
from pathlib import Path
import os,subprocess,sys,tempfile
r=Path(__file__).resolve().parents[2];trace=Path(sys.argv[1]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=r/'battle-core/build/wide-highest-stat-test';subprocess.run([runner,trace],check=True,env=env)
s=(r/'battle-core/src/battler_tags.c').read_text();start=s.index('        int best = STAT_ATK;');end=s.index('    case BATTLER_TAG_TYPE_SUPREME_OVERLORD:',start);leaf=s[start:end]
faults=[
 ('narrow-best',s.replace('double best_value = 0;','int32_t best_value = 0;',1)),
 ('narrow-value',s.replace('double v = pr_get_effective_stat_p(bt, p, stat, &sp);','int32_t v = pr_get_effective_stat_p(bt, p, stat, &sp);',1)),
 ('last-tie',s[:start]+leaf.replace('v > best_value','v >= best_value')+s[end:]),
 ('wrong-speed-multiplier',s[:start]+leaf.replace('best == STAT_SPD ? 1.5 : 1.3','best == STAT_SPD ? 1.3 : 1.3')+s[end:]),
 ('missing-message',s[:start]+leaf[:leaf.index('        pr_queue_msg')]+ '        return;\n    }\n'+s[end:]),
 ('missing-initial-boost',s.replace('t.multiplier = 1.3;','t.multiplier = 1.0;',1)),
]
# The source ignores ability multipliers while choosing. This mutation is
# retained only if the actual matrix distinguishes its selected stat.
ability_start=s.rfind('        const pr_stat_params sp =',0,start)
faults.append(('include-ability',s[:ability_start]+s[ability_start:end].replace('.ignore_ability = true','.ignore_ability = false',1)+s[end:]))
flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(r/x) for x in ['battle-core/include','battle-core/gen/out','battle-core/build']]]
with tempfile.TemporaryDirectory(prefix='highest-stat-guards-',dir=r/'work') as d:
 d=Path(d)
 for name,changed in faults:
  assert changed!=s,name;src=d/'fault.c';src.write_text(changed);obj=d/'fault.o';exe=d/'fault'
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,r/'battle-core/tests/wide-highest-stat-test.c',obj,r/'battle-core/build/libprcore.a','-lm','-o',exe],check=True,capture_output=True)
  result=subprocess.run([exe,trace],env=env,capture_output=True);assert result.returncode==1,(name,result.returncode,result.stdout[-500:],result.stderr[-500:]);print(name,'caught',flush=True)
 rows=trace.read_text().splitlines();bad={'footer':rows[:-1]+['HIGHEST_STAT_COMPLETE 1'],'missing-footer':rows[:-1],'tail':rows+['JUNK'],'version':['HIGHEST_STAT 2 8192 777']+rows[1:]}
 for name,prefix,col,value in [('case','C ',1,'0.5'),('owner','C ',2,'4'),('mode','C ',3,'2'),('id','I ',5,'4294967296'),('level','I ',6,'2147483648'),('nan','I ',26,'NaN'),('step','STEP ',1,'4'),('result','STEP ',2,'2'),('tag-count','TAG ',6,'33'),('queue','QCOUNT ',1,'33')]:
  ix=next(i for i,row in enumerate(rows) if row.startswith(prefix));row=rows[ix].split();row[col]=value;changed=rows.copy();changed[ix]=' '.join(row);bad[name]=changed
 for name,changed in bad.items():
  p=d/'bad.txt';p.write_text('\n'.join(changed)+'\n');result=subprocess.run([runner,p],env=env,capture_output=True);assert result.returncode==2,(name,result.returncode);print(name,'rejected',flush=True)
 print(f'{len(faults)} runtime faults caught; {len(bad)} malformed captures rejected')

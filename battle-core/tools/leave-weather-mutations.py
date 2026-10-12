#!/usr/bin/env python3
"""Compile actual runtime mutations and replay unchanged source traces."""
from pathlib import Path
import os, subprocess, sys, tempfile
root=Path(__file__).resolve().parent.parent;trace=Path(sys.argv[1]).resolve()
attrs=(root/'src/ab_attrs.c').read_text();phases=(root/'src/phases.c').read_text()
start=attrs.index('    case PR_AC_PreLeaveFieldClearWeatherAbAttr: {');end=attrs.index('    /* ---- coming in:',start);leaf=attrs[start:end]
mutants=[
 ('never-clear','attrs','pr_try_set_weather(bt, c->e, WEATHER_TYPE_NONE, NULL);','(void)weather;'),
 ('wrong-weather-clears','attrs','GATE(bt->weather == weather && !another);','GATE(!another);'),
 ('ignore-other-holder','attrs','GATE(bt->weather == weather && !another);','GATE(bt->weather == weather);'),
 ('wrong-peer-weather','attrs','->weatherType == weather','->weatherType != weather'),
 ('ignore-peer-passive','attrs','passive < 2 && !another','passive < 1 && !another'),
 ('fainted-peer-retains','attrs','q->hp <= 0','false'),
 ('ignore-peer-suppression','attrs','!pr_can_apply_ability(bt, q, ability)','false'),
 ('effective-weather-gate','attrs','bt->weather == weather','pr_get_effective_weather_for_move(bt, p) == weather'),
 ('simulation-needs-queue','attrs','if (pp->simulated) return 1;','if (false) return 1;'),
 ('missing-leave-caller','phases','pr_apply_ab_attrs(bt, PR_AC_PreLeaveFieldAbAttr, &params);','(void)params;'),
]
with tempfile.TemporaryDirectory(prefix='leave-weather-mutants-') as work:
 tmp=Path(work);flags=[os.environ.get('CC','gcc'),'-O2','-std=c99','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'gen/out')]
 for name,target,a,b in [('intact','attrs','','')]+mutants:
  bodies={'attrs':attrs,'phases':phases}
  if a:
   part=leaf if target=='attrs' else phases;assert a in part,(name,a);changed=part.replace(a,b,1)
   bodies[target]=attrs[:start]+changed+attrs[end:] if target=='attrs' else changed
  objects=[]
  for key,filename in [('attrs','ab_attrs.c'),('phases','phases.c')]:
   p=tmp/filename;p.write_text(bodies[key]);obj=p.with_suffix('.o');objects.append(str(obj));subprocess.run(flags+['-c',str(p),'-o',str(obj)],check=True)
  exe=tmp/'leave-weather-test';subprocess.run(flags+['-I'+str(root/'tests'),str(root/'tests/leave-weather-test.c')]+objects+[str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
  r=subprocess.run([str(exe),str(trace)],capture_output=True,text=True);print(name,'exit',r.returncode,flush=True)
  for line in r.stdout.splitlines():
   if line.startswith('covered ') and not line.startswith('covered turns'):print(line,flush=True)
  if r.returncode!=(0 if name=='intact' else 1):raise SystemExit(r.stdout[-1000:]+r.stderr)

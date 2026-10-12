#!/usr/bin/env python3
"""Actual production mutants replay unchanged source expectations."""
from pathlib import Path
import os, subprocess, sys, tempfile
root=Path(__file__).resolve().parent.parent
trace=Path(sys.argv[1]).resolve()
attrs=(root/'src/ab_attrs.c').read_text();phases=(root/'src/phases.c').read_text()
start=attrs.index('    case PR_AC_PostVictoryStatStageChangeAbAttr: {')
end=attrs.index('    /* ---- coming in:',start)
leaf=attrs[start:end]
mutants=[
 ('victory-stage-two','attrs','sg[0] = 1;','sg[0] = 2;'),
 ('last-tie-wins','attrs','value > highest','value >= highest'),
 ('wrong-stat-order','attrs','STAT_ATK, STAT_DEF, STAT_SPATK, STAT_SPDEF, STAT_SPD','STAT_SPD, STAT_SPDEF, STAT_SPATK, STAT_DEF, STAT_ATK'),
 ('stage-weighted-winner','attrs','double value = pr_get_stat(p, stats[i], false);','double value = pr_get_stat(p, stats[i], false) * (p->stat_stages[stats[i]-1] + 7);'),
 ('ignore-fixed-stage','attrs','changes->fixed.v[i].stages','2'),
 ('wrong-soul-heart-stat','attrs','st = (int8_t)x->stat','st = STAT_ATK'),
 ('wrong-soul-heart-stage','attrs','sg = (int8_t)x->stages','sg = 2'),
 ('victory-simulation-queues','attrs','if (pp->simulated) return 1;','if (false) return 1;'),
 ('missing-victory-caller','phases','pr_apply_ab_attrs(bt, PR_AC_PostVictoryAbAttr, &params);','(void)params;'),
 ('missing-all-knockout-caller','phases','pr_apply_ab_attrs(bt, PR_AC_PostKnockOutAbAttr, &params);','(void)params;'),
 ('knockout-only-direct','phases','pr_apply_ab_attrs(bt, PR_AC_PostKnockOutAbAttr, &params);','if (p->n_attacks_received > 0) pr_apply_ab_attrs(bt, PR_AC_PostKnockOutAbAttr, &params);'),
]
with tempfile.TemporaryDirectory(prefix='knockout-stats-mutants-') as work:
    tmp=Path(work)
    flags=[os.environ.get('CC','gcc'),'-O2','-std=c99','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'gen/out')]
    for name,target,a,b in [('intact','attrs','','')]+mutants:
        bodies={'attrs':attrs,'phases':phases}
        if a:
            part=leaf if target=='attrs' else phases
            assert a in part,(name,a)
            changed=part.replace(a,b,1)
            bodies[target]=attrs[:start]+changed+attrs[end:] if target=='attrs' else changed
        objects=[]
        for key,filename in [('attrs','ab_attrs.c'),('phases','phases.c')]:
            p=tmp/filename;p.write_text(bodies[key]);obj=p.with_suffix('.o');objects.append(str(obj))
            subprocess.run(flags+['-c',str(p),'-o',str(obj)],check=True)
        exe=tmp/'knockout-stats-test'
        subprocess.run(flags+['-I'+str(root/'build'),str(root/'tests/knockout-stats-test.c')]+objects+[str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
        r=subprocess.run([str(exe),str(trace)],capture_output=True,text=True)
        print(name,'exit',r.returncode,flush=True)
        for line in r.stdout.splitlines():
            if line.startswith(('covered ','Knockout stats queue')) and not line.startswith('covered turns'): print(line,flush=True)
        if r.returncode!=(0 if name=='intact' else 1):raise SystemExit('unexpected status: '+r.stderr+r.stdout[-1000:])

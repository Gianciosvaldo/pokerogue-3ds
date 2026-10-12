#!/usr/bin/env python3
"""Production selection/history mutants against unchanged actual source rows."""
from pathlib import Path
import os, subprocess, sys, tempfile
root=Path(__file__).resolve().parent.parent
trace=Path(sys.argv[1]).resolve()
sources={n:(root/'src'/n).read_text() for n in ['commands.c','history.c']}
mutants=[
 ('allow-repeated-move','commands.c','last && last->move == move','last && false'),
 ('restrict-all-after-history','commands.c','last && last->move == move','last != NULL'),
 ('ignore-failed-history','commands.c','last && last->move == move','last && last->result == MOVE_RESULT_SUCCESS && last->move == move'),
 ('ignore-blood-moon','commands.c','move == MOVE_ID_GIGATON_HAMMER || move == MOVE_ID_BLOOD_MOON','move == MOVE_ID_GIGATON_HAMMER'),
 ('skip-newest-history','commands.c','pr_last_move(bt, p, 0);\n        if (last && last->move == move)', 'pr_last_move(bt, p, 1);\n        if (last && last->move == move)'),
 ('filter-virtual-history','commands.c','pr_last_move(bt, p, 0);\n        if (last && last->move == move)', 'pr_last_non_virtual_move(bt, p, false, true, (bool[]){false});\n        if (last && last->move == move)'),
 ('restriction-during-execution','commands.c','if (for_selection && !pr_move_selectable','if (!pr_move_selectable'),
 ('no-struggle-fallback','commands.c','return MOVE_ID_STRUGGLE;','return -1;'),
 
]
with tempfile.TemporaryDirectory(prefix='consecutive-move-mutants-') as work:
    tmp=Path(work);flags=[os.environ.get('CC','gcc'),'-O2','-std=c99','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'gen/out')]
    for name,target,a,b in [('intact','commands.c','','')]+mutants:
        bodies=dict(sources)
        if a:
            assert a in bodies[target],(name,a)
            bodies[target]=bodies[target].replace(a,b,1)
        objects=[]
        for filename,source in bodies.items():
            p=tmp/filename;p.write_text(source);o=p.with_suffix('.o');objects.append(str(o));subprocess.run(flags+['-c',str(p),'-o',str(o)],check=True)
        exe=tmp/'consecutive-move-test';subprocess.run(flags+[str(root/'tests/consecutive-move-test.c')]+objects+[str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
        r=subprocess.run([str(exe),str(trace)],capture_output=True,text=True);print(name,'exit',r.returncode,flush=True)
        if r.returncode!=(0 if name=='intact' else 1):raise SystemExit(r.stderr+r.stdout)

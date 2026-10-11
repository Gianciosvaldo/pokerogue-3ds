#!/usr/bin/env python3
"""Actual runtime mutants against unchanged source full-turn traces."""
from pathlib import Path
import os,subprocess,sys,tempfile
root=Path(__file__).resolve().parent.parent
trace=Path(sys.argv[1]).resolve()
source=(root/'src/ab_attrs.c').read_text();start=source.index('    case PR_AC_MoodyAbAttr: {');end=source.index('    case PR_AC_SpeedBoostAbAttr: {',start)
leaf=source[start:end]
mutations=[('wrong-speed-boost-gate','GATE(true);','GATE(!p->switched_in_this_turn && !p->failed_run_away);'),('wrong-stat-order','STAT_ATK, STAT_DEF, STAT_SPATK, STAT_SPDEF, STAT_SPD','STAT_DEF, STAT_ATK, STAT_SPATK, STAT_SPDEF, STAT_SPD'),('raise-one','&chosen, 1, 2,','&chosen, 1, 1,'),('lower-two','&chosen, 1, -1,','&chosen, 1, -2,'),('same-stat-allowed','if (lower[j] != chosen)','if (true)'),('raise-at-cap','< 6) raise','<= 6) raise'),('lower-at-floor','> -6) lower','>= -6) lower'),('no-lower-at-max','if (nl) {','if (nl && nr) {'),('no-raise-at-floor','if (nr) {','if (nr && nl) {'),('simulation-draws','if (pp->simulated) return 1;','if (false) return 1;')]
with tempfile.TemporaryDirectory(prefix='moody-mutations-') as work:
 tmp=Path(work)
 for name,a,b in [('intact','','')]+mutations:
  if a:
   assert leaf.count(a)==1,(name,a)
   mutated=leaf.replace(a,b,1)
  else:mutated=leaf
  (tmp/'ab_attrs.c').write_text(source[:start]+mutated+source[end:])
  flags=[os.environ.get('CC','gcc'),'-O2','-std=c99','-ffp-contract=off','-I'+str(root/'include'),'-I'+str(root/'gen/out')]
  subprocess.run(flags+['-c',str(tmp/'ab_attrs.c'),'-o',str(tmp/'ab_attrs.o')],check=True)
  exe=tmp/'moodytest';subprocess.run(flags+['-I'+str(root/'tests'),str(root/'tests/moodytest.c'),str(tmp/'ab_attrs.o'),str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
  r=subprocess.run([str(exe),str(trace)],capture_output=True,text=True)
  print(name,'exit',r.returncode,flush=True)
  for line in r.stdout.splitlines():
   if line.startswith(('covered ','Moody ')) and not line.startswith('covered turns'):print(line,flush=True)
  if r.returncode!=(0 if name=='intact' else 1):raise SystemExit('unexpected mutation status: '+r.stdout[-1000:]+r.stderr)

#!/usr/bin/env python3
"""Strict parse2 versus typed semantic1; intact controls must pass first."""
from pathlib import Path
import subprocess,sys,tempfile
typed_exe,typed_path,fixture_exe,fixture_path=sys.argv[1:]
def run(exe,path):return subprocess.run([exe,str(path)],capture_output=True).returncode
if run(typed_exe,typed_path) or run(fixture_exe,fixture_path):raise SystemExit('intact controls must pass')
s=Path(typed_path).read_text();ls=s.splitlines(True);n=int(ls[0].split()[-1]);variants=[]
def add(name,text,code=2,exe=typed_exe):variants.append((name,text,code,exe))
add('typed-missing-footer',''.join(ls[:-1]));add('typed-wrong-count',s.replace(f'# OPTIONAL_COMPLETE {n}\n',f'# OPTIONAL_COMPLETE {n-1}\n'));add('typed-fractional-request',ls[0].rstrip()+'.5\n'+''.join(ls[1:]));add('typed-trailing',s+'CASE 0\n');add('typed-case-order',s.replace('CASE 1\n','CASE 0\n',1));add('typed-version',s.replace('# OPTIONAL_PHASE 1 ','# OPTIONAL_PHASE 2 ',1))
for tag in ['CASE ','RUN\n','QUEUE ','END ']:
 i=next(i for i,l in enumerate(ls) if l.startswith(tag));add('typed-missing-'+tag.strip(),''.join(ls[:i]+ls[i+1:]));add('typed-duplicate-'+tag.strip(),''.join(ls[:i]+[ls[i]]+ls[i:]))
for token,code in [('X',2),('I-1',1),('U',1),('N',1)]:
 if 'SwitchSummonPhase:A' not in s:raise SystemExit('real pending absent fixture required')
 add('typed-absence-'+token,s.replace('SwitchSummonPhase:A','SwitchSummonPhase:'+token,1),code)
for token in ['I0.5','I4','']:
 if 'MovePhase:I0' not in s:raise SystemExit('real indexed fixture required')
 add('typed-index-'+token,s.replace('MovePhase:I0','MovePhase:'+token,1))
add('typed-empty-dyn',s.replace('dyn=MovePhase:I0','dyn=',1));add('typed-duplicate-dyn-group',s.replace('dyn=MovePhase:I0','dyn=MovePhase:I0,MovePhase:I0',1))
s=Path(fixture_path).read_text();ls=s.splitlines(True)
def fixture(name,text,code=2):add('fixture-'+name,text,code,fixture_exe)
fixture('missing-footer',''.join(ls[:-1]));fixture('count',s.replace('OPTIONAL_FIXTURES 1 6','OPTIONAL_FIXTURES 1 5'));fixture('fractional-count',s.replace('OPTIONAL_FIXTURES 1 6','OPTIONAL_FIXTURES 1 6.5'));fixture('version',s.replace('OPTIONAL_FIXTURES 1 6','OPTIONAL_FIXTURES 2 6'));fixture('footer',s.replace('Z 6\n','Z 5\n'));fixture('trailing',s+'C 0\n');fixture('case-order',s.replace('C 1\n','C 0\n',1))
for tag in ['R ','P ','A ','QCOUNT ','Q ','S ','E ']:
 i=next(i for i,l in enumerate(ls) if l.startswith(tag));fixture('missing-'+tag.strip(),''.join(ls[:i]+ls[i+1:]));fixture('duplicate-'+tag.strip(),''.join(ls[:i]+[ls[i]]+ls[i:]))
fixture('fractional-level-count',s.replace('QCOUNT 2 1 0','QCOUNT 2 1.5 0',1));fixture('invalid-deferred',s.replace('QCOUNT 2 1 0','QCOUNT 2 1 2',1))
fixture('unknown-phase',s.replace('SwitchSummonPhase A','UNKNOWN A',1));fixture('fractional-index',s.replace('PostSummonPhase I0','PostSummonPhase I0.5',1));fixture('invalid-presence',s.replace('SwitchSummonPhase A','SwitchSummonPhase X',1))
for token in ['I-1','U','N']:fixture('absence-'+token,s.replace('Q D 0 0 SwitchSummonPhase A','Q D 0 0 SwitchSummonPhase '+token,1),1)
fixture('genuine-minus-one-is-absence',s.replace('CommonAnimPhase I-1','CommonAnimPhase A',1),1)
with tempfile.TemporaryDirectory(prefix='optional-phase-malformed-') as d:
 for name,text,code,exe in variants:
  path=Path(d)/name;path.write_text(text);result=run(exe,path);print(name,result,flush=True)
  if result!=code:raise SystemExit(f'{name}: expected{code}, actual{result}')
print(f'Optional transport parse refusals{sum(c==2 for _,_,c,_ in variants)}, typed semantic refusals{sum(c==1 for _,_,c,_ in variants)} after intact0 controls')

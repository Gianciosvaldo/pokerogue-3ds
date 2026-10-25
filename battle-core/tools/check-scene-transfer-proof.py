#!/usr/bin/env python3
"""Compiled faults and malformed-input controls for native scene observations."""
from pathlib import Path
import os,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[2]
trace=Path(sys.argv[1]).resolve();env=os.environ.copy();assert env.get('TURNTEST_LOCALES')
runner=root/'battle-core/build/scene-transfer-test'
subprocess.run([runner,trace],env=env,check=True)
source=(root/'battle-core/src/item_transfer.c').read_text()
cut=source.index('bool pr_try_transfer_held_item(')
faults=[
 ('preflight-detached','pre','if (copy.stack_count!=taken || !source) return true;','if (!source) return false;\n    if (copy.stack_count!=taken) return true;'),
 ('preflight-membership','pre','if (stock==&bt->mods[from_side][k]) return true;','if (true) return true;'),
 ('preflight-cap','pre','if (m->stack_count>=cap) return false;','if (m->stack_count>cap) return false;'),
 ('quantity','try','int taken=copy.stack_count<quantity ? copy.stack_count : quantity;','int taken=copy.stack_count>0 ? 1 : 0;'),
 ('capacity-room','try','if (matching>=0 && taken>cap-existing) taken=cap-existing;','if (false) taken=cap-existing;'),
 ('clone-virtual','try','copy.virtual_stack_count=0;','copy.virtual_stack_count=1;'),
 ('clone-lock','try','copy.is_transferable=true;','copy.is_transferable=false;'),
 ('ignore-update','try','if (!ignore_update) pr_refresh_berry_modifiers(bt,side);','pr_refresh_berry_modifiers(bt,side);'),
 ('item-lost','try','if (source && item_lost) {','if (source) {'),
 ('detached-removal','try','if (!bt->mods[from_side][index].stack_count && source) {','if (!bt->mods[from_side][index].stack_count) {'),
 ('same-side-index','try','if (from_side==side && matching>index) matching--;','if (false) matching--;'),
 ('zero-cleanup','cleanup','''        m.virtual_stack_count = 0;
        if (m.stack_count == 0) continue;
        if (!pr_mod_is_ported_item(&m)) pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);''','''        if (!pr_mod_is_ported_item(&m)) pr_note_unported(bt, PR_FEAT_TURN_MODIFIERS);
        m.virtual_stack_count = 0;
        if (m.stack_count == 0) continue;'''),
]
flags=['-O2','-std=c99','-ffp-contract=off','-I'+str(root/'battle-core/include'),'-I'+str(root/'battle-core/gen/out')]
with tempfile.TemporaryDirectory(prefix='scene-transfer-guards-',dir=root/'work') as tmp:
 tmp=Path(tmp)
 for name,part,old,new in faults:
  prefix,scope,suffix=('',source[:cut],source[cut:]) if part=='pre' else (source[:cut],source[cut:],'')
  if part=='cleanup':prefix='';scope=(root/'battle-core/src/modifiers.c').read_text();suffix=''
  assert scope.count(old)==1,(name,scope.count(old))
  src=tmp/(name+'.c');obj=tmp/(name+'.o');binary=tmp/name
  src.write_text(prefix+scope.replace(old,new)+suffix)
  subprocess.run(['gcc',*flags,'-c',src,'-o',obj],check=True,capture_output=True)
  subprocess.run(['gcc',*flags,root/'battle-core/tests/scene-transfer-test.c',obj,root/'battle-core/build/libprcore.a','-lm','-o',binary],check=True,capture_output=True)
  r=subprocess.run([binary,trace],env=env,capture_output=True)
  assert r.returncode==1,(name,r.returncode,r.stdout,r.stderr)
  print(name,'caught',flush=True)
 rows=trace.read_text().splitlines()
 malformed={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],
  'version':[rows[0].replace(' 2 ',' 3 ',1)]+rows[1:],
  'count':[rows[0].replace('3608','3607')]+rows[1:],
  'footer':rows[:-1]+['SCENETRANSFER_COMPLETE 3607']}
 for name,prefix,col,value in [('fractional-case','C ',1,'0.5'),('holder','C ',2,'4'),('side','C ',3,'2'),('index','C ',4,'96'),('mode','C ',5,'2'),('quantity','C ',6,'-1'),('update','C ',7,'2'),('loss','C ',8,'2'),('orphan','C ',9,'2'),('result','C ',10,'2'),('unknown-item','S ',1,'UNKNOWN'),('owner','S ',3,'-2'),('negative-stack','S ',4,'-1'),('virtual','S ',5,'-1'),('lock','S ',6,'2'),('id','MON ',1,'4294967296'),('nan-actor','A ',1,'nan')]:
  i=next(i for i,r in enumerate(rows) if r.startswith(prefix));v=rows[i].split();v[col]=value;t=rows.copy();t[i]=' '.join(v);malformed[name]=t
 for name,lines in malformed.items():
  path=tmp/'bad.txt';path.write_text('\n'.join(lines)+'\n')
  r=subprocess.run([runner,path],env=env,capture_output=True)
  assert r.returncode==2,(name,r.returncode,r.stdout,r.stderr)
  print(name,'rejected',flush=True)
 print(f'{len(faults)} compiled faults caught; {len(malformed)} malformed captures rejected')

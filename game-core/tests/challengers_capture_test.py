#!/usr/bin/env python3
"""Retain both native captures; reject missing, malformed and forged records."""
import copy,json,pathlib,subprocess,sys,tempfile
root=pathlib.Path(__file__).resolve().parents[2]
proofs=[(root/'game-core/tests/fixtures/challengers.native.json',root/'game-core/tools/check_challengers_capture.py'),(root/'run-core/tests/fixtures/pair-trainers.native.json',root/'run-core/tools/check_pair_capture.py')]
count=0
with tempfile.TemporaryDirectory(prefix='challengers-capture-') as d:
 for proof,(capture,checker) in enumerate(proofs):
  subprocess.run([sys.executable,str(checker),str(capture)],check=True)
  rows=json.loads(capture.read_text())
  for mutation in range(10):
   x=copy.deepcopy(rows)
   if mutation==0:x.pop()
   elif mutation==1:x[1]=x[0]
   elif mutation==2:x[0]['index']=True
   elif mutation==3:x[0]['extra']='forged'
   elif proof==0:
    if mutation==4:x[0]['configs'][0]['type']+=1
    elif mutation==5:x[0]['constructed']['levels'][0]+=1
    elif mutation==6:x[0]['constructed']['template']['members'][0][1]^=1
    elif mutation==7:x[0]['rewards']['funcs']=[]
    elif mutation==8:x[0]['configs'][0]['sprite']='unknown'
    else:x[0]['constructed']['nameIndex']+=1
   else:
    if mutation==4:x[0]['observed']['species']+=1
    elif mutation==5:x[0]['observed']['slot']=2
    elif mutation==6:x[0]['before']=[1]
    elif mutation==7:x[0]['observed']['rng'][1]=float('nan')
    elif mutation==8:x[0]['kind']=3
    else:x[0]['strength']+=1
   p=pathlib.Path(d)/f'{proof}-{mutation}.json';p.write_text(json.dumps(x,separators=(',',':'))+'\n')
   r=subprocess.run([sys.executable,str(checker),str(p)],capture_output=True,text=True)
   if not r.returncode:raise SystemExit(f'accepted forged capture {proof}/{mutation}')
   count+=1
print(f'Challengers/pair capture: {count} malformed/forged observations rejected')

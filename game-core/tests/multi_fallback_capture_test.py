#!/usr/bin/env python3
"""Retain native observations and prove capture corruption is rejected."""
import copy,json,pathlib,subprocess,sys,tempfile
root=pathlib.Path(__file__).resolve().parents[1];capture=root/'tests/fixtures/multi-fallback.native.json';checker=root/'tools/check_multi_fallback.py'
subprocess.run([sys.executable,str(checker),str(capture)],check=True)
rows=json.loads(capture.read_text());mutations=[]
for kind in range(10):
 x=copy.deepcopy(rows)
 if kind==0:x.pop()
 elif kind==1:x[1]=x[0]
 elif kind==2:x[0]['unexpected']=1
 elif kind==3:x[0]['context']=True
 elif kind==4:x[0]['factors']=[float('nan')]
 elif kind==5:x[0]['ok']=not x[0]['ok']
 elif kind==6:x[0]['factors'][0]=0.123
 elif kind==7:x[0]['lenses'][0]=2
 elif kind==8:x[0]['hits']=3;x[0]['factors']=[0.7,0.2,0.1]
 else:x[0]['oldArg']=5
 mutations.append(x)
with tempfile.TemporaryDirectory(prefix='multi-fallback-') as d:
 for i,x in enumerate(mutations):
  p=pathlib.Path(d)/f'bad-{i}.json';p.write_text(json.dumps(x))
  result=subprocess.run([sys.executable,str(checker),str(p)],capture_output=True,text=True)
  if result.returncode==0:raise SystemExit(f'corruption {i} was accepted')
print('Multi fallback capture: 10 malformed/forged native observations rejected')

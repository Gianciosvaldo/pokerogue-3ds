#!/usr/bin/env python3
# Verify that registration/inheritance drift is rejected, without changing source fixtures.
from pathlib import Path
import copy,gzip,json,os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];CORE=ROOT/'battle-core';audit=CORE/'tools/audit-move-inventory.py'
subprocess.run(['make','-s','-C',str(CORE),'build/move-inventory-dump'],check=True)
binary=CORE/'build/move-inventory-dump';lib=CORE/'build/libprcore.a'
x=json.loads(gzip.decompress((CORE/'inventory/move-attributes.source.json.gz').read_bytes()))
def execute(capture=None,dump=binary):
 return subprocess.run(['python3',str(audit),'--dump',str(dump)]+(['--capture',str(capture)]if capture else[]),stdout=subprocess.PIPE,stderr=subprocess.PIPE)
assert execute().returncode==0
with tempfile.TemporaryDirectory(prefix='move-inventory-guards-')as scratch:
 d=Path(scratch);variants={}
 y=copy.deepcopy(x);y['moves'].pop();variants['lost-move']=y
 y=copy.deepcopy(x);y['moves'][0]['id']=1;variants['duplicate-id']=y
 y=copy.deepcopy(x);y['moves'][1]['unimplemented']=True;variants['wrong-disabled']=y
 y=copy.deepcopy(x);row=next(m for m in y['moves']if len(m['attrs'])>=2 and m['attrs'][0]!=m['attrs'][1]);row['attrs'].reverse();variants['attribute-order']=y
 y=copy.deepcopy(x);row=next(m for m in y['moves']if m['chargeAttrs']);row['chargeAttrs']=[];variants['lost-charge']=y
 y=copy.deepcopy(x);row=next(m for m in y['moves']if m['attrs']);row['attrs'][0][-1]='OtherAttr';variants['wrong-parent']=y
 y=copy.deepcopy(x);y['moves'][1]['unimplemented']=0;variants['numeric-flag']=y
 y=copy.deepcopy(x);y['sourcePin']='0'*40;variants['wrong-pin']=y
 for name,y in variants.items():
  path=d/(name+'.json.gz');path.write_bytes(gzip.compress(json.dumps(y).encode()));r=execute(path);assert r.returncode!=0,(name,'accepted');print('caught capture',name)
 source=(CORE/'tools/move-inventory-dump.c').read_text()
 faults={
 'omit-final-move':('i<PR_MOVE_COUNT','i<PR_MOVE_COUNT-1'),
 'omit-charge':('charge<2','charge<1'),
 'reverse-attrs':('pr_attrs[first+k].cls','pr_attrs[first+n-1-k].cls'),
 'wrong-disabled':('i,m->unimplemented','i,!m->unimplemented'),
 'omit-inheritance':('if(pr_attr_is(cls,c))','if(c==cls)'),
 }
 for name,(old,new)in [('pristine',(None,None)),*faults.items()]:
  text=source
  if old:assert text.count(old)==1,(name,'anchor');text=text.replace(old,new)
  src=d/'dump.c';out=d/'dump';src.write_text(text)
  subprocess.run([os.environ.get('CC','cc'),'-O1','-std=c99','-Wl,--strip-debug','-I'+str(CORE/'include'),'-I'+str(CORE/'gen/out'),str(src),str(lib),'-lm','-o',str(out)],check=True)
  r=execute(dump=out);assert (r.returncode==0)==(name=='pristine'),(name,r.stderr.decode())
  print('compiled',name,'passed'if name=='pristine'else'caught')
print('Move inventory guards: 8 malformed/forged captures, 5 compiled faults, one pristine control passed')

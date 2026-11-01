#!/usr/bin/env python3
"""Reject malformed captures and divergent C projections without accepting forged expectations."""
import copy,gzip,json,pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
fixture=root/'tests/fixtures/trainer-constructor.native.json.gz'
builder=root/'tools/build-trainer-constructor-proof.py'
x=json.loads(gzip.decompress(fixture.read_bytes()))
with tempfile.TemporaryDirectory(prefix='trainer-transport-')as directory:
 d=pathlib.Path(directory);source=d/'native.json';out=d/'native.inc';reference=d/'reference.inc'
 subprocess.run(['python3',str(builder),str(fixture),str(reference)],check=True)
 def reject(name,mutate):
  y=copy.deepcopy(x);mutate(y);source.write_text(json.dumps(y))
  r=subprocess.run(['python3',str(builder),str(source),str(out)],capture_output=True)
  assert r.returncode or out.read_bytes()!=reference.read_bytes(),name+' accepted forged capture'
  print(name+': rejected')
 reject('original-member',lambda y:y['rows'][0]['atMoves']['after'].__setitem__(1,.5))
 reject('missing-party',lambda y:y['parties'].pop())
 reject('wrong-label',lambda y:y['parties'][0].__setitem__('type',63))
 reject('missing-member',lambda y:y['parties'][0]['members'].pop())
 reject('extra-field',lambda y:y['parties'][0]['members'][0]['atMoves']['input'].__setitem__('invented',1))
 reject('nonfinite-rng',lambda y:y['parties'][0]['members'][0]['atMoves']['before'].__setitem__(1,float('nan')))
 reject('integer-rng',lambda y:y['parties'][0]['members'][0]['atMoves']['before'].__setitem__(1,5))
 reject('nonfresh-role',lambda y:y['parties'][0]['members'][0]['atMoves']['input'].__setitem__('onField',True))
 reject('tera-role',lambda y:y['parties'][0]['members'][0]['atMoves']['input'].__setitem__('willTera',True))
 reject('template-size',lambda y:y['parties'][0]['template'].__setitem__('size',6))
 reject('boolean-level',lambda y:y['parties'][0].__setitem__('level',True))
 # Mutate consistent full-party snapshots: transport is allowed to copy native data,
 # but the committed independent projection must detect forged moves or outcomes.
 def consistent(y,key,value):
  p=y['parties'][0];m=p['members'][0]
  for f in [m['atMoves']['output'],m['atConstructor']['output'],m['finished'],p['refreshed'][0]]:f[key]=copy.deepcopy(value)
 reject('forged-consistent-moves',lambda y:consistent(y,'moves',[{'move':1,'ppUsed':0,'ppUp':0}]))
 def final_ivs(y):
  p=y['parties'][0];m=p['members'][0]
  for f in [m['atConstructor']['output'],m['finished'],p['refreshed'][0]]:f['ivs'][0]=(f['ivs'][0]+1)%32
 reject('forged-finish-ivs',final_ivs)
 source.write_text('{"version":1,"version":1}')
 assert subprocess.run(['python3',str(builder),str(source),str(out)],capture_output=True).returncode
print('Trainer capture transport: 14 malformed/forged captures rejected')

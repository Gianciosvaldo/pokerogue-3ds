#!/usr/bin/env python3
# Strict malformed input and independently frozen projection controls.
import copy,gzip,json,pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
fixture=root/'tests/fixtures/grunt-constructor.native.json.gz';builder=root/'tools/build-grunt-constructor-proof.py'
x=json.loads(gzip.decompress(fixture.read_bytes()))
with tempfile.TemporaryDirectory(prefix='grunt-transport-')as directory:
 d=pathlib.Path(directory);source=d/'native.json.gz';out=d/'native.inc';reference=d/'reference.inc'
 subprocess.run(['python3',str(builder),str(fixture),str(reference)],check=True)
 def reject(name,mutate):
  y=copy.deepcopy(x);mutate(y);source.write_bytes(gzip.compress(json.dumps(y).encode()))
  result=subprocess.run(['python3',str(builder),str(source),str(out)],capture_output=True)
  assert result.returncode or out.read_bytes()!=reference.read_bytes(),name+' accepted forged capture'
  print('caught',name,flush=True)
 reject('missing-first',lambda y:y['rows'].pop())
 reject('missing-party',lambda y:y['parties'].pop())
 reject('missing-member',lambda y:y['parties'][0]['members'].pop())
 reject('admin-type',lambda y:y['parties'][0].__setitem__('type',65))
 reject('wrong-variant',lambda y:y['parties'][0].__setitem__('variant',1))
 reject('wrong-double',lambda y:y['parties'][0].__setitem__('double',True))
 reject('profile-owner',lambda y:y['profiles'][0].__setitem__('partyMemberFuncs',['0']))
 reject('profile-egg',lambda y:y['profiles'][0].__setitem__('allowEggMoves',True))
 reject('profile-boolean',lambda y:y['profiles'][0].__setitem__('hasDouble',1))
 reject('nonfinite-rng',lambda y:y['parties'][0]['members'][0]['atMoves']['before'].__setitem__(1,float('nan')))
 reject('boolean-level',lambda y:y['parties'][0].__setitem__('level',True))
 reject('boolean-type',lambda y:y['parties'][0].__setitem__('type',True))
 reject('unreviewed-frame',lambda y:y['parties'][0]['members'][0]['atMoves']['input'].__setitem__('invented',1))
 reject('tera-role',lambda y:y['parties'][0]['members'][0]['atMoves']['input'].__setitem__('willTera',True))
 reject('preexisting-moves',lambda y:y['parties'][0]['members'][0]['atMoves']['input'].__setitem__('moves',[]))
 def consistent(y,key,value):
  p=y['parties'][0];m=p['members'][0]
  for frame in[m['atMoves']['output'],m['atConstructor']['output'],m['finished'],p['refreshed'][0]]:frame[key]=copy.deepcopy(value)
 reject('forged-consistent-moves',lambda y:consistent(y,'moves',[{'move':1,'ppUsed':0,'ppUp':0}]))
 reject('forged-finish-rng',lambda y:y['parties'][0]['members'][0]['atConstructor']['after'].__setitem__(1,.5))
 source.write_bytes(gzip.compress(b'{"version":1,"version":1}'))
 assert subprocess.run(['python3',str(builder),str(source),str(out)],capture_output=True).returncode
print('Grunt capture transport: 18 malformed/forged captures rejected')

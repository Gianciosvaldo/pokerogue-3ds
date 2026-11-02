#!/usr/bin/env python3
"""Malformed-input and independent-projection controls for actual Gym observations."""
import copy,gzip,json,pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
frozen=root/'tests/fixtures/gym-egg.native.json.gz';builder=root/'tools/build-gym-egg-proof.py'
x=json.loads(gzip.decompress(frozen.read_bytes()))
with tempfile.TemporaryDirectory(prefix='gym-egg-transport-')as directory:
 d=pathlib.Path(directory);source=d/'native.json';out=d/'native.inc';reference=d/'reference.inc'
 subprocess.run(['python3',str(builder),str(frozen),str(reference)],check=True)
 def reject(name,mutate):
  y=copy.deepcopy(x);mutate(y);source.write_text(json.dumps(y))
  r=subprocess.run(['python3',str(builder),str(source),str(out)],capture_output=True)
  assert r.returncode or out.read_bytes()!=reference.read_bytes(),name+' accepted forged capture'
  print(name+': rejected',flush=True)
 reject('missing-party',lambda y:y['parties'].pop())
 reject('missing-member',lambda y:y['parties'][0]['members'].pop())
 reject('unreviewed-profile',lambda y:y['parties'][0].__setitem__('type',100))
 reject('false-egg-config',lambda y:y['parties'][0]['environment'].__setitem__('allowEggMoves',False))
 reject('callback-owner',lambda y:y['parties'][0]['members'][0]['atConstructor']['input'].__setitem__('postProcess',True))
 reject('second-generation',lambda y:y['parties'][0]['members'][0]['atConstructor']['generations'].append(y['parties'][0]['members'][0]['atMoves']))
 reject('tera-role',lambda y:y['parties'][0]['members'][0]['atMoves']['input'].__setitem__('willTera',True))
 reject('boss-role',lambda y:y['parties'][0]['members'][0]['atMoves']['input'].__setitem__('boss',True))
 reject('initial-team-index',lambda y:y['parties'][0]['members'][0]['atMoves']['input'].__setitem__('initialTeamIndex',1))
 reject('boolean-constructor-slot',lambda y:y['parties'][0]['members'][0]['atConstructor']['input'].__setitem__('slot',True))
 reject('nonfinite-rng',lambda y:y['parties'][0]['members'][0]['atMoves']['before'].__setitem__(1,float('nan')))
 reject('boolean-level',lambda y:y['parties'][0].__setitem__('level',True))
 reject('unknown-frame-field',lambda y:y['parties'][0]['members'][0]['atMoves']['input'].__setitem__('invented',1))
 def consistent(y,key,value,initial=False):
  p=y['parties'][0];m=p['members'][0]
  frames=[m['atMoves']['output'],m['atConstructor']['generations'][0]['output'],m['atConstructor']['output'],m['finished'],p['refreshed'][0]]
  if initial:frames +=[m['atMoves']['input'],m['atConstructor']['generations'][0]['input']]
  for f in frames:f[key]=copy.deepcopy(value)
 reject('forged-consistent-moves',lambda y:consistent(y,'moves',[{'move':1,'ppUsed':0,'ppUp':0}]))
 reject('forged-consistent-identity',lambda y:consistent(y,'id',1,True))
 reject('api-tera-role',lambda y:y['api'][0]['atMoves']['input'].__setitem__('willTera',True))
 reject('api-wrong-form',lambda y:y['api'][0].__setitem__('form',4))
 reject('api-forged-moves',lambda y:y['api'][0]['atMoves']['output'].__setitem__('moves',[{'move':1,'ppUsed':0,'ppUp':0}]))
 source.write_text('{"version":1,"version":1}')
 assert subprocess.run(['python3',str(builder),str(source),str(out)],capture_output=True).returncode
print('Gym capture transport: 19 malformed/forged captures rejected')

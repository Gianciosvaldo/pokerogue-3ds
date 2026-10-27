#!/usr/bin/env python3
"""Native variable-type transport, actual core/item owners, and failure controls."""
from pathlib import Path
import copy,gzip,json,os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];GC=ROOT/'game-core';BC=ROOT/'battle-core'
def command(args):
 p=subprocess.run(list(map(str,args)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if p.returncode:raise RuntimeError(str(args)+'\n'+p.stdout[-4000:])
 return p.stdout
def execute(args):return subprocess.run(list(map(str,args)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
flags=['-O1','-std=gnu11','-ffp-contract=off',*['-I'+str(ROOT/p)for p in ['game-core/include','game-core/src','game-core/tests','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out','battle-core/include','battle-core/gen/out']]]
libs=[GC/p for p in ['build/libgc.a','build/reward-core/libreward.a','build/ai-core/libai.a','build/run-core/librun.a','build/run-core/battle-core/libprcore.a']]
with tempfile.TemporaryDirectory(prefix='fusion-variable-types-')as td:
 temp=Path(td);raw=gzip.decompress((GC/'tests/fixtures/fusion-variable-types.native.json.gz').read_bytes());native=temp/'native.json';native.write_bytes(raw);inc=temp/'native.inc'
 builder=['python3',GC/'tools/build-fusion-variable-types-proof.py']
 command([*builder,native,inc]);assert inc.read_bytes()==(GC/'tests/fixtures/fusion-variable-types.native.inc').read_bytes()
 print(command(['make','-C',GC,'fusion-variable-types-host-test']),flush=True)
 cc=os.environ.get('CC','cc')
 if os.environ.get('FUSION_VARIABLE_SANITIZE')=='1':
  san=flags+['-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'];objects=[]
  for source in [BC/'src/move_attrs.c',GC/'src/gc_reward.c']:
   obj=temp/(source.stem+'.o');command([cc,*san,'-c',source,'-o',obj]);objects.append(obj)
  exe=temp/'san';command([cc,*san,GC/'tests/fusion_variable_types_host_test.c',*objects,*libs,'-lm','-o',exe]);print(command([exe]),flush=True)
  print('Scoped ASan/UBSan/LSan: core variable-move attributes and item-spawn owner; supporting archives unsanitized')
 else:
  source=(BC/'src/move_attrs.c').read_text()
  faults=[
   ('else if (user->fusion_species == SPECIES_ID_PALDEA_TAUROS)','else if (false)','omit fusion recognition'),
   ('else if (user->fusion_species == SPECIES_ID_PALDEA_TAUROS)','else if (user->fusion_species)','recognize unrelated fusion'),
   ('if (sp == SPECIES_ID_PALDEA_TAUROS)','if (sp == SPECIES_ID_PALDEA_TAUROS && !user->fusion_species)','let fusion override primary'),
   ('*type = POKEMON_TYPE_FIGHTING;\n            break;\n        case PR_AC_IvyCudgelTypeAttr:','*type = user->fusion_form == 1 ? POKEMON_TYPE_FIRE : user->fusion_form == 2 ? POKEMON_TYPE_WATER : POKEMON_TYPE_FIGHTING;\n            break;\n        case PR_AC_IvyCudgelTypeAttr:','use stored fusion form'),
   ('*type = POKEMON_TYPE_FIGHTING;\n            break;\n        case PR_AC_IvyCudgelTypeAttr:','*type = POKEMON_TYPE_FIRE;\n            break;\n        case PR_AC_IvyCudgelTypeAttr:','wrong fusion type'),
   ('*type = POKEMON_TYPE_FIGHTING;\n            break;\n        case PR_AC_IvyCudgelTypeAttr:','*type = POKEMON_TYPE_FIGHTING;pr_rng_seed_int(&bt->rng,100,0);\n            break;\n        case PR_AC_IvyCudgelTypeAttr:','consume getter RNG'),
  ]
  obj=temp/'normal.o';exe=temp/'normal';command([cc,*flags,'-c',BC/'src/move_attrs.c','-o',obj]);command([cc,*flags,GC/'tests/fusion_variable_types_host_test.c',obj,*libs,'-lm','-o',exe]);r=execute([exe]);assert r.returncode==0,r.stdout
  for old,new,label in faults:
   assert source.count(old)==1,(label,source.count(old));p=temp/'move_attrs.c';p.write_text(source.replace(old,new));obj=temp/'fault.o';exe=temp/'fault'
   command([cc,*flags,'-c',p,'-o',obj]);command([cc,*flags,GC/'tests/fusion_variable_types_host_test.c',obj,*libs,'-lm','-o',exe]);r=execute([exe]);assert r.returncode!=0,('undetected compiled fault',label)
   print('Caught compiled fault:',label,flush=True)
  data=json.loads(raw);bad=[]
  def malformed(label,edit):
   d=copy.deepcopy(data);edit(d);bad.append((label,json.dumps(d)))
  malformed('missing case',lambda d:d['rows'].pop())
  malformed('duplicate case',lambda d:d['rows'].__setitem__(1,d['rows'][0]))
  malformed('boolean version',lambda d:d.__setitem__('version',True))
  malformed('boolean species',lambda d:d['rows'][0].__setitem__('primary',True))
  malformed('wrong fusion species accessor',lambda d:d['rows'][1].__setitem__('fusionSpeciesForm',1))
  malformed('missing actual type',lambda d:d['rows'][0].pop('actual'))
  malformed('nonfinite type',lambda d:d['rows'][0].__setitem__('variable',float('nan')))
  malformed('RNG drift',lambda d:d['rows'][0].__setitem__('after','!rnd,1,0.1,0.2,0.3'))
  malformed('missing item type',lambda d:d['rows'][0].__setitem__('items',[]))
  bad.append(('duplicate key',raw.decode().replace('"version":1','"version":1,"version":1',1)))
  for label,payload in bad:
   p=temp/'bad.json';p.write_text(payload);r=execute([*builder,p,temp/'bad.inc']);assert r.returncode!=0,('accepted malformed capture',label)
   print('Rejected malformed capture:',label,flush=True)
  for key in ['variable','actual','items']:
   d=copy.deepcopy(data);r=d['rows'][2870]
   if key=='items':r[key][0]=(r[key][0]+1)%18
   else:r[key]=(r[key]+1)%18
   p=temp/'forged.json';p.write_text(json.dumps(d));forged=temp/'forged.inc';command([*builder,p,forged]);test=temp/'forged.c';test.write_text((GC/'tests/fusion_variable_types_host_test.c').read_text().replace('#include "fixtures/fusion-variable-types.native.inc"','#include "'+str(forged)+'"'));exe=temp/'forged';command([cc,*flags,test,*libs,'-lm','-o',exe]);r=execute([exe]);assert r.returncode!=0,('accepted forged output',key)
   print('Rejected forged output:',key,flush=True)
  print('Fusion variable types: 6 compiled faults, 10 malformed captures, 3 forged outputs rejected')

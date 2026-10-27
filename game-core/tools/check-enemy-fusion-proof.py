#!/usr/bin/env python3
"""Native constructor transport, actual C owners, and failure controls."""
from pathlib import Path
import copy,gzip,json,os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];GC=ROOT/'game-core'
def command(args):
 p=subprocess.run(list(map(str,args)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if p.returncode:raise RuntimeError(str(args)+'\n'+p.stdout[-4000:])
 return p.stdout
def execute(exe):return subprocess.run([str(exe)],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
flags=['-O1','-std=gnu11','-ffp-contract=off',*['-I'+str(ROOT/p)for p in ['game-core/include','game-core/src','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out','battle-core/include','battle-core/gen/out']]]
libs=[GC/p for p in ['build/libgc.a','build/reward-core/libreward.a','build/ai-core/libai.a','build/run-core/librun.a','build/run-core/battle-core/libprcore.a']]
with tempfile.TemporaryDirectory(prefix='enemy-fusion-proof-')as td:
 temp=Path(td);raw=gzip.decompress((GC/'tests/fixtures/enemy-fusion.native.json.gz').read_bytes());native=temp/'native.json';native.write_bytes(raw);inc=temp/'native.inc'
 command(['python3',GC/'tools/build-enemy-fusion-proof.py',native,inc]);assert inc.read_bytes()==(GC/'tests/fixtures/enemy-fusion.native.inc').read_bytes()
 print(command(['make','-C',GC,'enemy-fusion-host-test']),flush=True)
 cc=os.environ.get('CC','cc');sanitize=os.environ.get('ENEMY_FUSION_SANITIZE')=='1'
 if sanitize:
  san=flags+['-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'];objects=[]
  for file in ['gc_mon.c','gc_encounter.c','gc_save.c']:
   obj=temp/(file+'.o');command([cc,*san,'-c',GC/'src'/file,'-o',obj]);objects.append(obj)
  exe=temp/'san';command([cc,*san,GC/'tests/enemy_fusion_host_test.c',*objects,*libs,'-lm','-o',exe]);print(command([exe]),flush=True)
  print('Scoped ASan/UBSan/LSan: constructor, encounter and save owners; supporting archive units unsanitized')
 else:
  faults=[
   ('gc_mon.c','if (g->fusion_species > 0) {','if (false) {','omit fusion projection'),
   ('gc_mon.c','p->fusion_form = (uint8_t)g->fusion_form;','p->fusion_form = (uint8_t)g->form;','use primary form'),
   ('gc_mon.c','p->fusion_ability_index = (int8_t)g->fusion_ability_index;','p->fusion_ability_index = (int8_t)g->ability_index;','use primary ability index'),
   ('gc_mon.c','p->fusion_gender = (int8_t)g->fusion_gender;','p->fusion_gender = (int8_t)g->gender;','use primary gender'),
   ('gc_mon.c','p->fusion_gender_known = true;','p->fusion_gender_known = false;','lose known secondary gender'),
   ('gc_mon.c','p->fusion_species = (uint16_t)g->fusion_species;','p->fusion_species = (uint16_t)g->fusion_species;pr_rng_seed_int(&run->run.rng,2,0);','add constructor transport RNG draw'),
   ('gc_mon.c','init_common(m);\n    m->exp = gc_level_total_exp(level, sp ? sp->growth_rate : 0);','init_common(m);p->ability=p->base_ability=gc_form_ability(p->species,f,g->ability_index);\n    m->exp = gc_level_total_exp(level, sp ? sp->growth_rate : 0);','retain primary ability cache'),
   ('gc_encounter.c','gc_mon_new_enemy(run, m, species, level, g, slot);','gc_mon_new_enemy(run, m, species, level, g, slot);m->b.fusion_species=0;','drop fusion at refresh'),
   ('gc_encounter.c','m->b.pp_up[k] = pp_up[k];','m->b.pp_up[k] = 0;','lose constructed move PP metadata'),
  ]
  # Confirm the identical override/link path works without a fault first.
  for file in sorted({f[0]for f in faults}):
   obj=temp/'normal.o';exe=temp/'normal';command([cc,*flags,'-c',GC/'src'/file,'-o',obj]);command([cc,*flags,GC/'tests/enemy_fusion_host_test.c',obj,*libs,'-lm','-o',exe]);result=execute(exe);assert result.returncode==0,result.stdout[-4000:]
  for file,old,new,label in faults:
   source=(GC/'src'/file).read_text();assert source.count(old)==1,(label,source.count(old));path=temp/file;path.write_text(source.replace(old,new));obj=temp/'fault.o';exe=temp/'fault'
   command([cc,*flags,'-c',path,'-o',obj]);command([cc,*flags,GC/'tests/enemy_fusion_host_test.c',obj,*libs,'-lm','-o',exe]);result=execute(exe);assert result.returncode!=0,('undetected compiled fault',label)
   print('Caught compiled fault:',label,flush=True)
  data=json.loads(raw);bad=[]
  def malformed(label,edit):
   d=copy.deepcopy(data);edit(d);bad.append((label,json.dumps(d)))
  malformed('missing case',lambda d:d['rows'].pop())
  malformed('duplicate input',lambda d:d['rows'].__setitem__(1,d['rows'][0]))
  malformed('missing fusion gender',lambda d:d['rows'][39]['mon'].pop('fusionGender'))
  malformed('invalid fusion gender',lambda d:d['rows'][39]['mon'].__setitem__('fusionGender',3))
  malformed('missing RNG',lambda d:d['rows'][0].pop('after'))
  malformed('wrong charm input',lambda d:d['rows'][-1].__setitem__('charms',0))
  malformed('nonfinite stat',lambda d:d['rows'][0]['mon']['stats'].__setitem__(0,float('nan')))
  malformed('boolean version',lambda d:d.__setitem__('version',True))
  malformed('boolean mode',lambda d:d['rows'][0].__setitem__('mode',True))
  bad.append(('duplicate key',raw.decode().replace('"version":1','"version":1,"version":1',1)))
  for label,payload in bad:
   path=temp/'bad.json';path.write_text(payload);result=subprocess.run(['python3',str(GC/'tools/build-enemy-fusion-proof.py'),str(path),str(temp/'bad.inc')],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);assert result.returncode!=0,('accepted malformed capture',label)
   print('Rejected malformed capture:',label,flush=True)
  for key in ['ability','stats','name','exp']:
   d=copy.deepcopy(data);m=d['rows'][-1]['mon']
   if key=='stats':m['stats'][0]+=1;m['hp']+=1
   elif key=='name':m['name']+='!'
   else:m[key]+=1
   path=temp/'forged.json';path.write_text(json.dumps(d));forged=temp/'forged.inc';command(['python3',GC/'tools/build-enemy-fusion-proof.py',path,forged]);test=temp/'forged.c';test.write_text((GC/'tests/enemy_fusion_host_test.c').read_text().replace('#include "fixtures/enemy-fusion.native.inc"','#include "'+str(forged)+'"'));exe=temp/'forged';command([cc,*flags,test,*libs,'-lm','-o',exe]);result=execute(exe);assert result.returncode!=0,('accepted forged output',key)
   print('Rejected forged output:',key,flush=True)
  print('Enemy fusion projection: 9 compiled faults, 10 malformed captures and 4 forged outputs rejected')

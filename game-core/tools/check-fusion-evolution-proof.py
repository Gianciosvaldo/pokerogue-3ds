#!/usr/bin/env python3
"""Reject production regressions and malformed/forged native observations."""
from pathlib import Path
import copy,gzip,json,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];GC=ROOT/'game-core';BC=ROOT/'battle-core'
def run(args,good=True):
 p=subprocess.run(list(map(str,args)),cwd=GC,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if(p.returncode==0)!=good:raise RuntimeError(str(args)+'\n'+p.stdout[-5000:])
 return p.stdout
cc=os.environ.get('CC','cc');flags=['-O2','-g','-std=c99','-ffp-contract=off',*['-I'+str(ROOT/p)for p in ['game-core/include','game-core/src','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out','battle-core/include','battle-core/gen/out']]]
libs=[GC/p for p in ['build/libgc.a','build/reward-core/libreward.a','build/ai-core/libai.a','build/run-core/librun.a','build/run-core/battle-core/libprcore.a']]
with tempfile.TemporaryDirectory(prefix='fusion-evolution-proof-')as td:
 temp=Path(td);raw=gzip.decompress((GC/'tests/fixtures/fusion-evolution.native.json.gz').read_bytes());capture=temp/'native.json';capture.write_bytes(raw);inc=temp/'native.inc';run(['python3',GC/'tools/build-fusion-evolution-proof.py',capture,inc]);assert inc.read_bytes()==(GC/'tests/fixtures/fusion-evolution.native.inc').read_bytes()
 print(run(['make','-C',GC,'fusion-evolution-host-test']),flush=True)
 print(run([GC/'build/fusion_evolution_host_test']),flush=True)
 faults=[
 ('gc_mon.c','if (evo >= 0 || !m->b.fusion_species) return evo;','return evo;','omit secondary selection'),
 ('gc_mon.c','int evo = gc_mon_get_evolution(run, m);','int evo = -1;','skip primary priority'),
 ('gc_mon.c','(fusion ? p->fusion_gender : p->gender) == c->v[0]','p->gender == c->v[0]','use primary gender'),
 ('gc_mon.c','!pre_form_identity(m, e, true)','!pre_form_identity(m, e, false)','use primary form'),
 ('gc_mon.c','if (fusion) p->fusion_species = e->species;','if (fusion) p->species = e->species;','mutate primary half'),
 ('gc_mon.c','if (index == 2 && pre_count == 3 && count == 2) index = 1;',';','omit hidden ability downgrade'),
 ('gc_mon.c',': p->fusion_ability_index == 1 ? effective->ability2 : effective->ability_hidden)',': gc_form_ability(effective_species,effective,p->fusion_ability_index))','wrong fusion NONE fallback'),
 ('gc_mon.c','const int pre_species = fusion ? p->fusion_species : p->species;','const int pre_species = p->species;','lose fused tracker cleanup'),
 ('gc_phases.c','request.d = fusion;','request.d = 0;','lose queued secondary flag'),
 ('gc_phases.c','const bool fusion = p->d == 1;','const bool fusion = false;','ignore selected half on confirm'),
 ('gc_reward.c','r->fusion_gender = p->fusion_gender;','r->fusion_gender = 0;','lose reward projection gender'),
 ('gc_save.c','SF(pr_pokemon, fusion_gender_known),','','omit saved gender marker'),
 ('gc_save.c','if(known>1)return false;',';','accept malformed gender marker'),
 ('gc_save.c','return !r.bad && (count==0 || count==2);\n}\n\n/* Old Transform','return !r.bad;\n}\n\n/* Old Transform','accept partial gender schema'),
 ('gc_save.c','legacy_fusion_gender && f == pokemon_fields','false && f == pokemon_fields','misreport absent legacy metadata as dropped'),
 ]
 for n,(file,old,new,label)in enumerate(faults):
  folder=temp/f'fault-{n}';folder.mkdir();s=(GC/'src'/file).read_text();assert s.count(old)==1,(label,s.count(old));src=folder/file;src.write_text(s.replace(old,new));obj=folder/'fault.o';exe=folder/'test';run([cc,*flags,'-c',src,'-o',obj]);run([cc,*flags,GC/'tests/fusion_evolution_host_test.c',obj,*libs,'-lm','-o',exe]);run([exe],False);print('Caught compiled fault:',label,flush=True);shutil.rmtree(folder)
 j=json.loads(raw);bad=[]
 def malformed(label,edit):
  d=copy.deepcopy(j);edit(d);bad.append((label,json.dumps(d)))
 malformed('missing case',lambda d:d['rows'].pop())
 malformed('wrong cycle',lambda d:d['rows'][0].__setitem__('offset',0))
 malformed('wrong species input',lambda d:d['rows'][0]['before'].__setitem__('species',1))
 malformed('missing secondary gender',lambda d:d['rows'][0]['before'].pop('fusionGender'))
 malformed('invalid gender',lambda d:d['rows'][0]['before'].__setitem__('fusionGender',2))
 malformed('boolean gender',lambda d:d['rows'][0]['before'].__setitem__('fusionGender',False))
 malformed('missing stats',lambda d:d['rows'][0]['after']['stats'].pop())
 malformed('negative HP',lambda d:d['rows'][0]['after'].__setitem__('hp',-1))
 malformed('invalid RNG',lambda d:d['rows'][0]['before'].__setitem__('global','!rnd,-1,0,0,0'))
 malformed('boolean version',lambda d:d.__setitem__('version',True))
 malformed('missing locales',lambda d:d.pop('pokemon'))
 malformed('missing selected field',lambda d:d['rows'][1]['selected'].pop('fusion'))
 bad.append(('duplicate key',raw.decode().replace('"version":1','"version":1,"version":1',1)))
 for n,(label,payload)in enumerate(bad):
  src=temp/f'bad-{n}.json';src.write_text(payload);run(['python3',GC/'tools/build-fusion-evolution-proof.py',src,temp/'bad.inc'],False)
 for n,label in enumerate(['after HP','rendered name','battle RNG','selected pre-form']):
  d=copy.deepcopy(j)
  if n==0:d['rows'][1]['after']['hp']+=1
  elif n==1:d['rows'][1]['after']['name']+='!'
  elif n==2:
   f=d['rows'][1]['after']['battle'].split(',');f[1]=str(int(f[1])+1);d['rows'][1]['after']['battle']=','.join(f)
  else:d['rows'][1]['selected']['preForm']='roaming'
  folder=temp/f'forged-{n}';folder.mkdir();src=folder/'native.json';src.write_text(json.dumps(d));inc=folder/'fixture.inc';run(['python3',GC/'tools/build-fusion-evolution-proof.py',src,inc]);test=folder/'test.c';source=(GC/'tests/fusion_evolution_host_test.c').read_text().replace('#include "doubles_host_test.c"','#include "'+str(GC/'tests/doubles_host_test.c')+'"').replace('#include "fusion-evolution-save-guards.inc"','#include "'+str(GC/'tests/fusion-evolution-save-guards.inc')+'"').replace('#include "fixtures/fusion-evolution.native.inc"','#include "'+str(inc)+'"');test.write_text(source);exe=folder/'test';run([cc,*flags,test,*libs,'-lm','-o',exe]);run([exe],False);print('Rejected forged observation:',label,flush=True);shutil.rmtree(folder)
 print(f'Fusion evolution proof: {len(faults)} compiled faults, {len(bad)} malformed captures and 4 forged observations rejected')

#!/usr/bin/env python3
"""Check native owner observations, compiled regression faults and capture guards."""
from pathlib import Path
import copy,gzip,json,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];GC=ROOT/'game-core'
def run(args,good=True):
 p=subprocess.run(list(map(str,args)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 if (p.returncode==0)!=good:raise RuntimeError(str(args)+'\n'+p.stdout[-4000:])
 return p.stdout
cc=os.environ.get('CC','cc');flags=['-O1','-std=c99','-ffp-contract=off',*['-I'+str(ROOT/p)for p in ['game-core/include','game-core/src','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out','battle-core/include','battle-core/gen/out']]]
libs=[GC/p for p in ['build/libgc.a','build/reward-core/libreward.a','build/ai-core/libai.a','build/run-core/librun.a','build/run-core/battle-core/libprcore.a']]
source=GC/'tests/shedinja_host_test.c';fixture=GC/'tests/fixtures/shedinja.native.json.gz';builder=GC/'tools/build-shedinja-proof.py'
print(run(['make','-C',GC,'shedinja-host-test']),flush=True)
with tempfile.TemporaryDirectory(prefix='shedinja-proof-')as td:
 temp=Path(td);inc=temp/'native.inc';run(['python3',builder,fixture,inc]);assert inc.read_bytes()==(GC/'tests/fixtures/shedinja.native.inc').read_bytes()
 faults=[
 ('gc_mon.c','    special_evolution(run,m,e,fusion);','    ;','omit extra party owner'),
 ('gc_mon.c','|| !run->pokeballs[POKEBALL_TYPE_POKEBALL]','|| false','ignore normal ball gate'),
 ('gc_mon.c','const int slot=run->n_party;','run->pokeballs[POKEBALL_TYPE_POKEBALL]--;const int slot=run->n_party;','consume the ball'),
 ('gc_mon.c','if(species && species->male_percent>=0)','if(false)','skip fresh gender RNG'),
 ('gc_mon.c','if(run->run.mode==GAME_MODES_SPLICED_ENDLESS && m->b.level>1)','if(false)','skip temporary fusion RNG'),
 ('gc_mon.c','if(run->run.mode==GAME_MODES_DAILY && !gc_generate_moveset','if(false && !gc_generate_moveset','skip Daily constructor moveset'),
 ('gc_mon.c','copy->b.pp_up[i]=m->b.pp_up[i];','copy->b.pp_used[i]=m->b.pp_used[i];copy->b.pp_up[i]=m->b.pp_up[i];','copy spent PP'),
 ('gc_mon.c','if(m->used_tm_alias)tm_publish(run,m);',';','lose shared history writes'),
 ('gc_mon.c','members[i]->used_tm_alias==source->used_tm_alias','members[i]->b.id==source->b.id','key arrays by Pokemon ID'),
 ('gc_mon.c','if(gc_int(run)->training.away)','if(false)','lose retained training member'),
 ('gc_mon.c','run->sys && (run->run.mode != GAME_MODES_DAILY || m->met_biome > -1)','run->sys','record Daily rentals in profile'),
 ('gc_items.c','clone.eviolite_base=true;','clone.eviolite_base=false;','lose inherited Eviolite clone class'),
 ('gc_items.c','clone.non_transferable=gc_item_key_locked(clone.key);','clone.non_transferable=false;','lose constructor-held lock'),
 ('gc_save.c','SF(gc_mon, used_tms), SF(gc_mon, n_used_tms),','SF(gc_mon, used_tms), SF(gc_mon, n_used_tms), SF(gc_mon, used_tm_alias),','persist a live JS array alias'),
 ]
 for n,(file,old,new,label)in enumerate(faults):
  folder=temp/f'fault-{n}';folder.mkdir();s=(GC/'src'/file).read_text();assert s.count(old)==1,(label,s.count(old));src=folder/file;src.write_text(s.replace(old,new));obj=folder/'fault.o';exe=folder/'test'
  run([cc,*flags,'-c',src,'-o',obj]);run([cc,*flags,source,obj,*libs,'-lm','-o',exe]);run([exe],False);print('Caught compiled fault:',label,flush=True);shutil.rmtree(folder)
 j=json.loads(gzip.decompress(fixture.read_bytes()));bad=[]
 def malformed(label,edit):
  d=copy.deepcopy(j);edit(d);bad.append((label,d))
 malformed('missing case',lambda d:d['rows'].pop())
 malformed('wrong input offset',lambda d:d['rows'][0].__setitem__('offset',0))
 malformed('missing input species',lambda d:d['rows'][0]['before']['party'][0].pop('species'))
 malformed('negative HP',lambda d:d['rows'][0]['after']['party'][0].__setitem__('hp',-1))
 malformed('invalid RNG',lambda d:d['rows'][0]['before'].__setitem__('global','!rnd,-1,0,0,0'))
 malformed('short stats',lambda d:d['rows'][0]['after']['party'][0]['stats'].pop())
 malformed('boolean version',lambda d:d.__setitem__('version',True))
 malformed('missing held class',lambda d:d['rows'][0]['after']['stock'][0].pop('cls'))
 malformed('missing profile entry',lambda d:d['rows'][0]['after']['profile'].pop())
 for label,d in bad:
  src=temp/'bad.json.gz';src.write_bytes(gzip.compress(json.dumps(d,separators=(',',':')).encode()));run(['python3',builder,src,temp/'bad.inc'],False)
 src=temp/'bad.json.gz';src.write_bytes(gzip.compress(gzip.decompress(fixture.read_bytes()).replace(b'"version":1',b'"version":1,"version":1',1)));run(['python3',builder,src,temp/'bad.inc'],False)
 for n,label in enumerate(['HP','global RNG','profile IV','held stack']):
  d=copy.deepcopy(j);f=d['rows'][4]['after']
  if n==0:f['party'][0]['hp']-=1
  elif n==1:
   v=f['global'].split(',');v[1]=str(int(v[1])+1);f['global']=','.join(v)
  elif n==2:f['profile'][3]['dex']['ivs'][0]+=1
  else:f['stock'][0]['stack']+=1
  folder=temp/f'forged-{n}';folder.mkdir();src=folder/'native.json.gz';src.write_bytes(gzip.compress(json.dumps(d,separators=(',',':')).encode()));inc=folder/'native.inc';run(['python3',builder,src,inc]);test=folder/'test.c';test.write_text(source.read_text().replace('"../src/internal.h"',json.dumps(str(GC/'src/internal.h'))).replace('"fixtures/shedinja.native.inc"',json.dumps(str(inc))));exe=folder/'test';run([cc,*flags,test,*libs,'-lm','-o',exe]);run([exe],False);print('Rejected forged observation:',label,flush=True);shutil.rmtree(folder)
 print(f'Shedinja proof: {len(faults)} compiled faults, {len(bad)+1} malformed captures and 4 forged observations rejected')

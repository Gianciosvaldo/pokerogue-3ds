#!/usr/bin/env python3
"""Native event replay, strict capture controls, source mutations and scoped sanitizers."""
from pathlib import Path
import copy,gzip,json,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2];GC=ROOT/'game-core'
def execute(args):return subprocess.run(list(map(str,args)),cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
def run(args):
 r=execute(args)
 if r.returncode:raise RuntimeError(str(args)+'\n'+r.stdout[-4000:])
 return r.stdout
flags=['-O1','-std=c99','-ffp-contract=off',*['-I'+str(ROOT/p)for p in ['game-core/include','game-core/src','game-core/build','run-core/include','ai-core/include','reward-core/include','reward-core/gen/out','battle-core/include','battle-core/gen/out']]]
libs=[GC/p for p in ['build/libgc.a','build/reward-core/libreward.a','build/ai-core/libai.a','build/run-core/librun.a','build/run-core/battle-core/libprcore.a']]
cc=os.environ.get('CC','cc');test=GC/'tests/trash_host_test.c';builder=['python3',GC/'tools/build-trash-proof.py']
print(run(['make','-C',GC,'trash-host-test','trash-flow-test']),flush=True)
with tempfile.TemporaryDirectory(prefix='trash-proof-')as td:
 temp=Path(td);capture=temp/'native.json';raw=gzip.decompress((GC/'tests/fixtures/trash.native.json.gz').read_bytes());capture.write_bytes(raw);inc=temp/'native.inc';run([*builder,capture,inc]);assert inc.read_bytes()==(GC/'build/trash.native.inc').read_bytes()
 if os.environ.get('TRASH_SANITIZE')=='1':
  san=['-g','-fsanitize=address,undefined','-fno-sanitize-recover=all'];objects=[]
  for file in ['gc_trash.c','gc_mystery.c','gc_encounter.c']:
   obj=temp/(file+'.o');run([cc,*flags,*san,'-c',GC/'src'/file,'-o',obj]);objects.append(obj)
  exe=temp/'san';run([cc,*flags,*san,test,*objects,*libs,'-lm','-o',exe]);print(run([exe]),flush=True);print('Scoped ASan/UBSan/LSan: Trash event, dispatcher, configured enemy items and reward requests; supporting archives unsanitized');raise SystemExit(0)
 faults=[
 ('gc_mystery.c','} else if (type == GC_ME_BUG_TYPE_SUPERFAN) {','} else if (type == GC_ME_TRASH_TO_TREASURE) { s->option_ok[0]=run->n_items>=3; } else if (type == GC_ME_BUG_TYPE_SUPERFAN) {','restore wrong inventory requirement'),
 ('gc_trash.c','if(option==1) {','if(false) {','replace digging with battle'),
 ('gc_trash.c','int p=run->party_order[k];','int p=k;','ignore party ordering'),
 ('gc_trash.c','virtual_stack<gc_item_max_held','virtual_stack<=gc_item_max_held','pick full-stock recipient'),
 ('gc_trash.c','give_healing_item(run,"LEFTOVERS");give_healing_item(run,"SHELL_BELL");','gc_item_remove_at(run,0);give_healing_item(run,"LEFTOVERS");give_healing_item(run,"SHELL_BELL");','consume unrelated first item'),
 ('gc_trash.c','gc_port_give_reward(run,"MYSTERY_ENCOUNTER_BLACK_SLUDGE");','(void)0;','lose permanent Black Sludge'),
 ('gc_trash.c','gc_me_leave_with_heal_shop(run);','gc_me_leave(run);','omit post-dig heal shop'),
 ('gc_trash.c','boss->b.form=1;','boss->b.form=0;','lose Gigantamax form'),
 ('gc_trash.c','boss->b.boss_segments++;','(void)0;','lose extra boss segment'),
 ('gc_trash.c','gc_mon_set_move(boss,i,moves[i]);','gc_mon_set_move(boss,i,moves[3-i]);','reverse configured moves'),
 ('gc_trash.c','gc_me_add_start_effect(run,2,0,MOVE_ID_TOXIC);','gc_me_add_start_effect(run,2,2,MOVE_ID_TOXIC);','Toxic targets boss'),
 ('gc_trash.c','pr_rng constructor_rng=run->run.rng;','pr_rng constructor_rng=gc_me_option_rng(run);','use expired option offset after awaits'),
 ('gc_trash.c','if(found<0)items[n++]=raw[i];','if(true)items[n++]=raw[i];','keep duplicate config stocks separate'),
 ('gc_trash.c','else if(items[found].stack+raw[i].stack<=gc_item_max_held(run,raw[i].key,raw[i].arg,boss))','else if(true)','overflow configured stock caps'),
 ('gc_mystery.c','c->n_tiers=in->me_reward.n_tiers;','c->n_tiers=0;','drop victory guaranteed tiers'),
 ]
 for index,(file,old,new,label)in enumerate(faults):
  folder=temp/f'fault-{index}';folder.mkdir();source=(GC/'src'/file).read_text();assert source.count(old)==1,(label,source.count(old));p=folder/file;p.write_text(source.replace(old,new));obj=folder/'fault.o';exe=folder/'test';run([cc,*flags,'-c',p,'-o',obj]);run([cc,*flags,test,obj,*libs,'-lm','-o',exe]);r=execute([exe]);assert r.returncode!=0 and 'Trash case'in r.stdout,('surviving '+label,r.returncode,r.stdout[-1000:]);print('Caught compiled fault:',label,flush=True);shutil.rmtree(folder)
 data=json.loads(raw)
 malformed=[
 ('missing case',lambda d:d['rows'].pop()),('duplicate case',lambda d:d['rows'].__setitem__(-1,copy.deepcopy(d['rows'][-2]))),('boolean index',lambda d:d['rows'][0].__setitem__('index',False)),('wrong order',lambda d:d['rows'][1].__setitem__('order',[0,1,2])),('invalid RNG',lambda d:d['rows'][0].__setitem__('after','!rnd,1,nan,0,0')),('integer eligibility',lambda d:d['rows'][0]['requirements'].__setitem__(0,1)),('missing config',lambda d:d['rows'][0]['config']['items'].pop()),('invalid IV',lambda d:d['rows'][0]['enemy']['ivs'].__setitem__(0,32)),('invalid item',lambda d:d['rows'][0]['enemyItems'][0].__setitem__('key','INVENTED')),('wrong rewards',lambda d:d['rows'][0]['reward']['tiers'].reverse()),('missing leave owner',lambda d:d['rows'][32].__setitem__('leave',[])),('extra root field',lambda d:d.__setitem__('unexpected',1)),
 ]
 for label,edit in malformed:
  d=copy.deepcopy(data);edit(d);capture.write_text(json.dumps(d));r=execute([*builder,capture,inc]);assert r.returncode!=0,'accepted malformed '+label;print('Rejected malformed:',label,flush=True)
 forged=[('boss IV',lambda d:d['rows'][0]['enemy']['ivs'].__setitem__(0,(d['rows'][0]['enemy']['ivs'][0]+1)%32)),('boss HP',lambda d:d['rows'][0]['enemy'].__setitem__('hp',d['rows'][0]['enemy']['hp']+1)),('ambient RNG',lambda d:d['rows'][0].__setitem__('after','!rnd,1,0.1,0.2,0.3')),('reward recipient',lambda d:d['rows'][32]['output'][0].__setitem__('owner',103)),('reward message',lambda d:d['rows'][32]['texts'].__setitem__(0,'Changed reward text'))]
 harness=test.read_text().replace('#include "../src/internal.h"','#include "internal.h"').replace('#include "trash.native.inc"','#include "'+str(inc)+'"')
 for label,edit in forged:
  d=copy.deepcopy(data);edit(d);capture.write_text(json.dumps(d));run([*builder,capture,inc]);p=temp/'forged.c';p.write_text(harness);exe=temp/'forged';run([cc,*flags,p,*libs,'-lm','-o',exe]);r=execute([exe]);assert r.returncode!=0 and 'Trash case'in r.stdout,('surviving forged '+label,r.stdout[-1000:]);print('Rejected forged:',label,flush=True);exe.unlink()
 print(f'Trash proof: {len(faults)} compiled faults, {len(malformed)} malformed captures, {len(forged)} forged observations rejected')

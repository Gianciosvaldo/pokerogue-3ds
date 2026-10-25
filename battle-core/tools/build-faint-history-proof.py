#!/usr/bin/env python3
from pathlib import Path
import json,itertools,sys,math
j=json.loads(Path(sys.argv[1]).read_text());rows=j['rows'];assert j['version']==1 and len(rows)==72
assert [(r['side'],r['pattern'],r['length']) for r in rows]==list(itertools.product([0,1],range(4),[0,1,11,12,13,14,32,100,512]))
assert j['waveSeed']=='faint-history-native' and type(j['maxExpLevel'])==int and j['maxExpLevel']>0
lines=['/* Actual FaintPhase logs and caller observations from the pin. */',f"static const int faint_max_exp_level={j['maxExpLevel']};",'static const char faint_wave_seed[]="faint-history-native";','static const struct{int side,pattern,length;struct{int index,turn;}events[512],tail[12];int nt,counts[2],power[4][2],tera[4][6];}faint_cases[72]={']
def rng(state):
 fields=state.split(',');assert len(fields)==5 and fields[0]=='!rnd'
 values=[float(x)for x in fields[1:]];assert all(math.isfinite(x)for x in values) and values[0]>=0 and all(0<=x<1 for x in values[1:])
 return '{'+','.join(fields[1:])+',0}'
lines.insert(3,'static const pr_rng faint_global_rng='+rng(j['globalRng'])+';')
lines.insert(4,'static const pr_rng faint_battle_rng='+rng(j['battleRng'])+';')
for r in rows:
 n=r['length'];side=r['side'];assert len(r['events'])==n and len(r['observations'])==4
 assert all(type(r[k])==int for k in ['side','pattern','length'])
 assert all(type(e['index'])==int and 0<=e['index']<6 and type(e['turn'])==int and e['turn']==i+1 for i,e in enumerate(r['events']))
 assert r['playerFaints']==len(r['logs'][0]) and r['enemyFaints']==len(r['logs'][1])
 assert len(r['logs'][side])==n and not r['logs'][1-side]
 assert [(x['index'],x['turn']) for x in r['logs'][side]]==[(side*6+x['index'],x['turn']) for x in r['events']]
 assert [o['delta'] for o in r['observations']]==[-1,0,1,2]
 def pairs(a):return '{'+','.join('{'+str(x['index'])+','+str(x['turn'])+'}'for x in a)+'}'
 powers=[];teras=[]
 for o in r['observations']:
  assert len(o['power'])==2 and all(type(x) in [float,int] and x in [1,2] for x in o['power'])
  assert len(o['tera'])==6 and all(type(x)==bool for x in o['tera'])
  powers.append('{'+','.join(str(int(x))for x in o['power'])+'}');teras.append('{'+','.join(str(int(x))for x in o['tera'])+'}')
 lines.append('{'+','.join(map(str,[side,r['pattern'],n]))+','+pairs(r['events'])+','+pairs(r['logs'][side][-12:])+','+str(min(n,12))+',{'+str(r['playerFaints'])+','+str(r['enemyFaints'])+'},{'+','.join(powers)+'},{'+','.join(teras)+'}},')
lines.append('};');Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')

#!/usr/bin/env python3
"""Keep fresh native configuration observations and the C replay projection aligned."""
import argparse,json,pathlib
p=argparse.ArgumentParser();p.add_argument('capture',type=pathlib.Path);p.add_argument('--write',action='store_true');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[1];frozen=root/'tests/fixtures/challengers.native.json';inc=root/'tests/challengers.native.inc'
rows=json.loads(a.capture.read_text())
if not isinstance(rows,list) or len(rows)!=320:raise SystemExit('expected all 320 native records')
arr=lambda xs:'{'+','.join(map(str,xs))+'}'
strings=lambda xs:'{'+','.join(json.dumps(x) for x in xs)+'}'
key=lambda x:'NULL' if x is None else json.dumps(x)
out=['/* Actual pinned encounter/configuration and Trainer constructor observations. */','typedef struct {int wave,biome,option,types[3],variants[3],n_levels,levels[8],name,partner;const char*name_key,*partner_key;int tpl_size,tpl_strength,members[8][4];const char*sprites[3];int n_funcs;const char*funcs[3];int n_tiers,tiers[4];bool fill;} challenger_case;','static const challenger_case challenger_native[]={']
for index,c in enumerate(rows):
 if c['index']!=index or c['option']!=index%3 or len(c['configs'])!=3:raise SystemExit('case identity/cardinality')
 cfg=c['configs'];o=c['constructed'];t=o['template'];r=c['rewards'];n=len(o['levels'])
 if not 1<=n<=6 or len(t['members'])!=n or any(len(x)!=4 for x in t['members']):raise SystemExit('template shape')
 if len(r['funcs'])>3 or len(r['tiers'])>4 or type(r['fill']) is not bool:raise SystemExit('reward shape')
 members='{'+','.join(arr(x) for x in t['members'])+'}'
 out.append('{'+','.join([str(c['wave']),str(c['biome']),str(c['option']),arr(x['type'] for x in cfg),arr(2 if x['doubleOnly'] else 1 if x['female'] else 0 for x in cfg),str(n),arr(o['levels']),str(o['nameIndex']),str(o['partnerIndex']),key(o['name']),key(o['partner']),str(t['size']),str(t['strength']),members,strings(x['sprite'] for x in cfg),str(len(r['funcs'])),strings(r['funcs']),str(len(r['tiers'])),arr(r['tiers']),str(r['fill']).lower()])+'},')
out.append('};');text='\n'.join(out)+'\n'
if a.write:frozen.write_bytes(a.capture.read_bytes());inc.write_text(text)
elif frozen.read_bytes()!=a.capture.read_bytes() or inc.read_text()!=text:raise SystemExit('native capture or C projection differs from frozen observations')
print('Mysterious Challengers: complete 320-record native capture and C projection match')

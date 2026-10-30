#!/usr/bin/env python3
"""Validate a fresh pinned native capture and compare its frozen C observations."""
import argparse,itertools,json,math,pathlib
p=argparse.ArgumentParser();p.add_argument('capture',type=pathlib.Path);p.add_argument('--write',action='store_true');a=p.parse_args()
rows=json.loads(a.capture.read_text());want=set(itertools.product(range(8),range(3),[-1,0],[-1,0],range(2)))
if not isinstance(rows,list) or len(rows)!=192:raise SystemExit('expected all 192 records')
seen=set();out=['/* Observed pinned PokemonMultiHitModifier.add/apply; see multi-fallback.producer.test.ts. */','static const struct {int context,stack,old_arg,incoming_arg,storage,ok,n_lenses,output_stack,hits;double factors[3];} fallback_native[]={']
for r in rows:
 if set(r)!={'context','stack','oldArg','incomingArg','storage','ok','lenses','hits','factors'}:raise SystemExit('record keys')
 key=tuple(r[k] for k in ['context','stack','oldArg','incomingArg','storage'])
 if any(type(n) is not int for n in key) or key not in want or key in seen:raise SystemExit('missing/duplicate/out-of-domain case')
 seen.add(key)
 if type(r['ok']) is not bool or type(r['hits']) is not int or not 1<=r['hits']<=3:raise SystemExit('result type/range')
 if not isinstance(r['lenses'],list) or len(r['lenses'])!=1 or any(type(n) is not int or not 1<=n<=2 for n in r['lenses']):raise SystemExit('lens observations')
 if len(r['factors'])!=r['hits'] or any(type(n) not in (int,float) or not math.isfinite(n) for n in r['factors']):raise SystemExit('strike observations')
 out.append('{'+','.join(map(str,[*key,int(r['ok']),len(r['lenses']),r['lenses'][0],r['hits']]))+',{'+','.join(map(str,r['factors']))+'}},')
out.append('};');output='\n'.join(out)+'\n';dest=pathlib.Path(__file__).resolve().parents[1]/'tests/multi-fallback.native.inc'
if a.write:dest.write_text(output)
elif dest.read_text()!=output:raise SystemExit('fresh native observations differ from frozen fixture')
print('Multi fallback: complete 192-record native capture matches C fixture')

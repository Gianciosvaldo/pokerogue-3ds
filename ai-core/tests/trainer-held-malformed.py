# Run against a complete 2000-case Card 108 trace; malformed input must exit 2.
from pathlib import Path
import subprocess,json,sys
exe=sys.argv[1]; trace=Path(sys.argv[2]); root=Path(sys.argv[3]); root.mkdir(parents=True,exist_ok=True); s=trace.read_text()
def remove(prefix):
 lines=s.splitlines(True); i=next(i for i,l in enumerate(lines) if l.startswith(prefix));return ''.join(lines[:i]+lines[i+1:])
def duplicate(prefix):
 lines=s.splitlines(True); i=next(i for i,l in enumerate(lines) if l.startswith(prefix));return ''.join(lines[:i]+[lines[i]]+lines[i:])
fixtures={'footer_missing':s[:s.rindex('# AI108_COMPLETE')], 'footer_wrong':s.replace('# AI108_COMPLETE 2000 0','# AI108_COMPLETE 1999 1'), 'header_wrong':s.replace('# AI108_CONTRACT 1 2000','# AI108_CONTRACT 2 2000'), 'trailing':s+'trailing\n','missing_rng':remove('RNG '),'duplicate_rng':duplicate('RNG '),'missing_aimon':remove('AIMON '),'duplicate_aimon':duplicate('AIMON '),'missing_damage':remove('PROBE '),'duplicate_damage':duplicate('PROBE '),'missing_after':remove('AFTER '),'unknown_record':s.replace('PROFILE kind=0 state=0','BOGUS x=0\nPROFILE kind=0 state=0',1),'invalid_profile':s.replace('PROFILE kind=0 state=0','PROFILE kind=8 state=0',1),'nonfinite_hp':s.replace(' hp=', ' hp=nan',1),'nonfinite_probe':s.replace('PROBE user=','PROBE user=nan',1),'missing_end':remove('END '),'duplicate_case':s.replace('CASE 1\n','CASE 0\n',1),'wrong_modifier':s.replace('cls=AttackTypeBoosterModifier','cls=UnknownHeldItem',1),'virtual_modifier':s.replace('virtual=0 type=','virtual=1 type=',1),'missing_party':remove('PARTY '),'missing_next':remove('NEXT '),'aimon_owner_duplicate':s.replace('AIMON b=2 ', 'AIMON b=0 ',1),'applied_owner_duplicate':s.replace('APPLIED b=2 ', 'APPLIED b=0 ',1),'missing_queue':remove('QUEUE ')}
results={}
for name,data in fixtures.items():
 p=root/f'malformed-{name}.txt';p.write_text(data);r=subprocess.run([exe,str(p),'2000'],capture_output=True,text=True);results[name]=r.returncode
 if r.returncode!=2: raise RuntimeError((name,r.returncode,r.stdout,r.stderr))
r=subprocess.run([exe,str(trace),'1999'],capture_output=True,text=True);results['request_mismatch']=r.returncode
assert r.returncode==2
(root/'malformed-results.json').write_text(json.dumps(results,indent=2)+'\n');print(len(results),'malformed/request guards exit2')

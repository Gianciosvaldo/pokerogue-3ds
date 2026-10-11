from pathlib import Path
import sys,subprocess,tempfile
exe,trace=sys.argv[1:];s=Path(trace).read_text();lines=s.splitlines(True)
if subprocess.run([exe,trace],capture_output=True).returncode:raise SystemExit('intact helper must pass')
variants={'missing-footer':''.join(lines[:-1]),'fractional-count':s.replace('HELPERS 1 9','HELPERS 1 9.5',1),'duplicate-case':s.replace('H 1 ','H 0 ',1),'trailing':s+'H 9\n','missing-case':''.join(lines[:1]+lines[2:]),'duplicate-record':''.join(lines[:1]+[lines[1]]+lines[1:])}
for name,k,v in [('invalid-max',2,'nan'),('invalid-hp',3,'-1'),('invalid-after',10,'99999')]:
 a=lines[1].split();a[k]=v;variants[name]=''.join(lines[:1]+[' '.join(a)+'\n']+lines[2:])
with tempfile.TemporaryDirectory() as d:
 for name,content in variants.items():
  p=Path(d)/name;p.write_text(content);r=subprocess.run([exe,str(p)],capture_output=True);print(name,r.returncode)
  if r.returncode!=2:raise SystemExit('expected2')
print(len(variants),'helper malformed refused')

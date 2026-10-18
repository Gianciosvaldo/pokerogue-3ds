from pathlib import Path
import subprocess,os,sys
root=Path(sys.argv[1]).resolve();out=Path(sys.argv[2]).resolve();trans=Path(sys.argv[3]).resolve();env=os.environ.copy();env['TURNTEST_LOCALES']=sys.argv[4]
def capture(folder,kind):
 p=folder/'callback-777.txt'
 return p if p.exists() else folder/f'{kind}-callback-777.txt'
def runner(kind):return root/'build'/f'{kind}-callback-test'
src=(root/'src/ab_attrs.c').read_text()
for name,a,b,test,trace in [
 ('no-fusion-gate','if (p->fusion_species && pr_ability_first_attr','if (0 && p->fusion_species && pr_ability_first_attr','fusion-callback-test.c',capture(out,'fusion')),
 ('no-transform-gate','if (p->transformed && pr_ability_first_attr','if (0 && p->transformed && pr_ability_first_attr','transform-callback-test.c',capture(trans,'transform'))]:
 assert a in src;f=out/f'mutant-{name}.c';f.write_text(src.replace(a,b,1));exe=out/f'mutant-{name}'
 subprocess.run(['gcc','-O2','-std=c99','-I'+str(root/'include'),'-I'+str(root/'gen/out'),str(root/'tests'/test),str(f),str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True,capture_output=True)
 r=subprocess.run([str(exe),str(trace),'--phase'],env=env,capture_output=True,text=True);(out/f'mutant-{name}.log').write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode,r.stdout,r.stderr);print(name,'caught',flush=True)
for kind,folder in [('fusion',out),('transform',trans)]:
 p=capture(folder,kind);s=p.read_text();count=int(s.splitlines()[0].split()[-1]);rows=s.splitlines(True);x=next(i for i,l in enumerate(rows)if l.startswith('X '));c=next(i for i,l in enumerate(rows)if l.startswith('C '))
 tests={'truncated':''.join(rows[:-1]),'footer-count':s.replace(f'COMPLETE {count}',f'COMPLETE {count-1}'),'extra':s+'extra\n','bad-species':s.replace(rows[x],rows[x].replace(rows[x].split()[1],'65535',1),1),'bad-form':s.replace(rows[x],'X '+rows[x].split()[1]+' 255 '+' '.join(rows[x].split()[3:])+'\n',1),'fraction':s.replace(rows[x],rows[x].replace('X ','X 1.5',1),1),'wrong-index':s.replace(rows[c],rows[c].replace('C 0 ','C 1 ',1),1)}
 for name,text in tests.items():
  f=out/f'malformed-{kind}-{name}.txt';f.write_text(text);r=subprocess.run([str(runner(kind)),str(f),'--phase'],env=env,capture_output=True,text=True);assert r.returncode==2,(kind,name,r.returncode);print(kind,name,'rejected',flush=True)
print('ALL callback guards passed')

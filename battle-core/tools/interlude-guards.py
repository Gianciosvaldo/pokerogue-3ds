from pathlib import Path
import subprocess,os,sys
root=Path(sys.argv[1]).resolve();out=Path(sys.argv[2]).resolve();env=os.environ.copy();env['TURNTEST_LOCALES']=sys.argv[3];source=(root/'src/phases.c').read_text()
def capture(name):
 p=out/name
 return p if p.exists() else out/({'api-expanded-777.txt':'interlude-777.txt','end-expanded-777.txt':'interlude-end-777.txt'}[name])
faults=[
 ('keep-berry','PH_WEATHER_EFFECT, PH_BERRY, PH_CHECK_STATUS_EFFECT','PH_WEATHER_EFFECT, PH_CHECK_STATUS_EFFECT','interlude-test','api-expanded-777.txt'),
 ('remove-first-only','while (pr_phase_try_remove(bt, e, removed[i], -1)) {}','pr_phase_try_remove(bt, e, removed[i], -1);','interlude-test','api-expanded-777.txt'),
 ('wrong-turn-end-level','for (int l = e->n_levels - 1; l >= 0; l--)','for (int l = 0; l < e->n_levels; l++)','interlude-test','api-expanded-777.txt'),
 ('ignore-reserves','for (int k = 0; k < bt->n_party[1]; k++)\n        if (party_hp(bt, 1, k) > 0) return;','for (int k = 0; k < bt->n_party[1] && k < 2; k++)\n        if (party_hp(bt, 1, k) > 0) return;','interlude-test','api-expanded-777.txt'),
 ('keep-pokemon-tail','if (!PH(ph)->upcoming_interlude) {','if (1) {','interlude-end-test','end-expanded-777.txt'),
 ('skip-weather-lapse','if (bt->weather != WEATHER_TYPE_NONE && !weather_lapse(bt)) {','if (!PH(ph)->upcoming_interlude && bt->weather != WEATHER_TYPE_NONE && !weather_lapse(bt)) {','interlude-end-test','end-expanded-777.txt'),
 ('skip-terrain-lapse','if (bt->terrain != TERRAIN_NONE && !terrain_lapse(bt))','if (!PH(ph)->upcoming_interlude && bt->terrain != TERRAIN_NONE && !terrain_lapse(bt))','interlude-end-test','end-expanded-777.txt')]
for name,a,b,test,trace in faults:
 assert a in source,name
 f=out/f'mutant-{name}.c';f.write_text(source.replace(a,b,1));exe=out/f'mutant-{name}'
 subprocess.run(['gcc','-O2','-std=c99','-I'+str(root/'include'),'-I'+str(root/'gen/out'),str(root/'tests'/f'{test}.c'),str(f),str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True,capture_output=True)
 r=subprocess.run([str(exe),str(capture(trace))],env=env,capture_output=True,text=True);(out/f'mutant-{name}.log').write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode,r.stdout,r.stderr);print(name,'caught',flush=True)
s=capture('api-expanded-777.txt').read_text();count=int(s.splitlines()[0].split()[2]);rows=s.splitlines(True);c=next(i for i,l in enumerate(rows)if l.startswith('C '));p=next(i for i,l in enumerate(rows)if l.startswith('P '));l=next(i for i,l in enumerate(rows)if l.startswith('L '))
malformed={'truncated':''.join(rows[:-1]),'wrong-version':s.replace('INTERLUDE 1 ','INTERLUDE 2 ',1),'wrong-footer':s.replace(f'COMPLETE {count}',f'COMPLETE {count-1}'),'extra':s+'extra\n','wrong-case':s.replace(rows[c],rows[c].replace('C 0 ','C 1 ',1),1),'fraction':s.replace(rows[c],rows[c].replace('C 0 ','C 0.5 ',1),1),'wrong-party':s.replace(rows[c],'C 0 9 7\n',1),'wrong-phase':s.replace(rows[p],rows[p].replace('P 0 ','P 1 ',1),1),'wrong-kind':s.replace(rows[p],'P 0 8 0\n',1),'wrong-level':s.replace(rows[l],rows[l].replace('L 0 ','L 1 ',1),1)}
for name,text in malformed.items():
 f=out/f'malformed-{name}.txt';f.write_text(text);r=subprocess.run([str(root/'build/interlude-test'),str(f)],env=env,capture_output=True,text=True);assert r.returncode==2,(name,r.returncode);print('malformed',name,'rejected',flush=True)
print('ALL interlude guards passed')

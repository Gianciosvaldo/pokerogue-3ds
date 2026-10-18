from pathlib import Path
import subprocess,os,sys
root=Path(sys.argv[1]).resolve();out=Path(sys.argv[2]).resolve();src=(root/'src/text.c').read_text();env=os.environ.copy();env['TURNTEST_LOCALES']=sys.argv[3]
def capture(name):
 aliases={'names-full-2034.txt':'fusion-name-2034.txt','messages-full-777.txt':'fusion-message-777.txt','heal-2034.txt':'fusion-heal-2034.txt','wish-4242.txt':'fusion-wish-4242.txt'}
 p=out/name
 return p if p.exists() else out/aliases[name]
faults=[('no-vowel-run','if (a) while (*end','if (0) while (*end'),('wrong-match-slice','sb + 2, eb','eb - 1, eb'),('no-rapid','fa.n == 4 &&','fa.n == 255 &&'),('prefix-b','ap.n ? &ap : &bp','bp.n ? &bp : &ap'),('suffix-a','bs.n ? &bs : &as','as.n ? &as : &bs'),('no-repeat-drop','if (i && i < fb.n)','if (0 && i && i < fb.n)'),('no-fusion-message','if (fusion_v >> 16)','if (0 && fusion_v >> 16)')]
for name,a,b in faults:
 assert a in src
 f=out/f'mutant-{name}.c';f.write_text(src.replace(a,b,1));exe=out/f'mutant-{name}'
 test='fusion-message-test.c' if name=='no-fusion-message' else 'fusion-name-test.c'
 subprocess.run(['gcc','-O2','-std=c99','-I'+str(root/'include'),'-I'+str(root/'gen/out'),str(root/'tests'/test),str(f),str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True,capture_output=True)
 trace=capture('messages-full-777.txt' if name=='no-fusion-message' else 'names-full-2034.txt')
 r=subprocess.run([str(exe),str(trace)],env=env,capture_output=True,text=True);(out/f'mutant-{name}.log').write_text(r.stdout+r.stderr)
 assert r.returncode==1,(name,r.returncode,r.stdout,r.stderr)
 print(name,'caught',flush=True)
base=capture('names-full-2034.txt').read_text();lines=base.splitlines(True)
malformed={'truncated':''.join(lines[:-1]),'wrong-version':base.replace('FUSENAME 1 ','FUSENAME 9 ',1),'count':base.replace('FUSENAME_COMPLETE 16384','FUSENAME_COMPLETE 16383'),'extra':base+'extra\n','bad-hex':base.replace(lines[1],lines[1].replace('C 0 ','C 0 z',1),1),'duplicate':base.replace(lines[2],lines[1],1),'odd-hex':base.replace(lines[1],lines[1].replace('C 0 ','C 0 0',1),1)}
for name,text in malformed.items():
 p=out/f'malformed-{name}.txt';p.write_text(text);r=subprocess.run([str(root/'build/fusion-name-test'),str(p)],capture_output=True,text=True);assert r.returncode==2,(name,r.returncode)
 print('malformed',name,'rejected',flush=True)
print('ALL guards passed')
for name,file,a,b,test,trace in [
 ('no-snapshot-fusion','events.c','if (species) a.fusion_v','if (0 && species) a.fusion_v','fusion-message-test.c','messages-full-777.txt'),
 ('no-stored-fusion','arena_tags.c','restore_pp,name.fusion_v','restore_pp,0','pending-heal-test.c','heal-2034.txt'),
 ('no-wish-fusion','move_effects.c','.fusion_name = pr_arg_pokemon(bt, "", user).fusion_v','.fusion_name = 0','fusion-wish-test.c','wish-4242.txt'),
 ('no-wish-delivery-fusion','phases.c','t->pokemon_name, NULL, t->fusion_name','t->pokemon_name, NULL, 0','fusion-wish-test.c','wish-4242.txt')]:
 source=(root/'src'/file).read_text();assert a in source
 f=out/f'mutant-{name}.c';f.write_text(source.replace(a,b,1));exe=out/f'mutant-{name}'
 subprocess.run(['gcc','-O2','-std=c99','-I'+str(root/'include'),'-I'+str(root/'gen/out'),str(root/'tests'/test),str(f),str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True,capture_output=True)
 r=subprocess.run([str(exe),str(capture(trace))],env=env,capture_output=True,text=True);(out/f'mutant-{name}.log').write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode,r.stdout,r.stderr);print(name,'caught',flush=True)
# Strict identity admission checks, not uint16/uint8 truncation.
base=capture('messages-full-777.txt').read_text();lines=base.splitlines(True);parts=lines[1].split()
for name,index,value in [('bad-species',2,'65535'),('bad-form',3,'255'),('bad-player',6,'2'),('bad-wave',8,'201'),('fraction',2,'1.5'),('bad-illusion',9,'2')]:
 row=parts.copy();row[index]=value;p=out/f'malformed-msg-{name}.txt';p.write_text(base.replace(lines[1],' '.join(row)+'\n',1));r=subprocess.run([str(root/'build/fusion-message-test'),str(p)],env=env,capture_output=True,text=True);assert r.returncode==2,(name,r.returncode);print('malformed message',name,'rejected',flush=True)
print('ALL extended guards passed')

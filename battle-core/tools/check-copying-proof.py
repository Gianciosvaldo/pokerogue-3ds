#!/usr/bin/env python3
"""Actual compiled faults and complete-fixture transport checks for copying."""
from pathlib import Path
import argparse,os,re,subprocess
parser=argparse.ArgumentParser();parser.add_argument('api',type=Path);parser.add_argument('turn',type=Path);parser.add_argument('--transition',type=Path);args=parser.parse_args()
root=Path(__file__).resolve().parents[1];out=root/'build/copying-guards';out.mkdir(parents=True,exist_ok=True);env=os.environ.copy()
if not env.get('TURNTEST_LOCALES'):raise SystemExit('TURNTEST_LOCALES required')
mutations={
 'allow-sketch-banlist':('closures_conditions.c',' || last->move==MOVE_ID_STRUGGLE || last->move==MOVE_ID_SKETCH','',False),
 'allow-known-sketch':('closures_conditions.c','if(PR_MOVES(u)[i]==last->move)return 0;','if(false && PR_MOVES(u)[i]==last->move)return 0;',False),
 'sketch-only-permanent':('pokemon.c','    if (PR_HAS_SUMMON_MOVES(p)) {','    if (false && PR_HAS_SUMMON_MOVES(p)) {',False),
 'mimic-alias-copied-slot':('pokemon.c','user->summon_moves_alias_mask=aliases & (uint8_t)~(1u<<slot);','user->summon_moves_alias_mask=aliases;',False),
 'mimic-loses-other-aliases':('pokemon.c','user->summon_moves_alias_mask=aliases & (uint8_t)~(1u<<slot);','user->summon_moves_alias_mask=0;',False),
 'source-learning-keeps-alias':('pokemon.c','    p->summon_moves_alias_mask &= (uint8_t)~(1u << slot);','    p->summon_moves_alias_mask=p->summon_moves_alias_mask;',False),
 'missing-mimic-ref-detach':('pokemon.c','if (PR_HAS_SUMMON_MOVES(user)) detach_move_refs(e,user,slot,false,true);','if (false && PR_HAS_SUMMON_MOVES(user)) detach_move_refs(e,user,slot,false,true);',False),
 'copied-pp-not-shared':('pokemon.c','if(p->summon_moves_alias_mask & (1u<<slot)) {','if(false && p->summon_moves_alias_mask & (1u<<slot)) {',True),
 'sketch-caller-missing':('move_effects.c','    case PR_AC_SketchAttr:','    case PR_AC_SketchAttr: return;',True),
 'mimic-caller-missing':('phases.c','            pr_copy_last_move(bt,e,user,first,move,false);','            (void)first;',True),
}
if args.transition:
 mutations['transform-keeps-mimic-alias']=('phases.c','user->summon_moves_active=true;user->summon_moves_alias_mask=0;','user->summon_moves_active=true;',True)
for name,(file,old,new,turn) in mutations.items():
 s=(root/'src'/file).read_text();assert s.count(old)==1,(name,s.count(old));source=out/(name+'.c');obj=out/(name+'.o');exe=out/name;source.write_text(s.replace(old,new))
 include=['-I'+str(root/'include'),'-I'+str(root/'gen/out'),'-I'+str(root/'tests'),'-I'+str(root/'build')]
 subprocess.run(['gcc','-O2','-std=c99','-ffp-contract=off',*include,'-c',str(source),'-o',str(obj)],check=True)
 test=root/'tests'/('copying-turn-test.c' if turn else 'copying-test.c')
 subprocess.run(['gcc','-O2','-std=c99',*include,str(test),str(obj),str(root/'build/libprcore.a'),'-lm','-o',str(exe)],check=True)
 fixture=args.transition if name=='transform-keeps-mimic-alias' else args.turn if turn else args.api;r=subprocess.run([str(exe),str(fixture.resolve())],env=env,capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr);assert r.returncode==1,(name,r.returncode);print(name,'caught',flush=True)
rows=args.api.read_text().splitlines();bad={'missing-footer':rows[:-1],'extra-tail':rows+['JUNK'],'bad-version':['COPYING 2 4000 777']+rows[1:],'wrong-footer':rows[:-1]+['COPYING_COMPLETE 1']}
ci=next(i for i,x in enumerate(rows)if x.startswith('C '));ii=next(i for i,x in enumerate(rows)if x.startswith('I '));ti=next(i for i,x in enumerate(rows)if x.startswith('T '))
for name,index,col,value in [('bad-kind',ci,2,'6'),('bad-slot',ci,3,'4'),('bad-amount',ci,4,'9'),('species-overflow',ii,1,'4294967297'),('bad-form',ii,2,'255'),('bad-temp-flag',ii,4,'2'),('bad-alias-mask',ii,5,'16'),('bad-count',ii,6,'5'),('bad-pp-up',ii,15,'4'),('odd-text',ti,1,'a'),('bad-text',ti,1,'zz')]:
 r=rows.copy();v=r[index].split();v[col]=value;r[index]=' '.join(v);bad[name]=r
for name,rows in bad.items():
 f=out/(name+'.txt');f.write_text('\n'.join(rows)+'\n');r=subprocess.run([str(root/'build/copying-test'),str(f)],env=env,capture_output=True,text=True);assert r.returncode==2,(name,r.returncode,r.stdout,r.stderr);print(name,'rejected',flush=True)
turn_rows=args.turn.read_text().splitlines()
in_input=False
for mi,x in enumerate(turn_rows):
 if x.startswith('CASE '):in_input=True
 elif x=='RUN':in_input=False
 elif in_input and x.startswith('MON ') and ' sm=1 ' in x:break
else:raise AssertionError('no input temporary moveset')
for name,key,value in [('flag-overflow','sm','4294967297'),('alias-overflow','malias','4294967296'),('bad-alias','malias','16')]:
 r=turn_rows.copy();r[mi]=re.sub(r'\b'+key+r'=\S+',key+'='+value,r[mi]);bad_file=out/('turn-'+name+'.txt');bad_file.write_text('\n'.join(r)+'\n')
 result=subprocess.run([str(root/'build/copying-turn-test'),str(bad_file)],env=env,capture_output=True,text=True)
 assert result.returncode==2,(name,result.returncode,result.stdout,result.stderr);print('turn-'+name,'rejected',flush=True)
for name,col,value in [('bad-move',0,'4294967297'),('fractional-used',1,'0.5'),('overflow-used',1,'256'),('bad-up',2,'4'),('bad-max',3,'32768')]:
 r=turn_rows.copy();moves=re.search(r'\bsmmoves=(\S+)',r[mi])[1].split(',');parts=moves[0].split(':');parts[col]=value;moves[0]=':'.join(parts)
 r[mi]=re.sub(r'\bsmmoves=\S+','smmoves='+','.join(moves),r[mi]);bad_file=out/('turn-'+name+'.txt');bad_file.write_text('\n'.join(r)+'\n')
 result=subprocess.run([str(root/'build/copying-turn-test'),str(bad_file)],env=env,capture_output=True,text=True)
 assert result.returncode==2,(name,result.returncode,result.stdout,result.stderr);print('turn-'+name,'rejected',flush=True)
print(f'{len(mutations)} compiled runtime faults caught; 15 malformed API and 8 malformed turn fixtures rejected',flush=True)

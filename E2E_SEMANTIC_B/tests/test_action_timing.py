from pathlib import Path
import subprocess
R=Path(__file__).resolve().parent.parent
EXE=str(R/'build/quectoc-ast')
def run(source, mode='--trace-semantic'):
    return subprocess.run([EXE,mode],input=source.encode(),capture_output=True,timeout=10)
p=run('let x=2+3*4; print(x);')
assert p.returncode==0,p.stderr
lines=p.stderr.decode().splitlines()
typed=[x for x in lines if ' -> Int ' in x]
assert [x.split()[1] for x in typed]==['Integer','Integer','Integer','Binary','Binary','Identifier'],typed
assert lines.index(next(x for x in lines if 'declare x' in x))>lines.index(typed[4])
p=run('let x=0; while(x<3){let x=x+1; print(x);} print(x);')
assert p.returncode==0,p.stderr
lines=p.stderr.decode().splitlines()
assert sum('enter S1' in x for x in lines)==1
assert not any('enter S2' in x for x in lines)
assert sum('checked While' in x for x in lines)==1
assert any('lookup x in S1' in x and '(S0)' in x for x in lines)
assert any('lookup x in S1' in x and '(S1)' in x for x in lines)
assert lines.index('SEM_TRACE leave S1')<lines.index(next(x for x in lines if 'checked While' in x))
for source in ['{{print( ;','{{print(@);','{{let x=1;']:
    p=run(source)
    assert p.returncode==1 and not p.stdout,(source,p.stderr)
    lines=p.stderr.decode().splitlines()
    assert lines.index('SEM_TRACE leave S2')<lines.index('SEM_TRACE leave S1')<lines.index('SEM_TRACE leave S0')
p=run('x=y;', '--semantic')
assert p.returncode==1 and p.stderr==b'SEMANTIC_UNDECLARED y\nSEMANTIC_UNDECLARED x\n'
p=run('print(missing); let x=;', '--semantic')
assert p.returncode==1 and not p.stdout
assert p.stderr.startswith(b'SEMANTIC_UNDECLARED missing\nPARSER_ERROR ')
p=run('let x=1;{}{}print(x);')
assert p.returncode==0 and b'enter S1' in p.stderr and b'enter S2' in p.stderr
p=run('print(1/0);','--semantic')
assert p.returncode==0 # Checking types does not evaluate division.
p=run('int x=1;','--semantic')
assert p.returncode==1 and b'PARSER_ERROR' in p.stderr
print('PASS 10 action timing, scope, diagnostic and language checks')

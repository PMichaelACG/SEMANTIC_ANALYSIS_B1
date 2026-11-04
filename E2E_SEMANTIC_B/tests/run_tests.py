#!/usr/bin/env python3
"""Independent acceptance and AST-shape expectations. Node IDs are ignored."""
from pathlib import Path
import json, re, subprocess, sys, xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parent.parent
EXE=str(ROOT/'build/quectoc-ast')
phase=json.loads((ROOT/'tests/phase.json').read_text())['phase']
def run(text=None,args=()):
    return subprocess.run([EXE,*args],input=None if text is None else text.encode(),capture_output=True,timeout=10)
def tree(dot):
    labels={}; children={}; incoming={}
    for line in dot.splitlines():
        node=re.fullmatch(r'  (n\d+) \[label=("(?:[^"\\]|\\.)*")\];',line)
        edge=re.fullmatch(r'  (n\d+) -> (n\d+) \[label=("(?:[^"\\]|\\.)*")\];',line)
        if node:
            key,label=node.groups(); assert key not in labels
            labels[key]=json.loads(label); children.setdefault(key,[])
        if edge:
            a,b,role=edge.groups();assert b not in incoming
            incoming[b]=a;children.setdefault(a,[]).append((json.loads(role),b))
    roots=set(labels)-set(incoming); assert len(roots)==1
    assert set(incoming)<=set(labels)
    seen=set()
    def visit(key):
        assert key not in seen;seen.add(key)
        return [labels[key],[[role,visit(child)] for role,child in children[key]]]
    result=visit(roots.pop());assert len(seen)==len(labels)
    return result
def check(condition,message):
    if not condition: raise AssertionError(message)

cases=json.loads((ROOT/'tests/syntax_cases.json').read_text())
for c in cases:
    p=run(c['input'])
    check(p.returncode==c['status'],(c['id'],p.returncode,p.stderr))
    check(p.stderr.decode()==c['stderr'],(c['id'],'diagnostic',p.stderr,c['stderr']))
    if p.returncode==0:check(tree(p.stdout.decode())[0]=='Program',c['id'])
    else:check(p.stdout==b'',(c['id'],'partial graph'))
print(f'PASS {len(cases)} syntax regression cases')

cases=json.loads((ROOT/'tests/trees.json').read_text())
for c in cases:
    p=run(c['input']);check(p.returncode==0 and not p.stderr,(c['id'],p.stderr))
    got=tree(p.stdout.decode());check(got==c['tree'],(c['id'],got,c['tree']))
print(f'PASS {len(cases)} independent AST shape cases')

# Stdin and explicit-file routes must agree exactly.
sample=ROOT/'examples/sample.qc'
p=run(sample.read_text());q=run(args=(str(sample),));s=run(sample.read_text(),args=('-',))
check((p.returncode,p.stdout,p.stderr)==(q.returncode,q.stdout,q.stderr)==(s.returncode,s.stdout,s.stderr),'input routes')
check(run(args=('a','b')).returncode==2,'usage status')
check(run(args=(str(ROOT/'missing-file.qc'),)).returncode==2,'missing file')
if Path('/dev/full').exists():
    with open('/dev/full','wb') as output:
        p=subprocess.run([EXE,str(sample)],stdout=output,stderr=subprocess.PIPE,timeout=10)
    check(p.returncode==2 and b'DOT output failed' in p.stderr,'output error')
if phase!='Baseline':
    p=run('print("a\x00b");')
    check(p.returncode==1 and not p.stdout and b'NUL in string' in p.stderr,'NUL rejection')
    # Large source order and duplicate spellings must not merge occurrences.
    p=run('print(x);'*200)
    t=tree(p.stdout.decode());check(len(t[1])==200,'statement order stress')
print('PASS command line, I/O and boundary checks')

rendered=0
for sample in sorted((ROOT/'examples').glob('*.qc')):
    if sample.name=='invalid.qc':continue
    p=run(args=(str(sample),));check(p.returncode==0,sample.name)
    svg=subprocess.run(['dot','-Tsvg'],input=p.stdout,capture_output=True,timeout=20)
    check(svg.returncode==0,svg.stderr)
    ET.fromstring(svg.stdout);rendered+=1
# The support test produces adversarial labels. They must remain literal text.
svg=subprocess.run(['dot','-Tsvg',str(ROOT/'build/escaping.dot')],capture_output=True,timeout=20)
check(svg.returncode==0,svg.stderr)
doc=ET.fromstring(svg.stdout)
groups=[g for g in doc.iter() if g.attrib.get('class')=='node']
check(len(groups)==3,'escaped label became an extra graph node')
texts=[''.join(t.itertext()) for t in doc.iter() if t.tag.endswith('}text')]
check(any('\\N' in t and 'evil' in t for t in texts),'literal backslash substitution')
print(f'PASS {rendered+1} Graphviz SVG renders including label escaping')

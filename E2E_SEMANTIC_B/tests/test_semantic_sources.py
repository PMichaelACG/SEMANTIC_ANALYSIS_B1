from pathlib import Path
import json, subprocess, re
R=Path(__file__).resolve().parent.parent
phase=json.loads((R/'tests/semantic_cases.json').read_text())
cmd=[str(R/'build/quectoc-ast'),'--semantic']
for c in phase['cases']:
    p=subprocess.run(cmd,input=c['input'].encode(),capture_output=True,timeout=10)
    assert p.returncode==c['status'],(c['id'],p.returncode,p.stderr)
    if c['diagnostic']: assert p.stderr.decode().strip()==c['diagnostic'],(c['id'],p.stderr)
    if c['status']==0:
        assert not p.stderr,(c['id'],p.stderr)
        if True:
            assert b'Type: Int' in p.stdout or b'Type: String' in p.stdout or c['id']=='empty_block'
            raw=subprocess.run([str(R/'build/quectoc-ast')],input=c['input'].encode(),capture_output=True,timeout=10)
            def tree_edges(x): return re.findall(rb'  n\d+ -> n\d+ \[label="[^"\n]*"\];',x)
            assert tree_edges(raw.stdout)==tree_edges(p.stdout),(c['id'],'tree changed')
            svg=subprocess.run(['dot','-Tsvg'],input=p.stdout,capture_output=True,timeout=20)
            assert svg.returncode==0,(c['id'],svg.stderr)
    else: assert not p.stdout,(c['id'],'output after failure')
print('PASS',len(phase['cases']),'semantic source cases')

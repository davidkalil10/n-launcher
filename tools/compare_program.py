"""Compare the C++ preparer with upstream's NSO images. Read-only; no uploads."""
import argparse,hashlib,pathlib,struct,subprocess,sys
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--probe',type=pathlib.Path,required=True)
parser.add_argument('--input',type=pathlib.Path,required=True)
parser.add_argument('--upstream',type=pathlib.Path,required=True)
parser.add_argument('--exefs',type=pathlib.Path,required=True,help='ExeFS cache produced by the PC builder for the same program version')
args=parser.parse_args()
sys.path.insert(0,str(args.upstream/'tools'))
from nso import Nso
result=subprocess.run([str(args.probe.resolve()),str(args.input),'--prepare'],check=True,capture_output=True,text=True)
count=0
for line in result.stdout.splitlines():
    if not line.startswith('module='):continue
    actual=dict(word.split('=',1) for word in line.split())
    name=actual['module'];module=Nso(str(args.exefs/name));image=module.image()
    svcs=sum(1 for at in range(0,len(module.text)-3,4) if struct.unpack_from('<I',module.text,at)[0]&0xffe0001f==0xd4000001)
    expected={'image_bytes':len(image),'end':module.end,'mod0':module.mod0,'dynamic':module.dynamic,'object':module.module_object,'svcs':svcs,'image_sha256':hashlib.sha256(image).hexdigest()}
    for key,value in expected.items():
        if actual.get(key)!=str(value):raise SystemExit(f'MISMATCH {name} {key}: C++={actual.get(key)} upstream={value}')
    print(f'PASS {name}: image SHA-256, offsets, BSS end and SVC count match upstream');count+=1
if not count:raise SystemExit('No prepared modules to compare')
print(f'PASS {count} modules. This does not validate game execution or select matching cache versions automatically.')

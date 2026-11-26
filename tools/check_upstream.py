"""Read-only public upstream comparison. Pins/sources are never replaced or merged automatically."""
import argparse,json,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--component',choices=['all','flow','relink'],default='all');args=p.parse_args()
for component,config,name,paths in [('flow','flow.json','nx2-flow.git',['loader/runtime','menu/source']),('relink','upstream.json','nx2-relink.git',['runtime','tools/relink.py','tools/nso.py','tools/nup.py'])]:
    if args.component not in ['all',component]:continue
    pin=json.loads((root/'config'/config).read_text(encoding='utf-8'));cache=root/'vendor'/name;cache.parent.mkdir(exist_ok=True)
    if not cache.exists():subprocess.run(['git','clone','--bare',pin['repository'],str(cache)],check=True)
    else:subprocess.run(['git','--git-dir',str(cache),'fetch','origin','+refs/heads/main:refs/heads/main','--tags'],check=True)
    print(f'\n{component}: reviewed baseline in config/{config}',flush=True)
    subprocess.run(['git','--git-dir',str(cache),'show','-s','--format=PINNED %H %s',pin['commit']],check=True)
    subprocess.run(['git','--git-dir',str(cache),'log','--oneline',pin['commit']+'..refs/heads/main','--',*paths],check=True)
print('Review changes and patch compatibility manually. No source files or version pins were changed.')

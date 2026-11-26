"""Read-only upstream comparison; never overwrites a working tree or auto-merges."""
import json,pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[1]
pin=json.loads((root/'config/upstream.json').read_text())
cache=root/'vendor/nx2-relink.git'
cache.parent.mkdir(exist_ok=True)
if not cache.exists():subprocess.run(['git','clone','--bare',pin['repository'],str(cache)],check=True)
else:subprocess.run(['git','--git-dir',str(cache),'fetch','origin','+refs/heads/main:refs/heads/main','--tags'],check=True)
subprocess.run(['git','--git-dir',str(cache),'show','-s','--format=PINNED %H %s',pin['commit']],check=True)
subprocess.run(['git','--git-dir',str(cache),'log','--oneline',pin['commit']+'..refs/heads/main','--','runtime','tools/relink.py','tools/nso.py','tools/nup.py'],check=True)
print('Review changes manually. No source files or version pins were changed.')

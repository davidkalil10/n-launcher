#!/usr/bin/env python3
"""Create the public updater feed and SD zip from verified build outputs only."""
from pathlib import Path
import hashlib,json,re,struct,zipfile
ROOT=Path(__file__).resolve().parents[1]

def package():
    version=re.search(r'#define NX2LINK_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"',(ROOT/'include/nx2link/version.hpp').read_text(encoding='utf-8')).group(1)
    out=ROOT/'dist';nro=out/'n-launcher.nro';data=nro.read_bytes()
    if len(data)<4096 or len(data)>(128<<20):raise ValueError('NRO exceeds updater size limit')
    if data[16:20]!=b'NRO0':raise ValueError('Invalid built NRO')
    image=struct.unpack_from('<I',data,24)[0];nacp,size=struct.unpack_from('<QQ',data,image+24);meta=data[image+nacp:image+nacp+size]
    if len(meta)!=0x4000 or meta[:512].split(b'\0')[0]!=b'N Launcher' or meta[0x3060:0x3070].split(b'\0')[0].decode()!=version:raise ValueError('Release version/title mismatch')
    sha=hashlib.sha256(data).hexdigest()
    (out/'update.ini').write_text(f'schema=1\nversion={version}\nasset=n-launcher.nro\nsize={len(data)}\nsha256={sha}\n',encoding='ascii',newline='\n')
    readme=f'''N Launcher {version}

Extraia a pasta switch no SD e abra N Launcher pelo hbmenu com kaiHEN ativo.
Arquivo: /switch/n-launcher/n-launcher.nro

Dados antigos em /switch/nx2-link sao movidos para n-launcher sem copiar os arquivos grandes.
Nao apague saves nem o cache para atualizar. Conflitos sao preservados e informados.
R: atualizacoes pelo GitHub. Y: opcoes do titulo, incluindo Memoria por jogo.
Padrao de memoria: Automatico. Jogos, cache e saves ficam separados do arquivo atualizado.

Fonte e creditos: https://github.com/davidkalil10/n-launcher/tree/v{version}
Pokopia splitado validado no console: imagem, audio, controles, salvar e carregar.
Outros jogos e a atualizacao via TLS ainda exigem validacao especifica no hardware.
'''
    archive=out/f'N-Launcher-v{version}-Switch2.zip'
    licenses={'LICENSE':ROOT/'LICENSE','menu-GPL-2.0-or-later.txt':ROOT/'third_party/nx2-flow/menu/LICENSE','loader-MIT.txt':ROOT/'third_party/nx2-flow/loader/LICENSE','NotoSans-OFL.txt':ROOT/'third_party/noto-sans/OFL.txt','Zstandard-BSD.txt':ROOT/'third_party/zstd/LICENSE'}
    def add(z,name,body):
        info=zipfile.ZipInfo(name,(2026,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;z.writestr(info,body)
    with zipfile.ZipFile(archive,'w') as z:
        add(z,'switch/n-launcher/n-launcher.nro',data)
        add(z,'N-Launcher-LEIA.txt',readme.encode('utf-8'))
        for name,path in licenses.items():add(z,'N-Launcher-LICENSES/'+name,path.read_bytes())
    names=[nro.name,'update.ini',archive.name]
    (out/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256((out/name).read_bytes()).hexdigest()+'  '+name+'\n' for name in names),encoding='ascii',newline='\n')
    (out/'release.json').write_text(json.dumps({'version':version,'tag':'v'+version,'asset':nro.name,'bytes':len(data),'sha256':sha,'zip':archive.name},indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'version':version,'nro':nro.name,'sha256':sha,'zip':archive.name}))
if __name__=='__main__':package()

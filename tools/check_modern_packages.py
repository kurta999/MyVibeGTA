"""Check current original asset ZIP and copied DX11 game ZIP, including CRCs."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile

ROOT=Path(__file__).resolve().parents[1]


def check(game):
    source=ROOT/'dist/MiniCity3D-original-modern-assets-20260928.zip'
    expected_binary=hashlib.sha256((ROOT/'build-msvc-ninja/MiniCity3D.exe').read_bytes()).hexdigest()
    probe=json.loads((ROOT/'assets/lighting/showcase.json').read_text())
    assert probe['capture_executable_sha256']==expected_binary
    report={'schema':1,'executable_sha256':expected_binary,'archives':[]}
    for path,is_game in ((game,True),(source,False)):
        with zipfile.ZipFile(path) as archive:
            assert archive.testzip() is None
            files=archive.namelist()
            modern=[f for f in files if '/baked/modern/' in f]
            assert sum(f.endswith('.dds') for f in modern)==54
            assert sum(f.endswith('.m3d') for f in modern)==28
            if is_game:
                assert not any('/source/' in f for f in files)
                assert not any(f.endswith('.png') for f in modern)
                executable=next(f for f in files if f.endswith('/MiniCity3D.exe'))
                assert hashlib.sha256(archive.read(executable)).hexdigest()==expected_binary
                for name in ('showcase.mcpb','showcase.json'):
                    member=next(f for f in files if f.endswith('/lighting/'+name))
                    assert archive.read(member)==(ROOT/'assets/lighting'/name).read_bytes()
            else:
                assert sum('/source/modern/' in f and f.endswith('.glb') for f in files)==14
                assert sum(f.endswith('.png') for f in modern)==54
                for name in ('build_modern_assets.py','build_modern_materials.py','cook_modern_textures.py','bootstrap_texconv.ps1'):
                    assert 'tools/'+name in files
            # All authored DDS payloads must match the verified workspace cook.
            for name in modern:
                if name.endswith('.dds'):
                    assert archive.read(name)==(ROOT/'assets/models/baked/modern'/Path(name).name).read_bytes()
            report['archives'].append({'file':path.name,'bytes':path.stat().st_size,'entries':len(files),
                'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'crc':'passed','modern_dds':54,
                'modern_meshes':28,'editable_sources':0 if is_game else 14})
    return report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game',type=Path,default=ROOT/'dist/MiniCity3D-modern-2k-20260928.zip')
    parser.add_argument('--output',type=Path,default=ROOT/'evidence/textures-20260928/package-checks.json')
    args=parser.parse_args();report=check(args.game)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

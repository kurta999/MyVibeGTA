"""Independent math expectations and decoded DDS quality against authored mips."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from cook_modern_textures import mipmaps, TEXCONV, FOLDER


def math_checks():
    base=np.full((2,2,4),255,dtype=np.uint8);base[0,:,:3]=0
    normal=np.full_like(base,255);normal[:,:,:3]=(128,128,255)
    orm=np.full_like(base,255);orm[:,:,1]=64
    chains=mipmaps(base,normal,orm)
    assert np.max(np.abs(chains[0][1][0,0,:3].astype(int)-188))<=1, 'mips are not linear-light averages'
    assert chains[2][1][0,0,1]==64 and chains[2][1][0,0,3]==255
    # Opposing tilted normals average to +Z, with coherence sqrt(0.5).
    normal[0,:,:3]=(218,128,218);normal[1,:,:3]=(37,128,218)
    chains=mipmaps(base,normal,orm)
    assert abs(int(chains[2][1][0,0,3])-181)<=1
    assert np.max(np.abs(chains[1][1][0,0,:3].astype(int)-[128,128,255]))<=1


def read_rgba(path):
    data=path.read_bytes();h=struct.unpack_from('<37I',data)
    assert h[32] in (28,29)
    result=[];offset=148;w,y=h[4],h[3]
    for _ in range(h[7]):
        size=w*y*4
        result.append(np.frombuffer(data[offset:offset+size],np.uint8).reshape(y,w,4))
        offset+=size;w=max(1,w//2);y=max(1,y//2)
    assert offset==len(data)
    return result


def check_quality(names):
    math_checks();report={'schema':1,'math_checks':'passed','materials':[]}
    with tempfile.TemporaryDirectory(prefix='texture-check-',dir=ROOT/'build-tools') as directory:
        temp=Path(directory)
        for name in names:
            maps=[np.asarray(Image.open(FOLDER/f'{name}-{c}.png').convert('RGBA')) for c in ('base','normal','orm')]
            if name in ('glass','car_glass'):
                assert np.array_equal(maps[1],np.broadcast_to(np.array([128,128,255,255],np.uint8),maps[1].shape)), 'glass must have an optically smooth normal field'
            expected=mipmaps(*maps);entry={'name':name,'maps':{}}
            for channel,reference in zip(('base','normal','orm'),expected):
                path=FOLDER/f'{name}-{channel}.dds'
                command=[str(TEXCONV),'-nologo','-y','-dx10','-m','12','-f','R8G8B8A8_UNORM','-o',str(temp)]
                if channel=='base':command+=['-srgb']
                command+=[str(path)]
                subprocess.run(command,check=True,stdout=subprocess.DEVNULL)
                decoded=read_rgba(temp/path.name);assert len(decoded)==12
                errors=[];angles=[];coherence=[]
                for wanted,actual in zip(reference,decoded):
                    if channel=='normal':
                        v=actual[:,:,:2].astype(np.float32)/127.5-1
                        xyz=np.concatenate((v,np.sqrt(np.maximum(0,1-(v*v).sum(axis=2)))[:,:,None]),axis=2)
                        xyz/=np.maximum(np.linalg.norm(xyz,axis=2,keepdims=True),1e-8)
                        target=wanted[:,:,:3].astype(np.float32)/127.5-1
                        target/=np.linalg.norm(target,axis=2,keepdims=True)
                        angle=np.degrees(np.arccos(np.clip((xyz*target).sum(axis=2),-1,1)))
                        angles.append(float(np.percentile(angle,99)))
                    else:
                        error=actual.astype(np.float32)-wanted.astype(np.float32)
                        errors.append(float(np.sqrt(np.mean(error[:,:,:3]**2))))
                        if channel=='orm':coherence.append(float(np.max(np.abs(error[:,:,3]))))
                metrics={'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'mips':len(decoded)}
                if angles:
                    metrics['worst_mip_p99_normal_degrees']=max(angles)
                    assert max(angles)<2,(name,metrics)
                else:
                    metrics['worst_mip_rgb_rmse_8bit']=max(errors)
                    assert max(errors)<3,(name,channel,metrics)
                if coherence:
                    metrics['worst_coherence_error_8bit']=max(coherence)
                    assert max(coherence)<=3,(name,metrics)
                entry['maps'][channel]=metrics
            report['materials'].append(entry);print(f'Decoded all mips: {name}',flush=True)
    return report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'evidence/textures-20260928/quality.json')
    parser.add_argument('names',nargs='*')
    parser.add_argument('--merge',action='store_true',help='Retain previous unchanged materials, verifying their DDS hashes')
    args=parser.parse_args()
    names=args.names or [m['name'] for m in json.loads((FOLDER/'materials.json').read_text())['materials']]
    result=check_quality(names);args.output.parent.mkdir(parents=True,exist_ok=True)
    if args.merge:
        previous=json.loads(args.output.read_text())
        changed=set(names)
        result['materials']+= [m for m in previous['materials'] if m['name'] not in changed]
        for material in result['materials']:
            for channel,item in material['maps'].items():
                assert hashlib.sha256((FOLDER/f"{material['name']}-{channel}.dds").read_bytes()).hexdigest()==item['sha256']
        result['materials'].sort(key=lambda m:m['name'])
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print('Linear colour, normal coherence and decoded texture quality checks passed')

"""Fetch authored vehicle sources and keep verifiable licenses and checksums."""
import concurrent.futures, hashlib, json, pathlib, sys
from fetch_animal_sources import state, get
ROOT=pathlib.Path(__file__).resolve().parents[1]
OUT=ROOT/'assets/models/source/expansion'
MODELS={'mazda':'SnIoWlh7S2','range-rover':'8zk4o6nALW','motorcycle':'1yfyze7uGxS',
        'bicycle':'eRg_VrQlvXY','skateboard':'3C0mzQB3obs'}
def fetch(pair):
    key,identifier=pair
    page='https://poly.pizza/m/'+identifier
    m=state(page)['initialData']['model']
    assert m['Licence']=='CC-BY 3.0',m['Licence']
    url='https://static.poly.pizza/'+m['ResourceID']+'.glb'
    path=OUT/(key+'.glb');content=path.read_bytes() if path.exists() else get(url)
    path.write_bytes(content)
    item=dict(id=key,title=m['Title'],author=m['Creator']['Username'],license=m['Licence'],
        license_url='https://creativecommons.org/licenses/by/3.0/',page=page,download=url,
        triangles=m.get('Tris'),sha256=hashlib.sha256(content).hexdigest())
    print(json.dumps(item),flush=True)
    return item
if __name__=='__main__':
    with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
        result=list(pool.map(fetch,MODELS.items()))
    (OUT/'poly-manifest.json').write_text(json.dumps(result,indent=2)+'\n')

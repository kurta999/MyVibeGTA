"""Fetch the attributed helicopter source, retaining metadata and source hash."""
import json, hashlib, pathlib
from fetch_animal_sources import state, get
root=pathlib.Path(__file__).resolve().parents[1]
out=root/'assets/models/source/helicopter';out.mkdir(parents=True,exist_ok=True)
model=state('https://poly.pizza/m/cTzINMr0WdS')['initialData']['model']
assert model['Licence']=='CC-BY 3.0'
url='https://static.poly.pizza/'+model['ResourceID']+'.glb'
b=get(url)
assert hashlib.sha256(b).hexdigest()=='98ed6f66bef79b5b3791908236f971da7d7d0cde7f4f60d5b2e858a677d6fb09', 'Source hash changed'
(out/'helicopter.glb').write_bytes(b)
item=dict(title=model['Title'],author=model['Creator']['Username'],license=model['Licence'],page='https://poly.pizza/m/cTzINMr0WdS',download=url,sha256=hashlib.sha256(b).hexdigest())
(out/'manifest.json').write_text(json.dumps(item,indent=2)+'\n')
(out/'CC-BY-3.0.txt').write_bytes((root/'assets/models/source/animals/CC-BY-3.0.txt').read_bytes())
print(item)

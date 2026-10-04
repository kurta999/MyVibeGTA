"""Record hashes/credits for retained vehicle and audio imports."""
from pathlib import Path
import json,hashlib
root=Path(__file__).resolve().parents[1]
out=root/'assets/models/source/expansion'
records=json.loads((out/'poly-manifest.json').read_text())
extra=[
('bicycle','Bike','Poly by Google','CC BY 3.0','https://poly.pizza/m/eRg_VrQlvXY','bicycle.glb'),
('skateboard','Complete Skateboard','Thomas vaniseghem (Superthomyboy)','CC BY 3.0','https://poly.pizza/m/3C0mzQB3obs','skateboard.glb'),
('tractor','Tractor','JordanGrant3D','CC0 1.0','https://jordangrant3d.itch.io/tractor','tractor.blend'),
('combine','CombineHarvester v3','printable_models','Personal Use','https://free3d.com/3d-model/combineharvester-v3--793584.html','combine-candidate.zip'),
('tank','Abrams tank','Sketlux; original by yd','CC0 1.0','https://opengameart.org/content/abrams-tank','tank.blend'),
('truck','Truck Peterbilt 389','luke1985','CC BY 4.0','https://opengameart.org/content/truck-peterbilt-389','truck.blend'),
('airplane','Cesna Airplane','wobba89','CC BY 3.0','https://opengameart.org/content/cesna-airplane','cessna.blend'),
('trailer','Lowpoly Semi Truck (trailer only)','Craig Snedeker','CC BY-NC-SA 4.0','https://craigsnedeker.itch.io/lowpoly-semi-truck','trailer-source.zip'),
]
for key,title,author,license,page,file in extra:
    if any(a['id']==key for a in records):continue
    records.append(dict(id=key,title=title,author=author,license=license,page=page,
        source=file,sha256=hashlib.sha256((out/file).read_bytes()).hexdigest()))
(out/'manifest.json').write_text(json.dumps(dict(date='2026-10-04',use='personal/private, confirmed by user',
    modifications='axis/scale conversion, evaluated modifiers, rigid grouping; combine paint; original geometry retained',assets=records),indent=2)+'\n')
audio=root/'assets/audio/source'
items=json.loads((audio/'download-manifest.json').read_text())
authors={'heavy-engine':'Nayckron; original recording by qubodup','helicopter':'aquinn','airplane':'jakobthiesen; loop edit by AntumDeluge','car':'qubodup'}
for item in items:
    item['author']=authors[item['id']];item['license']='CC0 1.0' if item['id']=='helicopter' else 'CC BY 3.0'
if not any(i['id']=='car' for i in items):
    source=audio/'car-engine.7z'
    items.append(dict(id='car',author='qubodup',license='CC BY 3.0',page='https://opengameart.org/content/car-engine-loop-96khz-4s',
        download='https://opengameart.org/sites/default/files/engine-loop.7z',sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
(audio/'download-manifest.json').write_text(json.dumps(items,indent=2)+'\n')
for base in (root,root/'build-msvc-ninja'):
    manifest=base/'assets/models/baked/modern/manifest.json'
    if manifest.exists():
        data=json.loads(manifest.read_text())
        data['assets']=[a for a in data['assets'] if 'aurora-' not in a.get('name','')]
        manifest.write_text(json.dumps(data,indent=2)+'\n')
print('Recorded retained sources, author credits and checksums.')

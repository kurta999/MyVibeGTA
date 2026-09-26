"""Discover/download attributed Poly Pizza animals. Network only with --fetch."""
import concurrent.futures
import hashlib
import json
import pathlib
import re
import sys
import time
import urllib.parse
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/models/source/animals'

def get(url):
    for attempt in range(3):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers={'User-Agent': 'MiniCity3D asset importer'}), timeout=30) as response:
                return response.read()
        except (OSError, TimeoutError):
            if attempt==2: raise
            time.sleep(1)

def state(url):
    html = get(url).decode()
    return json.JSONDecoder().raw_decode(html.split('window.__SERVER_APP_STATE__ =')[1].lstrip())[0]

def discover(query):
    result = state('https://poly.pizza/search/' + urllib.parse.quote(query))
    found = []
    def visit(value):
        if isinstance(value, dict):
            if 'publicID' in value and 'title' in value:
                found.append({k:value.get(k) for k in ('title','publicID','creator','licence')})
            else:
                for v in value.values(): visit(v)
        elif isinstance(value,list):
            for v in value: visit(v)
    visit(result)
    return query, found[:18]

if __name__ == '__main__':
    OUT.mkdir(parents=True, exist_ok=True)
    if '--fetch' not in sys.argv:
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            for query, entries in pool.map(discover, sys.argv[1:]):
                print(query, json.dumps(entries))
    else:
        manifest = json.loads((OUT/'manifest.json').read_text())
        for item in manifest:
            model = state(item['page'])['initialData']['model']
            assert model['Licence'] == 'CC-BY 3.0', model
            item.update(author=model['Creator']['Username'], license=model['Licence'],
                        license_url='https://creativecommons.org/licenses/by/3.0/',
                        title=model['Title'], download='https://static.poly.pizza/'+model['ResourceID']+'.glb')
            path=OUT/(item['id']+'.glb')
            content=path.read_bytes() if path.exists() else get(item['download'])
            digest=hashlib.sha256(content).hexdigest()
            if item.get('sha256') and digest!=item['sha256']:
                raise ValueError('Source changed: '+item['id'])
            if not path.exists(): path.write_bytes(content)
            item['sha256']=digest
            print(item['id'], path.stat().st_size)
        (OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')

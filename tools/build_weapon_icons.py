"""Crop the generated inventory atlas into game sprites; preserve original RGBA pixels."""
from pathlib import Path
import json
import shutil
import sys
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'assets/icons/weapons'
IDS = ['pistol', 'silenced-pistol', 'smg', 'shotgun', 'rifle', 'sniper',
       'rpg', 'flamethrower', 'fire-extinguisher', 'water-cannon', 'katana',
       'knife', 'machete', 'novelty-toy', 'rolling-pin', 'bat', 'bow',
       'minigun', 'shovel', 'grapple-hook']

def build(source):
    DEST.mkdir(parents=True, exist_ok=True)
    atlas = Image.open(source).convert('RGBA')
    assert atlas.size == (1402, 1122), 'Crop coordinates belong to the reviewed original atlas'
    if source.resolve() != (DEST / 'source-atlas.png').resolve():
        shutil.copyfile(source, DEST / 'source-atlas.png')
    # Reviewed alpha-component bounds: the generated grid has slight offsets,
    # so equal-cell cropping would include neighboring handles or stocks.
    bounds = [(31,100,259,262), (295,109,561,237), (581,97,826,253),
              (844,128,1111,211), (1124,115,1389,242), (14,390,280,482),
              (289,392,563,479), (577,374,853,525), (918,309,1061,549),
              (1124,369,1385,513), (16,667,285,722), (294,663,558,738),
              (570,653,843,726), (915,591,1099,809), (1124,667,1386,722),
              (15,911,282,1011), (290,827,508,1059), (566,879,836,1026),
              (853,922,1111,1001), (1122,897,1387,1031)]
    records = []
    for i, name in enumerate(IDS):
        x0,y0,x1,y1 = bounds[i]
        crop = (x0-3,y0-3,x1+3,y1+3)
        sprite = atlas.crop(crop)
        padded = Image.new('RGBA', (sprite.width+24, sprite.height+24))
        padded.paste(sprite, (12, 12))
        padded.save(DEST / (name+'.png'))
        records.append({'id': name, 'width': padded.width, 'height': padded.height,
                        'crop': list(crop)})
    (DEST / 'manifest.json').write_text(json.dumps({'mode': 'built-in image_gen',
        'source': 'source-atlas.png', 'icons': records}, indent=2)+'\n')
    print(f'Built {len(records)} RGBA weapon icons in {DEST}')

if __name__ == '__main__':
    build(Path(sys.argv[1]))

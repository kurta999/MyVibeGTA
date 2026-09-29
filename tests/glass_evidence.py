"""Guard the observed 8-bit normal quantization grain in the facade highlight."""
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageChops, ImageFilter, ImageStat

ROOT=Path(__file__).resolve().parents[1]
FOLDER=ROOT/'evidence/textures-20260928'
paths=[FOLDER/'lighting-before-glass-fix/facade.png',FOLDER/'glass-fix-after.png']
box=(905,520,1000,570)
values=[]
for path in paths:
    image=Image.open(path).convert('RGB').crop(box)
    residual=ImageChops.difference(image,image.filter(ImageFilter.GaussianBlur(1)))
    values.append(sum(ImageStat.Stat(residual).mean)/3)
assert values[0]>1 and values[1]<.25,(values,'grainy glass highlight returned')
log=(FOLDER/'glass-fix-after.txt').read_text()
assert log.count('12 mips: passed')==54
report={'schema':1,'crop':box,'high_frequency_residual_before':round(values[0],4),
        'high_frequency_residual_after':round(values[1],4),
        'images':[{'file':str(p.relative_to(FOLDER)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths],
        'scope':'Fixed direct-sun glass highlight; semantic flat-normal map checks also run in modern_texture_quality.py'}
(FOLDER/'glass-checks.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))

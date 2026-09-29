"""Contact sheet of actual authored maps: colour crop, normal crop, roughness."""
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
FOLDER=ROOT/'assets/models/baked/modern'
OUT=ROOT/'evidence/textures-20260928/materials.jpg'
materials=json.loads((FOLDER/'materials.json').read_text())['materials']
sheet=Image.new('RGB',(1440,1320),(24,29,35));draw=ImageDraw.Draw(sheet)
font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',18)
title=ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf',25)
draw.text((24,15),'Original 2K city materials — actual map crops',font=title,fill=(238,242,246))
draw.text((24,53),'Colour (512px crop) / tangent normal / roughness factor × map',font=font,fill=(182,195,207))
for i,material in enumerate(materials):
    x=(i%6)*240+12;y=(i//6)*408+100;name=material['name']
    draw.text((x,y),name.replace('_',' '),font=font,fill=(238,242,246))
    base=Image.open(FOLDER/f'{name}-base.png').convert('RGB').crop((512,512,1024,1024))
    sheet.paste(base.resize((216,216),Image.Resampling.LANCZOS),(x,y+30))
    normal=Image.open(FOLDER/f'{name}-normal.png').convert('RGB').crop((512,512,1024,1024))
    sheet.paste(normal.resize((102,102),Image.Resampling.LANCZOS),(x,y+259))
    rough=Image.open(FOLDER/f'{name}-orm.png').getchannel('G').crop((512,512,1024,1024))
    factor=material['roughness_factor'];rough=rough.point(lambda p:round(p*factor)).convert('RGB')
    sheet.paste(rough.resize((102,102),Image.Resampling.LANCZOS),(x+114,y+259))
OUT.parent.mkdir(parents=True,exist_ok=True);sheet.save(OUT,quality=94)
print(OUT)

"""Compose the actual GLB studio renders into a labelled review sheet."""
from pathlib import Path
import argparse
from PIL import Image,ImageDraw,ImageFont

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--folder',type=Path,default=ROOT/'evidence/modern-assets-20260928')
folder=parser.parse_args().folder
names=("coastal-office","terrace-apartments",
       "compact-pistol","carbine","street-lamp","twin-lamp","bench","bin",
       "bollard","bike-rack","planter","hydrant")
sheet=Image.new("RGB",(1800,1615),(21,26,31));draw=ImageDraw.Draw(sheet)
font=ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf",23)
title=ImageFont.truetype("C:/Windows/Fonts/segoeuib.ttf",34)
draw.text((24,16),"MINI CITY 3D  /  Original modern asset collection",font=title,fill=(236,236,226))
draw.text((24,65),"Editable GLB sources • PBR material sections • Authored building and car LODs",font=font,fill=(160,181,194))
for i,name in enumerate(names):
    x=(i%4)*450;y=110+(i//4)*370
    with Image.open(folder/"studio"/f"{name}.png") as source:
        sheet.paste(source.convert("RGB").resize((440,337)),(x+5,y))
    draw.text((x+15,y+340),name.replace('-', ' ').title(),font=font,fill=(235,232,222))
draw.text((24,1590),"Studio lighting preview; actual DX11 captures are included separately.",font=ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf",17),fill=(152,169,180))
sheet.save(folder/"modern-assets-overview.jpg",quality=93)

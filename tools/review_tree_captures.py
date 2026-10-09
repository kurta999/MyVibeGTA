"""Assemble native DX11 views for visual QA without altering the game captures."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

destination = Path(__file__).resolve().parents[1] / 'evidence/trees-20261007'
font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 20)
names = ['tree_detailed', 'tree_default', 'tree_oak', 'tree_thin', 'tree_pineDefaultA']
sheet = Image.new('RGB', (1920, 5 * 302 + 32), '#18252d')
draw = ImageDraw.Draw(sheet)
for column, title in enumerate(['Before / near', 'After / near', 'Before / distant', 'After / distant']):
    draw.text((column * 480 + 12, 5), title, font=font, fill='white')
for row, name in enumerate(names):
    draw.text((12, 32 + row * 302), name, font=font, fill='#ffd876')
    for column, (stage, distance) in enumerate([('before', 'near'), ('after', 'near'), ('before', 'distant'), ('after', 'distant')]):
        image = Image.open(destination / f'{stage}-{name}-{distance}.png')
        sheet.paste(image.resize((480, 270), Image.Resampling.LANCZOS), (column * 480, 60 + row * 302))
sheet.save(destination / 'review-sheet.png')
comparison = Image.new('RGB', (1200, 660), '#18252d')
draw = ImageDraw.Draw(comparison)
for column, stage in enumerate(['before', 'after']):
    draw.text((column * 600 + 20, 10), stage.capitalize(), font=font, fill='white')
    image = Image.open(destination / f'{stage}-tree_thin-near.png')
    comparison.paste(image.crop((500, 120, 1100, 750)), (column * 600, 30))
comparison.save(destination / 'comparison.png')
print('Created review-sheet.png and comparison.png from matching native captures')

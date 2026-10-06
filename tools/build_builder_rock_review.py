"""Build a rock texture/icon review sheet without changing game assets.

Hashes establish which current assets were reviewed; they do not prove that
materials are visually recognizable. Native DX11/world review remains separate.
"""
import argparse
import configparser
import csv
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw


ROCKS = "chalk mudstone shale tuff pumice sandstone limestone travertine dolostone conglomerate slate marble schist gneiss andesite granite diorite gabbro basalt quartzite".split()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("evidence", type=Path)
    parser.add_argument("--native", action="store_true", help="Also assemble labeled crops from the native DX11 world captures")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parent.parent
    output = args.evidence.resolve()
    output.mkdir(parents=True, exist_ok=True)
    catalog = configparser.ConfigParser()
    catalog.read(repo / "data/builder.ini")
    items = {catalog[s]["Id"]: catalog[s] for s in catalog.sections() if s.startswith("Item")}
    assets = {a["id"]: a for a in json.loads((repo / "assets/models/source/builder/manifest.json").read_text())["assets"]}
    sheet = Image.new("RGB", (1500, 1100), (24, 31, 38))
    draw = ImageDraw.Draw(sheet)
    draw.text((12, 10), "Current rock assets: diffuse textures and model-rendered icons | HP is game durability", fill=(245, 225, 170))
    rows = []
    for index, name in enumerate(ROCKS):
        asset, item = assets[name], items[name]
        paths = {"model": repo / f"assets/models/baked/builder/{name}.m3d", "texture": repo / f"assets/models/baked/builder/{name}.png",
                 "icon": repo / f"assets/models/baked/builder/{name}.icon.png", "source": repo / asset["source"]}
        keys = {"model": "sha256", "texture": "texture_sha256", "icon": "icon_sha256", "source": "source_sha256"}
        record = {"item": name, "hp": item["HP"], "blast_resistance": item["BlastResistance"]}
        for kind, path in paths.items():
            fingerprint = hashlib.sha256(path.read_bytes()).hexdigest()
            assert fingerprint == asset[keys[kind]], (name, kind)
            record[kind + "_path"] = str(path.relative_to(repo))
            record[kind + "_sha256"] = fingerprint
        x, y = index % 5 * 300, 40 + index // 5 * 265
        draw.text((x + 8, y + 8), f"{item['Name']} | HP {item['HP']}", fill=(245, 225, 170))
        with Image.open(paths["texture"]) as texture:
            assert texture.size == (256, 256), name
            sheet.paste(texture.convert("RGB").resize((192, 192), Image.Resampling.LANCZOS), (x + 8, y + 34))
        with Image.open(paths["icon"]) as icon:
            thumb = icon.convert("RGBA").resize((88, 88), Image.Resampling.LANCZOS)
            sheet.paste(thumb, (x + 205, y + 75), thumb)
        draw.text((x + 8, y + 235), f"Blast resistance {item['BlastResistance']}", fill=(194, 207, 218))
        rows.append(record)
    with (output / "rock-assets.csv").open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    destination = output / "rocks.review.png"
    sheet.save(destination, optimize=True)
    print(f"Verified hashes for {len(rows)} rock models/textures/icons/sources; review sheet: {destination}")
    if args.native:
        # These are review crops only. Keep the full native PNGs authoritative
        # for scene/HUD context and persistence comparisons.
        native = Image.new("RGB", (1500, 1160), (24, 31, 38))
        draw = ImageDraw.Draw(native)
        draw.text((12, 10), "Native DX11 rock faces and hotbar icons | labeled crops; full captures retained", fill=(245, 225, 170))
        for index, name in enumerate(ROCKS):
            with Image.open(output / (name + ".world.png")) as view:
                assert view.size == (1600, 900), (name, view.size)
                x, y = index % 5 * 300, 40 + index // 5 * 280
                draw.text((x + 8, y + 8), f"{items[name]['Name']} | HP {items[name]['HP']}", fill=(245, 225, 170))
                face = view.crop((610, 285, 995, 635)).convert("RGB").resize((275, 225), Image.Resampling.LANCZOS)
                native.paste(face, (x + 8, y + 29))
                icon = view.crop((530, 800, 590, 875)).convert("RGB").resize((24, 30), Image.Resampling.LANCZOS)
                native.paste(icon, (x + 264, y + 246))
        native_destination = output / "rocks.native.review.png"
        native.save(native_destination, optimize=True)
        print(f"Labeled native review crops: {native_destination}")


if __name__ == "__main__":
    main()

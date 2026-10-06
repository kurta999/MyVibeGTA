"""Assemble labeled QA crops from the earned-tool DX11 captures.

The native screenshots remain the authoritative evidence. This script creates
review sheets only; it does not generate or alter game model/texture assets.
"""
import argparse
import configparser
import csv
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageOps


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("evidence", type=Path)
    args = parser.parse_args()
    root = args.evidence.resolve()
    rows = list(csv.DictReader((root / "catalog.tsv").open(), delimiter="\t"))
    assert len(rows) == 22 and len({row["item"] for row in rows}) == 22
    repo = Path(__file__).resolve().parent.parent
    recipes = configparser.ConfigParser()
    recipes.read(repo / "data/builder-recipes.ini")
    recipe_map = {recipes[section]["Id"]: recipes[section] for section in recipes.sections() if section.startswith("Recipe") and section != "Recipes"}
    manifest = {entry["id"]: entry for entry in json.loads((repo / "assets/models/source/builder/manifest.json").read_text())["assets"]}
    checklist = [
        "# Earned-tool catalog checklist", "",
        "Generated from the live progression report, current recipes, and asset manifest. "
        "`catalog.tsv` retains per-item model/texture/icon paths, speed, tier, durability, target, and resulting resource. "
        "PASS records the scenario's crafting, action, drop/pickup, chest, F5 and on-disk reload assertions; visual review is recorded separately in README.md.", "",
        "| Item | Craft ingredients | Station | Tested target → result | Remaining / maximum durability | Model, texture, icon, source hashes | Gameplay loop |",
        "|---|---|---|---|---|---|---|",
    ]
    for row in rows:
        item = row["item"]
        asset = manifest[item]
        for field, fingerprint in (("model", "sha256"), ("texture", "texture_sha256"), ("icon", "icon_sha256")):
            assert hashlib.sha256((repo / row[field]).read_bytes()).hexdigest() == asset[fingerprint], (item, field)
        assert hashlib.sha256((repo / asset["source"]).read_bytes()).hexdigest() == asset["source_sha256"], item
        recipe = recipe_map[row["recipe"]]
        ingredients = ", ".join(recipe[f"Input{n}Count"] + " × " + recipe[f"Input{n}"] for n in range(int(recipe["Inputs"])))
        checklist.append(f"| {item} | {ingredients} | {recipe['Station']} | {row['target']} → {row['drop']} | {row['remaining_durability']} / {row['max_durability']} | PASS | {row['crafted_used_dropped_stored_reloaded']} |")
    (root / "catalog-checklist.md").write_text("\n".join(checklist) + "\n", encoding="utf-8")
    groups = {
        family: [row["item"] for row in rows if row["item"].endswith("-" + family)]
        for family in ("pickaxe", "axe", "shovel", "hoe")
    }
    groups["special"] = ["shears", "brush"]
    crops = {
        "hand": (850, 240, 1230, 715),
        "third": (480, 300, 1030, 700),
        "drop": (650, 300, 950, 620),
    }
    for family, items in groups.items():
        assert len(items) == (2 if family == "special" else 5)
        sheet = Image.new("RGB", (len(items) * 260, 3 * 300), (24, 31, 38))
        draw = ImageDraw.Draw(sheet)
        for column, item in enumerate(items):
            for row, (view, crop) in enumerate(crops.items()):
                source = root / (item + "." + view + ".png")
                with Image.open(source) as image:
                    assert image.size == (1600, 900), (source, image.size)
                    thumb = ImageOps.contain(image.crop(crop), (252, 270), Image.Resampling.LANCZOS)
                x, y = column * 260, row * 300
                sheet.paste(thumb, (x + (260 - thumb.width) // 2, y + 25))
                draw.text((x + 5, y + 5), item + " | " + view, fill=(245, 225, 170))
        destination = root / (family + ".review.png")
        sheet.save(destination, optimize=True)
        print(destination)


if __name__ == "__main__":
    main()

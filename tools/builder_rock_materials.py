"""Original, deterministic rock-material recipes for the builder asset pipeline.

Morphology follows the art direction in minecraft plan.md. These are stylized
authored materials, not scans, exact mineral proportions, or strength measurements.
Periodic noise/features let a full-face UV tile meet its neighboring block.
"""
import math
import random


REFERENCES = {
    "igneous": "https://www.usgs.gov/faqs/what-are-igneous-rocks",
    "sedimentary": "https://www.bgs.ac.uk/discovering-geology/rocks-and-minerals/",
    "metamorphic": "https://www.usgs.gov/faqs/what-are-metamorphic-rocks?items_per_page=6&page=1",
    "classification": "https://apps.usgs.gov/thesaurus/thesaurus-full.php?thcode=4",
}
RECIPES = {
    "chalk": ("Powdery pale matrix with soft cloudy patches and sparse pinholes", "sedimentary"),
    "mudstone": ("Fine brown matrix with broad irregular bedding seams", "sedimentary"),
    "shale": ("Dark thin sediment laminae with uneven spacing", "sedimentary"),
    "tuff": ("Ash matrix with angular, mixed-size volcanic fragments", "igneous"),
    "pumice": ("Pale matrix with dense rounded and elongated dark vesicles", "igneous"),
    "sandstone": ("Cemented rounded sand grains in warm ochre tones", "sedimentary"),
    "limestone": ("Cream mottling and scattered pale shell-like inclusions", "sedimentary"),
    "travertine": ("Uneven buff bands interrupted by elongated pores", "sedimentary"),
    "dolostone": ("Buff small crystalline facets with occasional small vugs", "sedimentary"),
    "conglomerate": ("Large rounded multicolored pebbles in darker cement", "sedimentary"),
    "slate": ("Blue-gray fine matrix and sharp diagonal cleavage seams", "metamorphic"),
    "marble": ("Cream crystalline matrix with branching gray veins", "metamorphic"),
    "schist": ("Aligned, overlapping elongated mica-like bright and dark flakes", "metamorphic"),
    "gneiss": ("Broad irregular light/dark mineral bands with granular detail", "metamorphic"),
    "andesite": ("Fine gray groundmass with sparse elongated pale crystals", "igneous"),
    "granite": ("Coarse interlocking pink feldspar, pale quartz and dark mica-like grains", "igneous"),
    "diorite": ("Medium interlocking high-contrast black and white crystals", "igneous"),
    "gabbro": ("Coarse predominantly dark green-black crystals with pale grains", "igneous"),
    "basalt": ("Dark fine volcanic groundmass with sparse tiny pores and specks", "igneous"),
    "quartzite": ("Dense pale interlocking sugary grains and scattered bright facets", "metamorphic"),
}


def noise_field(size, cells, rng):
    """Smooth periodic value noise without a sinusoidal/checker lattice."""
    lattice = [[rng.uniform(-1, 1) for _ in range(cells)] for _ in range(cells)]
    axes = []
    for pixel in range(size):
        coordinate = pixel * cells / size
        index = int(coordinate)
        amount = coordinate - index
        axes.append((index, (index + 1) % cells, amount * amount * (3 - 2 * amount)))
    result = []
    for a, b, fy in axes:
        for c, d, fx in axes:
            top = lattice[a][c] * (1 - fx) + lattice[a][d] * fx
            bottom = lattice[b][c] * (1 - fx) + lattice[b][d] * fx
            result.append(top * (1 - fy) + bottom * fy)
    return result


def crystal_field(size, cells, palette, rng):
    """Irregular interlocking grain cells, with independently chosen minerals."""
    step = size / cells
    seeds = {(x, y): ((x + rng.uniform(.15, .85)) * step, (y + rng.uniform(.15, .85)) * step,
                      rng.choice(palette), rng.uniform(-9, 9)) for y in range(cells) for x in range(cells)}
    result = []
    for y in range(size):
        cy = int(y / step)
        for x in range(size):
            cx = int(x / step)
            nearest, second, chosen = 1e20, 1e20, None
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    sx, sy, color, shade = seeds[((cx + dx) % cells, (cy + dy) % cells)]
                    distance_x = (x - sx + size / 2) % size - size / 2
                    distance_y = (y - sy + size / 2) % size - size / 2
                    distance = distance_x ** 2 + distance_y ** 2
                    if distance < nearest:
                        second, nearest, chosen = nearest, distance, (color, shade)
                    elif distance < second:
                        second = distance
            color, shade = chosen
            # Narrow grain boundaries survive mip filtering without turning
            # the entire face into repeating diagonal bands.
            boundary = max(0, 1 - (math.sqrt(second) - math.sqrt(nearest)) / .55)
            result.append(tuple(c + shade - boundary * 13 for c in color))
    return result


def texture(name, color, size=256):
    if name not in RECIPES:
        raise ValueError(name)
    rng = random.Random("builder-rock-v2:" + name)
    cloud = noise_field(size, 4, rng)
    medium = noise_field(size, 13, rng)
    fine = noise_field(size, 67, rng)
    grain = noise_field(size, 127, rng)
    crystal = None
    palettes = {
        "sandstone": (100, [(192, 148, 90), (212, 176, 116), (161, 112, 63), (180, 130, 73)]),
        "dolostone": (43, [(173, 163, 133), (193, 183, 153), (143, 141, 123), (163, 151, 121)]),
        "granite": (24, [(210, 162, 143), (190, 135, 120), (201, 192, 180), (164, 167, 161), (48, 46, 43)]),
        "diorite": (34, [(218, 222, 215), (191, 201, 194), (147, 157, 155), (44, 49, 51), (60, 65, 67)]),
        "gabbro": (25, [(42, 51, 45), (59, 76, 64), (35, 39, 38), (80, 90, 75), (151, 161, 139)]),
        "quartzite": (65, [(216, 216, 200), (182, 185, 167), (200, 200, 181), (159, 163, 149)]),
    }
    if name in palettes:
        cells, palette = palettes[name]
        crystal = crystal_field(size, cells, palette, rng)
    # Random band spacing separates sediment bedding from regular waves. Its
    # perturbation is periodic; shale/slate/travertine/gneiss use distinct scales.
    bands = [rng.uniform(-1, 1) for _ in range(64)]
    def band_value(coordinate, count):
        position = coordinate % size * count / size
        index, amount = int(position), position % 1
        amount = amount * amount * (3 - 2 * amount)
        return bands[index] * (1 - amount) + bands[(index + 1) % count] * amount
    pixels = []
    for y in range(size):
        for x in range(size):
            i = y * size + x
            c, m, f, g = cloud[i], medium[i], fine[i], grain[i]
            shade = 13 * f + 6 * g + 10 * m
            base = color
            if crystal is not None:
                base, shade = crystal[i], 5 * f + 4 * g
            if name == "chalk":
                shade = 10 * c + 5 * m + 4 * g
            elif name == "mudstone":
                coordinate = (y + c * 7 + m * 2) % size
                seam = coordinate % 64
                seam = min(seam, 64 - seam)
                shade += 14 * c - 28 * max(0, 1 - seam / 1.7)
            elif name == "shale":
                coordinate = (y + c * 4 + m * 2) % size
                fraction = coordinate / 8 % 1
                shade += band_value(coordinate, 32) * 24 - 26 * max(0, 1 - min(fraction, 1 - fraction) / .12)
            elif name == "tuff":
                shade = 13 * c + 18 * f + 8 * g
            elif name == "pumice":
                shade = 8 * c + 7 * g
            elif name == "limestone":
                shade = 24 * c + 12 * m + 4 * g
            elif name == "travertine":
                coordinate = (y + c * 8 + m * 2) % size
                shade = band_value(coordinate, 18) * 28 + 5 * f + 4 * g
            elif name == "conglomerate":
                shade = 9 * c + 7 * f + 5 * g - 16
            elif name == "slate":
                coordinate = (y - x + size + m * 2) % size
                seam = coordinate % (size / 7)
                seam = min(seam, size / 7 - seam)
                shade = 7 * c + 5 * g - 34 * max(0, 1 - seam / 1.4)
            elif name == "marble":
                vein = abs(c + m * .28)
                shade = 5 * g + 6 * f - 92 * max(0, 1 - vein / .045) - 18 * max(0, 1 - vein / .15)
            elif name == "schist":
                shade = 18 * m + 7 * f + 7 * g - 17
            elif name == "gneiss":
                coordinate = (y + c * 26 + m * 4) % size
                band = band_value(coordinate, 9)
                base = (191, 189, 174) if band > -.1 else (69, 75, 74)
                shade = 13 * f + 8 * g + 10 * m
            elif name == "andesite":
                shade = 9 * c + 10 * f + 6 * g
            elif name == "basalt":
                shade = 5 * c + 7 * f + 5 * g
            pixels.append([max(0, min(255, int(channel + shade))) for channel in base])

    def inclusion(cx, cy, rx, ry, tint, angle=0, rim=0, angular=False):
        co, si = math.cos(angle), math.sin(angle)
        reach = math.ceil(max(rx, ry) * 1.25)
        for yy in range(math.floor(cy) - reach, math.floor(cy) + reach + 1):
            for xx in range(math.floor(cx) - reach, math.floor(cx) + reach + 1):
                dx, dy = xx - cx, yy - cy
                u, v = (dx * co + dy * si) / rx, (-dx * si + dy * co) / ry
                distance = abs(u) + abs(v) if angular else math.sqrt(u * u + v * v)
                i = (yy % size) * size + xx % size
                boundary = 1 + fine[i] * (.13 if angular else .1)
                if distance >= boundary:
                    continue
                edge = max(0, (distance / boundary - .65) / .35)
                relief = (u * .35 - v * .5) * 8 + fine[i] * 5 + rim * edge
                pixels[i] = [max(0, min(255, int(channel + relief))) for channel in tint]

    def scatter(count, radius_x, radius_y, palette, angle=None, rim=0, angular=False):
        for _ in range(count):
            inclusion(rng.random() * size, rng.random() * size, rng.uniform(*radius_x), rng.uniform(*radius_y),
                      rng.choice(palette), rng.uniform(-math.pi, math.pi) if angle is None else angle + rng.uniform(-.16, .16), rim, angular)

    if name == "chalk":
        scatter(90, (.4, 1.2), (.4, 1.2), [(158, 159, 143), (178, 177, 163)], rim=12)
    elif name == "tuff":
        scatter(420, (2, 8), (1.5, 6), [(158, 149, 126), (91, 87, 75), (178, 173, 153), (112, 101, 82)], angular=True)
    elif name == "pumice":
        scatter(730, (1, 4.5), (1.5, 6), [(93, 90, 76), (114, 109, 94), (133, 128, 110)], rim=25)
    elif name == "limestone":
        scatter(150, (1, 4), (.5, 2), [(222, 215, 184), (154, 147, 118)], rim=6)
    elif name == "travertine":
        scatter(140, (1, 5), (.5, 1.8), [(125, 110, 83), (150, 131, 101)], angle=0, rim=20)
    elif name == "dolostone":
        scatter(45, (.8, 2), (.8, 2), [(110, 103, 82)], rim=14)
    elif name == "conglomerate":
        scatter(125, (5, 14), (4, 11), [(174, 156, 128), (111, 114, 107), (74, 72, 66), (204, 192, 161), (146, 99, 75)], rim=-18)
    elif name == "schist":
        scatter(1500, (2, 7), (.45, 1.4), [(193, 203, 200), (76, 85, 85), (153, 169, 163), (210, 215, 199)], angle=-.32, rim=-7, angular=True)
    elif name == "andesite":
        scatter(135, (.9, 2), (2, 4), [(169, 177, 171), (191, 194, 181)], angular=True)
        scatter(260, (.4, 1), (.4, 1), [(69, 73, 72)])
    elif name == "basalt":
        scatter(85, (.5, 1.3), (.5, 1.3), [(26, 28, 29)], rim=10)
        scatter(65, (.3, .8), (.3, .8), [(105, 110, 108)], angular=True)
    elif name == "quartzite":
        scatter(330, (.3, 1), (.3, 1), [(231, 232, 216)], angular=True)
    return bytearray(channel for pixel in pixels for channel in (*pixel, 255))


def recipe_manifest(name):
    description, family = RECIPES[name]
    return {"version": 2, "recipe": description, "family": family, "seed": "builder-rock-v2:" + name,
            "uv": "Full 0..1 tile per cube face", "reference": REFERENCES[family], "classification_reference": REFERENCES["classification"],
            "license": "Original project-authored asset; reference prose/photos not copied"}

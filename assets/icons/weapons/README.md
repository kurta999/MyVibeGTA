These original AI-generated textured inventory renders were created with the built-in image_gen tool on 2026-10-03. The exact generation prompt is in `PROMPT.txt`, and the reviewed original RGBA sheet is `source-atlas.png` (1402x1122).

`tools/build_weapon_icons.py` extracts all 20 weapons/tools using reviewed alpha-component bounds, adds transparent padding, and preserves the original pixels. Each icon uses a separate PNG to prevent mip bleeding between neighboring items. `manifest.json` records the source crop and final dimensions.

The DX11 world uses camera-facing, depth-tested alpha sprites with a gentle bob, consistent aspect ratios, and unlit textures for night visibility. The HUD shares the same images. Tiny radar/map symbols retain their existing readable pictograms. Missing custom weapon icons are not replaced with misleading cyan spheres; a matching PNG can be added here.

Rebuild icons from the checked-in source:

```powershell
python tools/build_weapon_icons.py assets/icons/weapons/source-atlas.png
```

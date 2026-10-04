# Downloaded vehicle models

The DX11 vehicle catalog renders downloaded authored meshes. The procedural
Aurora cars and their generation recipes have been removed. Sources are retained
in `source/expansion/`, `source/helicopter/` and `source/vehicles/`. Source SHA-256,
evaluated triangle counts, transforms and rigid parts are recorded in
`baked/vehicles/expansion-*.import.json`. None of these vehicle meshes were
generated with ImageGen or assembled from new boxes.

| Vehicle | Author and source | License | Cooked triangles |
|---|---|---|---:|
| Car: Range Rover | [IvOfficial](https://poly.pizza/m/8zk4o6nALW) | CC BY 3.0 | 31,418 |
| Sports car: Mazda RX-7 | [IvOfficial](https://poly.pizza/m/SnIoWlh7S2) | CC BY 3.0 | 19,720 |
| Motorcycle: Suzuki SV650 | [Paul Spooner](https://poly.pizza/m/1yfyze7uGxS) | CC BY 3.0 | 684 |
| Skateboard | [Thomas van iseghem (Superthomyboy)](https://poly.pizza/m/3C0mzQB3obs) | CC BY 3.0 | 4,128 |
| Bicycle | [Poly by Google](https://poly.pizza/m/eRg_VrQlvXY) | CC BY 3.0 | 10,714 |
| Tractor | [JordanGrant3D](https://jordangrant3d.itch.io/tractor) | CC0 1.0 | 108,261 |
| CombineHarvester v3 | [printable_models](https://free3d.com/3d-model/combineharvester-v3--793584.html) | **Personal Use** | 92,076 |
| Abrams tank | [Sketlux, adapted from yd's Freeciv tank](https://opengameart.org/content/abrams-tank) | CC0 1.0 | 45,532 |
| Peterbilt 389 truck | [luke1985](https://opengameart.org/content/truck-peterbilt-389) | CC BY 4.0 (selected) | 32,926 |
| Trailer from Lowpoly Semi Truck | [Craig Snedeker](https://craigsnedeker.itch.io/lowpoly-semi-truck) | **CC BY-NC-SA 4.0** | 13,044 |
| Cesna Airplane | [wobba89](https://opengameart.org/content/cesna-airplane) | CC BY 3.0 | 13,142 |
| Helicopter | [Poly by Google](https://poly.pizza/m/cTzINMr0WdS) | CC BY 3.0 | 1,041 |
| Motorboat | [Quaternius](https://poly.pizza/m/5UEl54KsuC) | CC0 1.0 | 224 |

The user confirmed **personal/private use** on 2026-10-04. The combine's Personal
Use terms and trailer's noncommercial terms are retained for that purpose.
These two models must be replaced before a commercial release; review the
combine's terms before any public distribution. Modified trailer geometry
remains under CC BY-NC-SA 4.0. No endorsement by model authors or vehicle brands
is implied. License links: [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/),
[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/),
[CC0](https://creativecommons.org/publicdomain/zero/1.0/),
[CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/).

Conversion uses Blender 4.5 LTS in background factory mode with scripts disabled.
`tools/bake_expansion_models.py` evaluates source modifiers, triangulates,
normalizes dimensions and facing, preserves UVs/normals/vertex colors/base-color
maps, and groups existing connected geometry into bounded rigid assemblies.
Reports assert that every evaluated source triangle reaches the baked meshes.
Background display geometry is excluded from the tank/truck and only the actual
trailer objects are selected from its source scene. The combine OBJ has no MTL;
green/yellow paint and dark tires are assigned to retained source components.
It has a **corn header with a rotating cutting auger**, rather than a wheat reel.
Original files remain unchanged. Wheels, helicopter rotor and plane propeller
rotate about source-derived pivots; the same parts become physical debris.

`data/vehicle-models.ini` stores each rigid assembly's mesh, center, collision
extent and spin axis (0 static, 1 wheel X, 2 rotor Y, 3 propeller Z, 4 auger X).
Tank axes 5/6 designate the turret yaw and gun elevation joints. The baker
separates the authored turret, hatch and barrel and removes their original
posed yaw/elevation. Source-derived pivots and the baked barrel length keep
the moving muzzle, shots and explosion fragments aligned with the model;
all 45,532 tank triangles remain present.
Physics hulls approximate the visual models. Asset detail varies: the original
motorboat/helicopter and the motorcycle are low-poly authored assets; the new
tractor, combine, tank, cars and truck retain substantially more detail.

Rebuild sources with:

```powershell
blender --background --factory-startup --disable-autoexec --python tools/bake_expansion_models.py
```

The cooker does not download models. `tools/fetch_expansion_models.py` fetches
the five Poly Pizza GLBs and records their public source metadata. The other
sources are downloaded from the author pages above and retained locally.

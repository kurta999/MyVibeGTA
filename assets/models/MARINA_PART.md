# Marina Part-inspired assets

`tools/build_marina_assets.py` creates all files in `assets/models/baked/marina/`. The four building variants and their LODs are original procedural meshes. `MarinaFacade_Color.png` and `MarinaFacade_NormalDX.png` are original procedural texture atlases. No Google Maps imagery or photographs are copied into the game or its textures.

The reference study used Google Maps for the location and street layout and official project galleries for the massing of balcony and terrace buildings:

- [Marina Part phase I](https://autoker.hu/hu/gallery/marina-part-phase-i/)
- [Marina Part phase II](https://autoker.hu/hu/gallery/marina-part-phase-ii/)
- [Marina Part phase IV](https://autoker.hu/gallery/marina-part-phase-iv/)

The district is an interpretation for gameplay. Parcels and building shapes are not survey-accurate. The generated assets can be regenerated with Python 3 using only its standard library:

```powershell
python tools/build_marina_assets.py
```

## Authored four-level DX11 chains

Each existing building now also has indexed `-lod1`, `-lod2`, and `-lod3`
geometry and a versioned `MCLOD1` catalog beside it. The original full mesh and
legacy `-lod` pair remain readable. `AUTHORED_LODS.json` records triangle counts,
achieved ratios, and hashes. Regenerate the chains without rebuilding textures:

```powershell
python tools/build_marina_assets.py --lods-only
```

LOD1 removes small window frames while retaining balcony structure. LOD2 uses
window/guard panels and simpler balconies. LOD3 merges balcony bays into larger
floor slabs/guards while keeping window rhythms, roof/parapets and overall
bounds. Quads retain distinct normal/UV seams and shared corner indices.

| Variant | LOD1 / full | LOD2 / full | LOD3 / full |
| --- | ---: | ---: | ---: |
| Wave | 49.3% | 20.5% | 8.6% |
| Terrace | 49.0% | 20.7% | 9.6% |
| Courtyard | 50.2% | 18.8% | 12.0% |
| Bayfront | 50.5% | 20.1% | 9.3% |

DX11 selects these levels at projected sizes 500/240/80 pixels with 12%
hysteresis, scaled by the persisted LOD setting. The loader rejects catalog
paths outside the asset namespace, missing meshes, nondecreasing geometry, and
incompatible bounds, logging a diagnostic while retaining the legacy fallback.
`--smoke --marina --lod-view` tints selected levels green/yellow/orange/red;
`--lod-level-0` through `--lod-level-3` force inspection levels in smoke mode.
These original Marina chains do not complete LOD authoring for the new named
sedan, pedestrians or wolf showcase assets.

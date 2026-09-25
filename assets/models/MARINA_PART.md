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

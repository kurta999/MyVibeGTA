# Showcase source audit

Checked 2026-09-28. These records describe public listing/API metadata, not imported assets. None of these model archives has been downloaded or inspected, and none is included in a game package.

## Grey Wolf

- [Grey Wolf (Rigged and Animated), rhcreations](https://sketchfab.com/3d-models/grey-wolf-rigged-and-animated-56de4df672654ed599777d2980bf0f53)
- [Authoritative model metadata](https://api.sketchfab.com/v3/models/56de4df672654ed599777d2980bf0f53): downloadable; 22,498 triangles, 11,411 vertices, one animation.
- Listing and metadata license: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/). Attribute rhcreations, link the source/license, and identify modifications if this asset is integrated.
- The listing describes Blender 2.79/Cycles authoring. The viewer exposes a single `Wolf ArmatureAction`; separate idle, locomotion, attack, hit, and death clips are not verified.
- An anonymous request to the official `/download` endpoint returned HTTP 401. The available browser has no signed-in Sketchfab session. An authenticated download or a user-supplied original archive is required before validating textures, rig, animation content, dimensions, and provenance hashes.

## Classic Sedan candidates

The short name in the original plan does not uniquely identify a source. Two public candidates were checked:

| Listing | Metadata | Triangles | License |
| --- | --- | ---: | --- |
| [Realistic Classic Sedan – Unity & Unreal](https://sketchfab.com/3d-models/realistic-classic-sedan-unity-unreal-c0f6b70538fc4864816239c764cf6dc7) | [API record](https://api.sketchfab.com/v3/models/c0f6b70538fc4864816239c764cf6dc7) | 1,108,270 | CC BY 4.0 |
| [Chevrolet Caprice Classic Sedan](https://sketchfab.com/3d-models/chevrolet-caprice-classic-sedan-96200ce146014b858e2b284c123bbe42) | [API record](https://api.sketchfab.com/v3/models/96200ce146014b858e2b284c123bbe42) | 17,040 | CC BY 4.0 |

Both metadata records mark the model downloadable and report zero animations. Neither has been selected as the intended named asset. The first exceeds the plan's 60k-triangle car budget and requires reduction/baking. Exact creator credit, downloaded license files, model contents, source hashes, wheel/seat/door transforms, and LODs must be recorded at import.

## Renderpeople

- [Free model samples](https://renderpeople.com/free-3d-people/)
- [Official terms](https://renderpeople.com/general-terms-and-conditions/), sections 2.5 and 4.3(b): free assets are covered by the terms; distribution allowing easy individual extraction/download of models is restricted.
- Excluding original FBX/glTF files is insufficient if a shipped cooked mesh is also easily extractable. The current M3D/M3S package format does not establish compliance. No Renderpeople model has been integrated or shipped. This requires a license-compatible distribution approach or an approved redistributable substitute before delivery.

## Existing sources

Existing Poly Haven apartment/factory sources and their license records remain in the repository. Existing traffic/weapon provenance is documented separately in `TRAFFIC_WEAPONS_LICENSES.md`. A matching filename, an online preview, or public metadata is not a substitute for archive inspection and a recorded source hash.

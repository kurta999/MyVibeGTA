# Recorded vehicle audio

| Source | Credit | Selected license | Runtime profiles |
|---|---|---|---|
| [Car Engine Loop 96kHz, 4s](https://opengameart.org/content/car-engine-loop-96khz-4s) | qubodup | CC BY 3.0 | car, sports car, motorcycle, boat |
| [Engine-loop heavy vehicle/tank](https://opengameart.org/content/engine-loop-heavy-vehicletank) | Nayckron, adapted from qubodup | CC BY 3.0 | tractor, combine, tank, truck |
| [Helicopter Sounds](https://opengameart.org/content/helicopter-sounds) | aquinn | CC0 1.0 | helicopter |
| [Airplane Prop Loop](https://opengameart.org/content/airplane-prop-loop) | jakobthiesen; loop edit by AntumDeluge | CC BY 3.0 | airplane |

Sources are retained in `source/`. Vehicle-specific profiles are adapted from
these four recordings, rather than separate field recordings of each model.
`tools/bake_vehicle_audio.py` decodes with Blender's Aud library, selects sustained
engine sections, removes DC, filters excess hiss, applies profile pitch and
crossfades the loop boundary. Output is mono PCM16 at 48kHz; source hashes and
conversion parameters are in `vehicles/conversion.json`. Runtime voices loop
continuously and smoothly adjust pitch and gain with RPM. Pedal vehicles and the
trailer have no engine loop. The weapon effects remain synthesized, with distinct
parameters/signatures per weapon and a separate heavy tank-cannon signature.

Rebuild with Blender 4.5 LTS:

```powershell
blender --background --factory-startup --disable-autoexec --python tools/bake_vehicle_audio.py
```

[CC BY 3.0](https://creativecommons.org/licenses/by/3.0/) and
[CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) apply to the retained
sources and their corresponding modified clips. Retain these credits with any
distribution. Streaming station content is received live and is not packaged.

"""Check captured DX11 effect contributions and negative occlusion cases.

Run tools/verify_lighting.ps1 first. Pillow is used only for captured evidence.
"""
import hashlib
import json
import argparse
from pathlib import Path
from PIL import Image, ImageChops, ImageStat

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--evidence',type=Path,default=ROOT/'evidence/lighting-20260928')
parser.add_argument('--executable',type=Path,default=ROOT/'build-msvc-ninja/MiniCity3D.exe')
args=parser.parse_args();EVIDENCE=args.evidence
manifest=json.loads((EVIDENCE/"captures.json").read_text(encoding="utf-8-sig"))
assert manifest["ExecutableSha256"].lower()==hashlib.sha256(
    args.executable.read_bytes()).hexdigest()
for scene in manifest["Scenes"]:
    assert scene["ExitCode"]==0
    assert scene["ScreenshotSha256"].lower()==hashlib.sha256(
        (EVIDENCE/(scene["Name"]+".png")).read_bytes()).hexdigest()

def capture(name,box):
    image=Image.open(EVIDENCE/(name+".png")).convert("RGB")
    assert image.size==(1920,1080)
    return image.crop(box)

# Exclude all HUD regions; the visible ghosts and halo lie inside this crop.
flare_box=(250,260,1500,900)
visible=capture("flare-visible",flare_box)
assert max(high for low,high in visible.getextrema())>200
for name in ("flare-disabled","flare-occluded","flare-night"):
    image=capture(name,flare_box)
    assert image.getbbox() is None,(name,"unexpected flare contribution")
horizon_mean=max(ImageStat.Stat(capture("sun",(500,610,1400,625))).mean)
assert horizon_mean<190,("invalid sun derivative at horizon",horizon_mean)

glint_box=(840,495,1040,600)
glint=capture("facade",glint_box)
no_sun=capture("facade-no-sun",glint_box)
glint_gain=sum(ImageStat.Stat(glint).mean)/3-sum(ImageStat.Stat(no_sun).mean)/3
assert glint_gain>20,("sun highlight missing",glint_gain)

car_box=(820,575,1070,710)
coat_delta=ImageChops.difference(capture("car-coat",car_box),capture("car-no-coat",car_box))
coat_difference=sum(ImageStat.Stat(coat_delta).mean)/3
assert coat_difference>.15,("clearcoat contribution missing",coat_difference)
report={"captures":len(manifest["Scenes"]),"flare_negative_cases":3,
        "facade_sun_gain_8bit":round(glint_gain,3),
        "car_coat_difference_8bit":round(coat_difference,3),
        "horizon_max_mean_8bit":round(horizon_mean,3),
        "scope":"Fixed-camera rendered effect and occlusion checks; no moving-scene stability claim"}
(EVIDENCE/"effect-checks.json").write_text(json.dumps(report,indent=2)+"\n")
print(json.dumps(report))

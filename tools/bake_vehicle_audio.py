"""Prepare licensed recordings as smooth 48 kHz mono loops using Blender Audaspace."""
import aud, pathlib, numpy as np, wave, json, hashlib
ROOT=pathlib.Path(__file__).resolve().parents[1]
SOURCE=ROOT/'assets/audio/source';OUT=ROOT/'assets/audio/vehicles';OUT.mkdir(parents=True,exist_ok=True)
sources={
 'car':SOURCE/'engine-loop/engine-loop-1.wav',
 'heavy':SOURCE/'heavy-engine.wav',
 'helicopter':SOURCE/'helicopter.mp3',
 'airplane':SOURCE/'airplane.flac',
}
decoded={k:aud.Sound.file(str(p)).rechannel(1).resample(48000).data()[:,0].astype(np.float64) for k,p in sources.items()}
offsets={}
for key,pcm in decoded.items():
    if len(pcm)>48000*8:
        # Choose a steady running section; avoid ignition, shutdown and pauses.
        block=4800;energy=np.array([np.mean(pcm[n:n+block]**2) for n in range(0,len(pcm)-block,block)])
        candidates=[]
        for start in range(len(energy)-40):
            e=energy[start:start+40];mean=np.mean(e)
            if mean>np.median(energy)*.8:candidates.append((np.std(e)/max(mean,1e-9),start))
        start=min(candidates)[1]*block
        decoded[key]=pcm[start:start+48000*4];offsets[key]=start/48000
profiles=[('car','car',1,5500),('sport-car','car',1.12,6500),('motorcycle','car',1.36,6500),
 ('boat','heavy',.92,3000),('helicopter','helicopter',1,6500),
 ('tractor','heavy',.83,3400),('combine','heavy',.75,3000),('tank','heavy',.65,2600),
 ('truck','heavy',.9,4500),('airplane','airplane',1,6000)]
reports=[]
for name,key,pitch,cutoff in profiles:
    data=decoded[key];data=data-np.mean(data)
    # Recorded timbre remains intact; low-pass removes hiss above useful engine harmonics.
    taps=np.arange(-32,33);kernel=2*cutoff/48000*np.sinc(2*cutoff/48000*taps)*np.hanning(65);kernel/=sum(kernel)
    data=np.convolve(np.pad(data,(32,32),mode='wrap'),kernel,mode='valid')
    if pitch!=1:data=np.interp(np.arange(0,len(data),pitch),np.arange(len(data)),data)
    blend=min(1440,len(data)//12);a=np.linspace(0,1,blend)
    data[-blend:]=data[-blend:]*(1-a)+data[:blend]*a;data=data[blend:]
    data*=.78/max(np.max(np.abs(data)),1e-8)
    pcm=np.rint(np.clip(data,-1,1)*32767).astype('<i2')
    with wave.open(str(OUT/(name+'.wav')),'wb') as target:
        target.setnchannels(1);target.setsampwidth(2);target.setframerate(48000);target.writeframes(pcm.tobytes())
    reports.append(dict(profile=name,source=str(sources[key].relative_to(ROOT)),offset=offsets.get(key,0),
        pitch=pitch,lowpass=cutoff,samples=len(pcm),source_sha256=hashlib.sha256(sources[key].read_bytes()).hexdigest()))
    print(name,len(pcm),'samples',flush=True)
(OUT/'conversion.json').write_text(json.dumps(reports,indent=2)+'\n')

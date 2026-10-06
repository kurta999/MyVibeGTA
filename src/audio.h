#pragma once
namespace audio {
enum class Effect { Shot, SilencedShot, Pickup, Success, Step, Engine, Splash, Hit, Fail, Reload, Surf, Skid, Traffic, Explosion, Bow, Flame, Water, Electric,
#ifdef MINI_CITY_JOLT
    BuilderContact,
#endif
    Count };
bool init();
void play(Effect effect,int variant=0);
void playAt(Effect effect,float x,float z,int variant=0);
void setListener(float x,float z,float yaw);
void setVolume(int percent);
void updateEngine(int vehicleKind,float speedRatio,float throttle,float dt);
void stopEngine();
unsigned recordedEngineSamples(int vehicleKind);
unsigned long long effectFingerprint(Effect effect,int variant);
#ifdef MINI_CITY_JOLT
bool exportEffectWav(Effect effect,int variant,const wchar_t* path);
#endif
void shutdown();
}

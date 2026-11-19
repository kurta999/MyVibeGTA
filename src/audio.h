#pragma once
#include <string>
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
// One engine sound slot per vehicle kind (checked against game::KIND_COUNT).
constexpr int ENGINE_KIND_COUNT=13;
// How engine pitch/gain follow speed and throttle.
enum class EngineCurve { Geared, Rotor, Propeller, Pedal };
// Overrides a kind's engine clip (assets/audio/vehicles/<clip>.wav, empty =
// silent) and curve; physics::load passes the values from vehicles.ini.
void setEngineProfile(int vehicleKind,const std::string& clip,EngineCurve curve);
void updateEngine(int vehicleKind,float speedRatio,float throttle,float dt);
void stopEngine();
unsigned recordedEngineSamples(int vehicleKind);
unsigned long long effectFingerprint(Effect effect,int variant);
#ifdef MINI_CITY_JOLT
bool exportEffectWav(Effect effect,int variant,const wchar_t* path);
#endif
void shutdown();
}

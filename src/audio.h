#pragma once
namespace audio {
enum class Effect { Shot, SilencedShot, Pickup, Success, Step, Engine, Splash, Hit, Fail, Reload, Surf, Skid, Traffic, Count };
bool init();
void play(Effect effect,int variant=0);
void playAt(Effect effect,float x,float z,int variant=0);
void setListener(float x,float z,float yaw);
void setVolume(int percent);
void shutdown();
}

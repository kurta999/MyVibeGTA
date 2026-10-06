#pragma once
#include "builder.h"
#include <vector>

namespace builder_feedback {
enum class Cue {Stone,Wood,Soil,Sand,Snow,Foliage,Metal,Brush};
struct Particle {game::Vec3 p,v;float life,total,size;int item;Cue cue;bool dust=false;};
struct Line {game::Vec3 a,b;float width;};
struct Contact {builder::Target target;builder::Tool tool;Cue cue;std::uint64_t serial=0;};
void clear();
void update(float dt);
void contact(const builder::Target& target,builder::Tool tool,bool completed=false);
const std::vector<Particle>& particles();
const Contact& lastContact();
Cue cue(int item,builder::Tool tool);
float cycle(builder::Tool tool);
std::vector<Line> cracks();
}

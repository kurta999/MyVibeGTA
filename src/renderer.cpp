#include "game.h"
#include <GL/glu.h>
#include <cstdio>
#include <cstring>
#include "textures.h"
#include "ui.h"
#include "camera.h"
namespace game {
void quad(float x1,float z1,float x2,float z2,float y,Color c){
    color(c);glBegin(GL_QUADS);glNormal3f(0,1,0);glVertex3f(x1,y,z1);glVertex3f(x2,y,z1);
    glVertex3f(x2,y,z2);glVertex3f(x1,y,z2);glEnd();
}
void texturedQuad(float x1,float z1,float x2,float z2,float y,int tile,float repeatX,float repeatZ,bool rotated=false){
    textures::bind(tile);color(rgb(255,255,255));
    glBegin(GL_QUADS);glNormal3f(0,1,0);
    if(!rotated){
        glTexCoord2f(0,0);glVertex3f(x1,y,z1);glTexCoord2f(repeatX,0);glVertex3f(x2,y,z1);
        glTexCoord2f(repeatX,repeatZ);glVertex3f(x2,y,z2);glTexCoord2f(0,repeatZ);glVertex3f(x1,y,z2);
    }else{
        glTexCoord2f(0,0);glVertex3f(x1,y,z1);glTexCoord2f(0,repeatX);glVertex3f(x2,y,z1);
        glTexCoord2f(repeatZ,repeatX);glVertex3f(x2,y,z2);glTexCoord2f(repeatZ,0);glVertex3f(x1,y,z2);
    }
    glEnd();textures::unbind();
}
void texturedBox(float x,float y,float z,float w,float h,float d,int tile,Color tint=rgb(255,255,255),float u=1,float v=1){
    float x0=x-w/2,x1=x+w/2,z0=z-d/2,z1=z+d/2,y1=y+h;
    textures::bind(tile);color(tint);
    glBegin(GL_QUADS);
    glNormal3f(0,0,-1);glTexCoord2f(0,0);glVertex3f(x0,y,z0);glTexCoord2f(u,0);glVertex3f(x1,y,z0);
    glTexCoord2f(u,v);glVertex3f(x1,y1,z0);glTexCoord2f(0,v);glVertex3f(x0,y1,z0);
    glNormal3f(0,0,1);glTexCoord2f(0,0);glVertex3f(x1,y,z1);glTexCoord2f(u,0);glVertex3f(x0,y,z1);
    glTexCoord2f(u,v);glVertex3f(x0,y1,z1);glTexCoord2f(0,v);glVertex3f(x1,y1,z1);
    glNormal3f(-1,0,0);glTexCoord2f(0,0);glVertex3f(x0,y,z1);glTexCoord2f(d/160.0f,0);glVertex3f(x0,y,z0);
    glTexCoord2f(d/160.0f,v);glVertex3f(x0,y1,z0);glTexCoord2f(0,v);glVertex3f(x0,y1,z1);
    glNormal3f(1,0,0);glTexCoord2f(0,0);glVertex3f(x1,y,z0);glTexCoord2f(d/160.0f,0);glVertex3f(x1,y,z1);
    glTexCoord2f(d/160.0f,v);glVertex3f(x1,y1,z1);glTexCoord2f(0,v);glVertex3f(x1,y1,z0);
    glNormal3f(0,1,0);glTexCoord2f(0,0);glVertex3f(x0,y1,z0);glTexCoord2f(u,0);glVertex3f(x1,y1,z0);
    glTexCoord2f(u,d/160.0f);glVertex3f(x1,y1,z1);glTexCoord2f(0,d/160.0f);glVertex3f(x0,y1,z1);
    glEnd();textures::unbind();
}
void box(float x,float y,float z,float w,float h,float d,Color c){
    float x0=x-w/2,x1=x+w/2,z0=z-d/2,z1=z+d/2,y1=y+h;
    glBegin(GL_QUADS);
    glNormal3f(0,1,0);color(c,1.10f);glVertex3f(x0,y1,z0);glVertex3f(x1,y1,z0);glVertex3f(x1,y1,z1);glVertex3f(x0,y1,z1);
    glNormal3f(-1,0,0);color(c,0.92f);glVertex3f(x0,y,z0);glVertex3f(x0,y1,z0);glVertex3f(x0,y1,z1);glVertex3f(x0,y,z1);
    glNormal3f(1,0,0);color(c,1.0f);glVertex3f(x1,y,z0);glVertex3f(x1,y,z1);glVertex3f(x1,y1,z1);glVertex3f(x1,y1,z0);
    glNormal3f(0,0,-1);color(c,0.85f);glVertex3f(x0,y,z0);glVertex3f(x1,y,z0);glVertex3f(x1,y1,z0);glVertex3f(x0,y1,z0);
    glNormal3f(0,0,1);color(c,0.96f);glVertex3f(x0,y,z1);glVertex3f(x0,y1,z1);glVertex3f(x1,y1,z1);glVertex3f(x1,y,z1);
    glEnd();
}
void sphere(float x,float y,float z,float radius,Color c){
    color(c);glPushMatrix();glTranslatef(x,y,z);
    GLUquadric* q=gluNewQuadric();gluSphere(q,radius,7,5);gluDeleteQuadric(q);glPopMatrix();
}
void cylinder(float x,float y,float z,float radius,float height,Color c){
    color(c);glPushMatrix();glTranslatef(x,y,z);glRotatef(-90,1,0,0);
    GLUquadric* q=gluNewQuadric();gluCylinder(q,radius,radius*0.75f,height,7,1);gluDeleteQuadric(q);glPopMatrix();
}
void drawBuilding(const Building& b,int index){
    float range=ui::graphicsQuality==0?475.0f:ui::graphicsQuality==1?675.0f:850.0f;
    if(len(Vec2{b.x+b.w/2,b.z+b.d/2}-player)>range)return;
    texturedQuad(b.x-20,b.z-20,b.x+b.w+20,b.z+b.d+20,0.12f,4,b.w/110,b.d/110);
    texturedBox(b.x+b.w/2,0.25f,b.z+b.d/2,b.w,b.h,b.d,index%4,rgb(255,255,255),b.w/160,b.h/90);
    box(b.x+b.w/2,b.h+0.25f,b.z+b.d/2,b.w+5,3,b.d+5,rgb(90,105,111));
    // Roof parapet, corner trim, and a shallow storefront give each block a silhouette.
    Color trim=index%2?rgb(211,201,178):rgb(175,188,185);
    box(b.x+b.w/2,b.h+3,b.z, b.w+6,7,5,trim);
    box(b.x+b.w/2,b.h+3,b.z+b.d,b.w+6,7,5,trim);
    box(b.x,b.h+3,b.z+b.d/2,5,7,b.d+5,trim);
    box(b.x+b.w,b.h+3,b.z+b.d/2,5,7,b.d+5,trim);
    if(ui::graphicsQuality>0){
        for(int floor=1;floor<int(b.h/29);++floor){
            float y=floor*29.0f;
            box(b.x+b.w/2,y,b.z-2,b.w+3,2,5,trim);
            if(index%3==0){
                for(int bay=0;bay<3;++bay){
                    float bx=b.x+b.w*(bay+0.5f)/3.0f;
                    box(bx,y-4,b.z-5,b.w/5,2,7,rgb(86,94,95));
                }
            }
        }
        box(b.x+b.w/2,b.h+3,b.z+b.d/2,b.w*0.24f,5,b.d*0.24f,rgb(91,103,106));
    }
    Color awning=index%3==0?rgb(108,56,59):index%3==1?rgb(55,115,102):rgb(119,98,65);
    box(b.x+b.w/2,10,b.z-8,b.w*0.48f,3,16,awning);
    box(b.x+b.w/2,0.3f,b.z-1.5f,16,23,3,rgb(62,87,93));
}
void drawGrass(float x,float z,float scale){
    Color green=rgb(85,139,67);
    glBegin(GL_TRIANGLES);color(green);
    glNormal3f(0,1,0);
    glVertex3f(x-2*scale,0.4f,z);glVertex3f(x+2*scale,0.4f,z);glVertex3f(x,9*scale,z);
    glVertex3f(x,0.4f,z-2*scale);glVertex3f(x,0.4f,z+2*scale);glVertex3f(x,9*scale,z);
    glEnd();
}
void drawStreetTree(float x,float z){
    cylinder(x,0,z,2.1f,23,rgb(104,76,53));
    for(int level=0;level<3;++level){
        float y=18+level*7.0f;
        sphere(x,y,z,level==2?12.0f:14.0f,rgb(64+level*7,119+level*5,65));
    }
}
void drawPalm(float x,float z){
    textures::bind(14);color(rgb(255,255,255));glPushMatrix();glTranslatef(x,0,z);glRotatef(-90,1,0,0);
    GLUquadric* trunk=gluNewQuadric();gluQuadricTexture(trunk,GL_TRUE);gluCylinder(trunk,3,2,47,9,6);
    gluDeleteQuadric(trunk);glPopMatrix();textures::unbind();
    textures::bind(15);color(rgb(255,255,255));
    for(int i=0;i<7;++i){
        float a=i*2*PI/7;float dx=std::cos(a),dz=std::sin(a);
        glBegin(GL_TRIANGLES);glNormal3f(0,1,0);
        glTexCoord2f(0.5f,1);glVertex3f(x,48,z);
        glTexCoord2f(0,0);glVertex3f(x+dx*30-dz*7,36,z+dz*30+dx*7);
        glTexCoord2f(1,0);glVertex3f(x+dx*30+dz*7,36,z+dz*30-dx*7);glEnd();
    }
    textures::unbind();
}
void drawPerson(Vec2 p,float angle,bool playerCharacter,int style=0,bool pedestrianArmed=false,float hitFlash=0){
    if(!playerCharacter&&len(p-player)>650)return;
    glPushMatrix();glTranslatef(p.x,0,p.z);glRotatef(-angle*180/PI,0,1,0);
    int variant=playerCharacter?1:style%4;
    Color flash=hitFlash>0?rgb(255,132,132):rgb(255,255,255);
    float stride=playerCharacter&&occupied<0?std::sin(walkPhase)*4.0f:playerCharacter?0:std::sin(worldTime*8+p.x)*2.5f;
    float armSwing=std::sin(playerCharacter?walkPhase:worldTime*8+p.x)*2.3f;
    texturedBox(stride,0,-3,5,12,5,24+variant);
    texturedBox(-stride,0,3,5,12,5,24+variant);
    texturedBox(0,11,0,12,14,11,20+variant,flash);
    texturedBox(-armSwing,13,-8,4,12,5,20+variant,flash);
    texturedBox(0,13,-8,3,5,4,30);
    bool armed=(playerCharacter&&occupied<0)||pedestrianArmed;
    if(armed){
        texturedBox(5,19,8,11,4,5,20+variant,flash);
        texturedBox(10,17,8,4,4,4,30);
        float barrel=weapon==2?15.0f:weapon==3||weapon==4?20.0f:weapon==1?13.0f:10.0f;
        box(12+barrel*0.5f,18,8,barrel,3,3,rgb(40,43,48));
        box(12,14.5f,8,3,6,3,rgb(70,55,43));
        box(12+barrel-2,20,8,2,2,3,rgb(115,115,110));
    }else{
        texturedBox(armSwing,13,8,4,12,5,20+variant,flash);
        texturedBox(0,13,8,3,5,4,30);
    }
    texturedBox(0,25,0,9,10,9,30);
    // The atlas face is placed on the side pointing along the person's facing direction.
    textures::bind(16+variant);color(rgb(255,255,255));
    glBegin(GL_QUADS);glNormal3f(1,0,0);
    glTexCoord2f(0,0);glVertex3f(4.7f,25,-4.4f);
    glTexCoord2f(1,0);glVertex3f(4.7f,25,4.4f);
    glTexCoord2f(1,1);glVertex3f(4.7f,34,4.4f);
    glTexCoord2f(0,1);glVertex3f(4.7f,34,-4.4f);
    glEnd();textures::unbind();
    texturedBox(-1,34,0,10,3,10,28+(variant%2));
    glPopMatrix();
}
void drawVehicle(const Vehicle& v,bool driven){
    if(len(v.p-player)>800)return;
    glPushMatrix();glTranslatef(v.p.x,v.kind==Kind::Boat?-2.0f:0,v.p.z);
    glRotatef(-v.angle*180/PI,0,1,0);glRotatef(v.lean*180/PI,1,0,0);
    if(v.kind==Kind::Car||v.kind==Kind::SportCar){
        int paint=v.c.r>v.c.b?8:9;
        float bodyHeight=v.kind==Kind::SportCar?9.0f:12.0f;
        texturedBox(0,4,0,46,bodyHeight,25,paint);
        texturedBox(-3,4+bodyHeight,0,25,v.kind==Kind::SportCar?7.0f:10.0f,22,paint);
        texturedBox(8,17,0,2,8,21,10);
        texturedBox(-17,17,0,2,8,21,10);
        for(int x:{-15,15})for(int z:{-13,13})texturedBox(float(x),1,float(z),9,9,5,11);
        for(int z:{-8,8})box(23,8,float(z),1,4,5,rgb(250,228,174));
        if(v.damage>25){
            box(19,11,0,3,1,12,rgb(44,45,47));
            if(v.damage>60)box(-22,9,0,3,4,20,rgb(51,51,53));
        }
    }else if(v.kind==Kind::Bike){
        for(int x:{-12,12}){texturedBox(float(x),1,0,8,8,8,11);sphere(float(x),5,0,3,rgb(170,175,178));}
        texturedBox(0,9,0,25,4,5,9);texturedBox(3,14,0,10,5,7,9);
        box(11,17,0,3,12,4,rgb(60,62,66));
        if(driven)drawPerson({0,0},0,true);
    }else{
        texturedBox(0,2,0,48,9,24,9);texturedBox(-5,10,0,20,11,18,10);
        box(21,7,0,8,5,13,rgb(110,113,116));
    }
    glPopMatrix();
    if(v.damage>35&&ui::effectsQuality>0){
        glDisable(GL_LIGHTING);
        for(int i=0;i<4;++i){
            float drift=std::fmod(worldTime*13+i*9.0f,42.0f);
            sphere(v.p.x+forward(v.angle).x*18+std::sin(worldTime+i)*drift*0.3f,
                15+drift,v.p.z+forward(v.angle).z*18+std::cos(worldTime+i)*drift*0.3f,
                2+drift*0.13f,rgb(105,107,107));
        }
        glEnable(GL_LIGHTING);
    }
}
void drawBeacon(Vec2 p,Color c,float height=35){
    if(len(p-player)>750)return;
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(c.r,c.g,c.b,0.22f);
    glBegin(GL_QUADS);
    glVertex3f(p.x-13,0.5f,p.z);glVertex3f(p.x+13,0.5f,p.z);
    glVertex3f(p.x+13,height,p.z);glVertex3f(p.x-13,height,p.z);
    glEnd();glDisable(GL_BLEND);
    sphere(p.x,5+std::sin(worldTime*3)*2,p.z,7,c);
    glEnable(GL_LIGHTING);
}
void drawUmbrella(float x,float z,int index){
    if(len(Vec2{x,z}-player)>700)return;
    cylinder(x,0,z,1.5f,24,rgb(180,169,142));
    Color canopy=index%2?rgb(231,101,104):rgb(79,178,184);
    glBegin(GL_TRIANGLE_FAN);glNormal3f(0,1,0);color(canopy);
    glVertex3f(x,32,z);
    for(int i=0;i<=12;++i){float a=i*2*PI/12;glVertex3f(x+std::cos(a)*18,23,z+std::sin(a)*18);}
    glEnd();
    texturedBox(x+24,0,z+12,27,2,10,4);
}
void drawStreetlights(){
    float solar=std::sin((gameHour-6)*PI/12.0f);
    bool night=solar<0.1f;
    for(int c=0;c<5;++c)for(int row=0;row<7;++row){
        float x=300+c*450+68.0f,z=115+row*215.0f;
        if(len(Vec2{x,z}-player)>700)continue;
        cylinder(x,0,z,1.4f,48,rgb(78,82,84));
        box(x-7,47,z,14,1.8f,1.8f,rgb(79,83,85));
        box(x-14,46,z,6,3,6,night?rgb(255,232,147):rgb(199,198,173));
        if(night){
            glDisable(GL_LIGHTING);sphere(x-14,44,z,4,rgb(255,229,126));
            glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(1,0.76f,0.31f,0.13f);
            glBegin(GL_TRIANGLE_FAN);glVertex3f(x-14,0.5f,z);
            for(int i=0;i<=20;++i){float a=i*2*PI/20;
                glColor4f(1,0.76f,0.31f,0);glVertex3f(x-14+std::cos(a)*60,0.5f,z+std::sin(a)*60);}
            glEnd();glDisable(GL_BLEND);glEnable(GL_LIGHTING);
        }
    }
}
void drawWorld(){
    quad(0,0,WORLD_W,BEACH_START,0,rgb(94,143,92));
    texturedQuad(0,BEACH_START,WORLD_W,SHORE,0.02f,6,24,2.5f);
    texturedQuad(0,SHORE,WORLD_W,WORLD_D,-0.5f,7,24,3.5f);
    texturedQuad(0,BEACH_START-35,WORLD_W,BEACH_START,0.15f,4,24,0.4f);
    for(float pier:{300.0f,800.0f,1490.0f})texturedQuad(pier-22,SHORE-30,pier+22,SHORE+85,0.25f,4,0.5f,1.2f);
    for(int c=0;c<5;++c){
        float x=300+c*450;
        texturedQuad(x-ROAD_W/2,0,x+ROAD_W/2,BEACH_START-35,0.06f,5,1,(BEACH_START-35)/110);
    }
    for(int r=0;r<4;++r){
        float z=250+r*390;
        texturedQuad(0,z-ROAD_W/2,WORLD_W,z+ROAD_W/2,0.07f,5,WORLD_W/110,1,true);
    }
    for(size_t i=0;i<buildings.size();++i)drawBuilding(buildings[i],int(i));
    if(ui::vegetationDensity>0){
        for(int ix=0;ix<29;++ix)for(int iz=0;iz<18;++iz){
            float x=38+ix*81.0f,z=35+iz*82.0f;
            if(len(Vec2{x,z}-player)>420)continue;
            bool clear=true;
            for(const auto& b:buildings)if(x>b.x-7&&x<b.x+b.w+7&&z>b.z-7&&z<b.z+b.d+7){clear=false;break;}
            for(int road=0;road<5;++road)if(std::abs(x-(300+road*450.0f))<ROAD_W*0.56f)clear=false;
            for(int road=0;road<4;++road)if(std::abs(z-(250+road*390.0f))<ROAD_W*0.56f)clear=false;
            if(clear&&((ix+iz)%3!=0||ui::vegetationDensity>1)){
                float shift=float((ix*73+iz*41)%17)-8.0f;
                drawGrass(x+shift,z,0.65f+float((ix+iz)%4)*0.12f);
                drawGrass(x+shift+7,z+5,0.5f);
                if((ix*17+iz*29)%13==0)drawStreetTree(x,z);
            }
        }
    }
    for(int i=0;i<22;i+=ui::graphicsQuality==0?3:ui::graphicsQuality==1?2:1){
        float x=70+i*108.0f;if(len(Vec2{x,BEACH_START+47}-player)<750)drawPalm(x,BEACH_START+47);}
    for(int i=0;i<11;i+=ui::graphicsQuality==0?2:1)drawUmbrella(160+i*205.0f,1740+(i%2)*63.0f,i);
    drawStreetlights();
    for(const auto& prop:props)if(prop.alive&&len(prop.p-player)<650){
        glPushMatrix();glTranslatef(prop.p.x,prop.y,prop.p.z);
        glRotatef(prop.rotation,0,1,0);
        if(prop.barrel){
            texturedBox(0,1,0,19,23,19,9,rgb(177,183,183));
            box(0,5,0,21,2,21,rgb(74,87,92));
            box(0,18,0,21,2,21,rgb(74,87,92));
        }else{
            texturedBox(0,0,0,23,22,23,4,rgb(172,148,109));
            box(0,21,0,24,2,24,rgb(105,83,60));
        }
        glPopMatrix();
    }
    for(const auto& p:pickups)if(p.available){drawBeacon(p.p,rgb(88,228,225),28);
        if(len(p.p-player)<650)texturedBox(p.p.x,9+std::sin(worldTime*3)*2,p.p.z,14,6,5,10);}
    for(size_t i=0;i<missions.size();++i)drawBeacon(missions[i].start,
        i==0?rgb(236,193,66):i==1?rgb(231,97,133):i==2?rgb(91,174,255):
        i==3?rgb(226,135,77):i==4?rgb(200,123,255):rgb(238,213,130),42);
    if(activeMission>=0&&missionStep<int(missions[activeMission].goals.size())){
        Vec2 goal=missions[activeMission].goals[missionStep];
        bool target=missions[activeMission].kind==MissionKind::Targets||
            (missions[activeMission].kind==MissionKind::Finale&&missionStep==1);
        drawBeacon(goal,target?rgb(255,136,52):rgb(116,255,112),48);
    }
    for(const auto& ped:peds){
        if(ped.alive)drawPerson(ped.p,ped.angle,false,ped.style,ped.armed,ped.hitFlash);
        else if(ped.respawn>2&&len(ped.p-player)<650){
            texturedBox(ped.p.x,0.2f,ped.p.z,18,2,8,12,ped.shirt);
            sphere(ped.p.x+12,4,ped.p.z,4,rgb(220,184,146));
        }
    }
    for(const auto& part:debris)if(ui::effectsQuality>0&&len(Vec2{part.p.x,part.p.z}-player)<650){
        glPushMatrix();glTranslatef(part.p.x,part.p.y,part.p.z);
        glRotatef(part.rotation,0.3f,1,0.2f);
        texturedBox(0,-part.h*0.5f,0,part.w,part.h,part.d,part.tile);
        glPopMatrix();
    }
    for(size_t i=0;i<vehicles.size();++i)drawVehicle(vehicles[i],int(i)==occupied);
    if(occupied<0&&(invulnerable<=0||int(invulnerable*10)%2==0)){
        glPushMatrix();glTranslatef(0,playerY,0);
        drawPerson(player,cameraYaw,true);glPopMatrix();}
    for(const auto& bullet:bullets)if(len(Vec2{bullet.p.x,bullet.p.z}-player)<650)
        sphere(bullet.p.x,bullet.p.y,bullet.p.z,2.5f,rgb(255,230,105));
    glDisable(GL_LIGHTING);
    if(muzzleFlash>0&&occupied<0){Vec2 muzzle=player+forward(cameraYaw)*21;
        sphere(muzzle.x,16+playerY,muzzle.z,5+muzzleFlash*35,rgb(255,220,110));}
    for(const auto& impact:impacts)if(ui::effectsQuality>0&&len(impact.p-player)<650){
        float fraction=impact.life/0.7f;
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.55f,0.05f,0.05f,0.35f*fraction);
        glBegin(GL_TRIANGLE_FAN);glVertex3f(impact.p.x,0.35f,impact.p.z);
        for(int i=0;i<=12;++i){float a=i*2*PI/12;
            glVertex3f(impact.p.x+std::cos(a)*12,0.35f,impact.p.z+std::sin(a)*12);}
        glEnd();glDisable(GL_BLEND);
        for(int i=0;i<5;++i){float a=i*2*PI/5+impact.p.x;
            sphere(impact.p.x+std::cos(a)*(1-fraction)*18,7+(1-fraction)*20,
                impact.p.z+std::sin(a)*(1-fraction)*18,1.5f,rgb(180,45,45));}
    }
    glEnable(GL_LIGHTING);
}
void orthoBegin(){
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,screenW,screenH,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();glDisable(GL_DEPTH_TEST);glDisable(GL_FOG);
    glDisable(GL_LIGHTING);textures::unbind();
}
void orthoEnd(){
    glEnable(GL_DEPTH_TEST);glEnable(GL_FOG);glEnable(GL_LIGHTING);
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);
}
void screenRect(float x,float y,float w,float h,Color c){
    color(c);glBegin(GL_QUADS);glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);glEnd();
}
void text(float x,float y,const char* s,Color c){
    color(c);glRasterPos2f(x,y);glListBase(fontBase-32);glCallLists(GLsizei(std::strlen(s)),GL_UNSIGNED_BYTE,s);
}
void drawPauseMenu(){
    if(!ui::paused())return;
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.02f,0.04f,0.07f,0.72f);
    glBegin(GL_QUADS);glVertex2f(0,0);glVertex2f(float(screenW),0);
    glVertex2f(float(screenW),float(screenH));glVertex2f(0,float(screenH));glEnd();
    glDisable(GL_BLEND);
    float x=screenW/2.0f-270,y=screenH/2.0f-245;
    screenRect(x,y,540,490,rgb(29,35,43));
    text(x+28,y+45,"MINI CITY 3D",rgb(254,219,145));
    const char* title=ui::page==ui::Page::Main?"PAUSED":ui::page==ui::Page::Graphics?"GRAPHICS":
        ui::page==ui::Page::Controls?"CONTROLS":"AUDIO";
    text(x+28,y+78,title,rgb(224,234,239));
    char buffer[140];
    if(ui::page==ui::Page::Main){
        const char* items[]={"Resume","Graphics","Controls","Audio","Save game","Load game","Exit game"};
        for(int i=0;i<7;++i){float row=y+124+i*47;
            if(i==ui::selection)screenRect(x+22,row-24,496,39,rgb(78,113,128));
            text(x+42,row,items[i],i==ui::selection?rgb(255,239,170):rgb(223,228,230));
        }
    }else if(ui::page==ui::Page::Graphics){
        const char* q[]={"Low","Medium","High"};const char* sizes[]={"1280 x 720","1600 x 900","1920 x 1080"};
        const char* shadows[]={"Off","Medium","High"};
        const char* items[]={q[ui::graphicsQuality],sizes[ui::windowChoice],q[ui::vegetationDensity],q[ui::effectsQuality],shadows[ui::shadowQuality]};
        const char* labels[]={"Scene quality","Window size","Vegetation","Effects","Shadows (DX11)"};
        for(int i=0;i<5;++i){float row=y+130+i*58;
            if(i==ui::selection)screenRect(x+22,row-25,496,39,rgb(78,113,128));
            std::snprintf(buffer,sizeof(buffer),"%s:  < %s >",labels[i],items[i]);
            text(x+42,row,buffer,rgb(235,235,225));}
    }else if(ui::page==ui::Page::Controls){
        const char* labels[]={"Mouse sensitivity","Invert vertical mouse","Forward","Backward","Left","Right","Sprint","Interact"};
        for(int i=0;i<8;++i){float row=y+116+i*39;
            if(i==ui::selection)screenRect(x+22,row-23,496,33,rgb(78,113,128));
            if(i==0)std::snprintf(buffer,sizeof(buffer),"%s:  < %d >",labels[i],ui::mouseSensitivity);
            else if(i==1)std::snprintf(buffer,sizeof(buffer),"%s:  < %s >",labels[i],ui::invertY?"On":"Off");
            else std::snprintf(buffer,sizeof(buffer),"%s:  %s",labels[i],ui::keyName(ui::bindings[i-2]));
            text(x+42,row,buffer,rgb(235,235,225));
        }
        if(ui::waitingForBinding>=0)text(x+42,y+453,"Press a new key (Esc cancels)",rgb(255,220,126));
    }else{
        std::snprintf(buffer,sizeof(buffer),"Master volume:  < %d%% >",ui::masterVolume);
        screenRect(x+22,y+105,496,39,rgb(78,113,128));
        text(x+42,y+132,buffer,rgb(235,235,225));
    }
    text(x+27,y+466,"Arrow keys: select/change    Enter: choose    Esc: back",rgb(192,202,209));
}
void drawMap(float mx,float my,float mw,float mh,bool large){
    screenRect(mx-3,my-3,mw+6,mh+6,rgb(23,28,35));
    screenRect(mx,my,mw,mh,rgb(111,145,106));
    auto mapX=[&](float x){return mx+x/WORLD_W*mw;};
    auto mapY=[&](float z){return my+z/WORLD_D*mh;};
    screenRect(mx,mapY(BEACH_START),mw,mapY(SHORE)-mapY(BEACH_START),rgb(222,200,151));
    screenRect(mx,mapY(SHORE),mw,mh-(mapY(SHORE)-my),rgb(52,127,174));
    for(int c=0;c<5;++c)screenRect(mapX(300+c*450-ROAD_W/2),my,ROAD_W/WORLD_W*mw,
        BEACH_START/WORLD_D*mh,rgb(77,79,84));
    for(int r=0;r<4;++r)screenRect(mx,mapY(250+r*390-ROAD_W/2),mw,ROAD_W/WORLD_D*mh,rgb(77,79,84));
    if(large)for(const auto& b:buildings)screenRect(mapX(b.x),mapY(b.z),
        std::max(2.0f,b.w/WORLD_W*mw),std::max(2.0f,b.d/WORLD_D*mh),rgb(153,136,120));
    for(const auto& pickup:pickups)if(pickup.available)
        screenRect(mapX(pickup.p.x)-3,mapY(pickup.p.z)-3,6,6,rgb(91,245,234));
    if(activeMission>=0&&missionStep<int(missions[activeMission].goals.size())){
        Vec2 goal=missions[activeMission].goals[missionStep];
        color(rgb(179,255,124));glLineWidth(large?3.0f:2.0f);
        glBegin(GL_LINES);glVertex2f(mapX(player.x),mapY(player.z));
        glVertex2f(mapX(goal.x),mapY(goal.z));glEnd();glLineWidth(1);
    }else if(nextMission()>=0){
        Vec2 start=missions[nextMission()].start;
        color(rgb(255,211,119));glLineWidth(large?3.0f:2.0f);
        glBegin(GL_LINES);glVertex2f(mapX(player.x),mapY(player.z));
        glVertex2f(mapX(start.x),mapY(start.z));glEnd();glLineWidth(1);
    }
    for(size_t i=0;i<missions.size();++i){
        Color c=i==0?rgb(246,206,79):i==1?rgb(241,101,145):i==2?rgb(96,180,255):
            i==3?rgb(239,146,78):rgb(204,131,255);
        if(missionDone[i])c=rgb(130,209,141);
        else if(int(i)!=nextMission())c=rgb(110,119,125);
        screenRect(mapX(missions[i].start.x)-4,mapY(missions[i].start.z)-4,8,8,c);
    }
    if(activeMission>=0&&missionStep<int(missions[activeMission].goals.size())){
        Vec2 goal=missions[activeMission].goals[missionStep];
        screenRect(mapX(goal.x)-5,mapY(goal.z)-5,10,10,rgb(128,255,115));
    }
    screenRect(mapX(player.x)-4,mapY(player.z)-4,8,8,rgb(255,245,105));
    if(large){
        text(mapX(370),mapY(120),"DOWNTOWN",rgb(243,239,209));
        text(mapX(1370),mapY(BEACH_START+125),"BEACH",rgb(109,78,48));
        text(mapX(1270),mapY(SHORE+90),"HARBOR",rgb(241,245,246));
    }
}
void drawHud(){
    orthoBegin();
    screenRect(12,12,590,103,rgb(27,32,39));
    char buffer[220];
    int hour=int(gameHour),minute=int((gameHour-hour)*60);
    std::snprintf(buffer,sizeof(buffer),"HEALTH %d    $%d    %02d:%02d    %s  %d / %s",int(health),money,hour,minute,
                  weaponNames[weapon],magazine[weapon],ammo[weapon]<0?"INF":std::to_string(ammo[weapon]).c_str());
    text(24,34,buffer,rgb(245,244,231));
    screenRect(24,45,230,15,rgb(104,55,55));screenRect(24,45,230*health/100,15,rgb(86,211,107));
    if(occupied>=0){const char* name=vehicles[occupied].kind==Kind::Boat?"BOAT":vehicles[occupied].kind==Kind::Bike?"BIKE":
        vehicles[occupied].kind==Kind::SportCar?"SPORT CAR":"CAR";
        std::snprintf(buffer,sizeof(buffer),"%s  |  CONDITION %d%%  |  WASD drive  |  E exit",
            name,int(100-vehicles[occupied].damage));
    }else std::snprintf(buffer,sizeof(buffer),"WASD move  |  SHIFT run  |  RMB aim  |  LMB shoot  |  R reload  |  E enter");
    text(24,83,buffer,rgb(226,230,229));
    text(24,105,"1-5 weapons  |  Q cycle  |  F mission  |  M map  |  T time",rgb(206,215,214));
    if(reloadRemaining>0)text(263,60,"RELOADING",rgb(255,214,113));
    if(activeMission>=0){
        const auto& m=missions[activeMission];
        screenRect(screenW-352,12,340,78,rgb(27,32,39));
        std::snprintf(buffer,sizeof(buffer),"%s    %d sec",m.name,int(missionTime));
        text(screenW-339,34,buffer,rgb(252,222,140));
        text(screenW-339,58,missionObjective(),rgb(224,229,225));
        std::snprintf(buffer,sizeof(buffer),"Objective %d / %d",missionStep+1,int(m.goals.size()));
        text(screenW-339,80,buffer,rgb(201,220,205));
    }else for(const auto& m:missions)if(len(m.start-player)<75){
        screenRect(screenW-345,12,333,55,rgb(27,32,39));
        std::snprintf(buffer,sizeof(buffer),"Press F: %s",m.name);
        text(screenW-331,44,buffer,rgb(250,218,125));break;
    }
    if(activeMission<0&&nextMission()>=0){
        screenRect(screenW-345,72,333,45,rgb(27,32,39));
        std::snprintf(buffer,sizeof(buffer),"NEXT: %s",missions[nextMission()].name);
        text(screenW-331,100,buffer,rgb(250,218,125));
    }
    // Center reticle points in the horizontal firing direction.
    if(occupied<0&&rightMouse&&!ui::paused()){float cx=screenW/2.0f,cy=screenH/2.0f;
        float kick=recoil*9.0f;
        screenRect(cx-10-kick,cy-1,6,2,rgb(251,245,218));screenRect(cx+4+kick,cy-1,6,2,rgb(251,245,218));
        screenRect(cx-1,cy-10-kick,2,6,rgb(251,245,218));screenRect(cx-1,cy+4+kick,2,6,rgb(251,245,218));}
    drawMap(18,screenH-166,180,140,false);
    if(messageTime>0){
        screenRect(screenW/2-310,screenH-72,620,45,rgb(27,32,39));
        text(screenW/2-296,screenH-43,message.c_str(),rgb(255,231,163));
    }
    if(showMap){
        float mw=std::min(float(screenW-120),650.0f),mh=mw*WORLD_D/WORLD_W;
        if(mh>screenH-125){mh=float(screenH-125);mw=mh*WORLD_W/WORLD_D;}
        float mx=(screenW-mw)/2,my=(screenH-mh)/2;
        screenRect(mx-26,my-48,mw+52,mh+100,rgb(25,30,37));
        text(mx,my-18,"CITY MAP  -  yellow: you  green: objective  cyan: guns",rgb(243,236,216));
        drawMap(mx,my,mw,mh,true);
        text(mx,my+mh+26,"Colored squares: missions.  Press M to close.",rgb(224,229,226));
    }
    if(health<=0){screenRect(screenW/2-155,screenH/2-45,310,90,rgb(28,32,39));
        text(screenW/2-58,screenH/2-9,"YOU DIED",rgb(251,117,110));
        text(screenW/2-80,screenH/2+20,"Press R to restart",rgb(245,245,235));}
    if(debugHud){
        screenRect(screenW-265,screenH-125,252,112,rgb(28,34,39));
        std::snprintf(buffer,sizeof(buffer),"FPS %.0f  FRAME %.1f ms",frameRate,frameMs);
        text(screenW-253,screenH-100,buffer,rgb(220,245,220));
        std::snprintf(buffer,sizeof(buffer),"SIM %.2f  PHYS %.2f ms",simulationMs,physicsMs);
        text(screenW-253,screenH-75,buffer,rgb(220,245,220));
        std::snprintf(buffer,sizeof(buffer),"ACTIVE AI %d  PROPS %d",activeAi,int(props.size()));
        text(screenW-253,screenH-50,buffer,rgb(220,245,220));
    }
    drawPauseMenu();
    orthoEnd();
}
void setupLighting(){
    float solar=std::sin((gameHour-6)*PI/12.0f);
    float daylight=std::clamp(solar*2.3f+0.42f,0.0f,1.0f);
    float skyR=0.045f+0.52f*daylight,skyG=0.065f+0.68f*daylight,skyB=0.14f+0.74f*daylight;
    glClearColor(skyR,skyG,skyB,1);
    GLfloat fog[]={skyR,skyG,skyB,1};glFogfv(GL_FOG_COLOR,fog);
    float fogEnd=ui::graphicsQuality==0?650.0f:ui::graphicsQuality==1?850.0f:1050.0f;
    glFogf(GL_FOG_START,fogEnd*0.42f);glFogf(GL_FOG_END,fogEnd);
    GLfloat ambient[]={0.16f+0.44f*daylight,0.18f+0.40f*daylight,0.24f+0.32f*daylight,1};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);
    GLfloat sunColor[]={0.22f+0.80f*daylight,0.27f+0.62f*daylight,0.43f+0.34f*daylight,1};
    GLfloat sunPosition[]={std::cos((gameHour-6)*PI/12)*700,std::max(100.0f,std::abs(solar)*700),-280,0};
    glLightfv(GL_LIGHT0,GL_DIFFUSE,sunColor);glLightfv(GL_LIGHT0,GL_POSITION,sunPosition);glEnable(GL_LIGHT0);
    struct Lamp{float distance,x,z;};std::vector<Lamp> close;
    if(daylight<0.45f){
        for(int c=0;c<5;++c)for(int row=0;row<7;++row){
            float x=300+c*450+54.0f,z=115+row*215.0f;
            float d=len(Vec2{x,z}-player);
            if(d<360)close.push_back({d,x,z});
        }
        std::sort(close.begin(),close.end(),[](const Lamp& a,const Lamp& b){return a.distance<b.distance;});
    }
    for(int i=0;i<7;++i){GLenum light=GLenum(GL_LIGHT1+i);
        if(i<int(close.size())){
            GLfloat position[]={close[i].x,44,close[i].z,1};
            GLfloat diffuse[]={1.0f,0.76f,0.36f,1};
            glLightfv(light,GL_POSITION,position);glLightfv(light,GL_DIFFUSE,diffuse);
            glLightf(light,GL_CONSTANT_ATTENUATION,1);
            glLightf(light,GL_LINEAR_ATTENUATION,0.009f);
            glLightf(light,GL_QUADRATIC_ATTENUATION,0.00018f);
            glEnable(light);
        }else glDisable(light);
    }
}
void drawSkyBody(){
    float solar=std::sin((gameHour-6)*PI/12.0f),angle=(gameHour-6)*PI/12;
    bool sun=solar>=0;float direction=sun?1.0f:-1.0f;
    float x=player.x+std::cos(angle)*direction*820;
    float y=std::abs(solar)*600+110;
    glDisable(GL_DEPTH_TEST);glDisable(GL_FOG);glDisable(GL_LIGHTING);
    sphere(x,y,player.z-160,sun?48.0f:32.0f,sun?rgb(255,226,147):rgb(222,232,248));
    glEnable(GL_LIGHTING);glEnable(GL_FOG);glEnable(GL_DEPTH_TEST);
}
void render(){
    glViewport(0,0,screenW,screenH);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();
    gluPerspective(65.0,double(screenW)/std::max(1,screenH),2.0,1250.0);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    Vec2 focus=previousPlayer*(1.0f-renderAlpha)+player*renderAlpha;
    bool aiming=rightMouse&&occupied<0&&!ui::paused();
    camera::Pose pose=camera::compute(focus,playerY,aiming,occupied);
    gluLookAt(pose.eye.x,pose.eye.y,pose.eye.z,
              pose.target.x,pose.target.y,pose.target.z,0,1,0);
    setupLighting();glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    drawSkyBody();drawWorld();drawHud();SwapBuffers(dc);
}
bool initRenderer(){
    dc=GetDC(win);PIXELFORMATDESCRIPTOR pfd{};pfd.nSize=sizeof(pfd);pfd.nVersion=1;
    pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=32;pfd.cDepthBits=24;pfd.iLayerType=PFD_MAIN_PLANE;
    int format=ChoosePixelFormat(dc,&pfd);if(!format||!SetPixelFormat(dc,format,&pfd))return false;
    glrc=wglCreateContext(dc);if(!glrc||!wglMakeCurrent(dc,glrc))return false;
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDisable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);glEnable(GL_COLOR_MATERIAL);glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glClearColor(0.56f,0.75f,0.88f,1);
    GLfloat fog[]={0.56f,0.75f,0.88f,1};glFogfv(GL_FOG_COLOR,fog);
    glFogi(GL_FOG_MODE,GL_LINEAR);glFogf(GL_FOG_START,430);glFogf(GL_FOG_END,1050);glEnable(GL_FOG);
    HFONT f=CreateFontA(-17,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,"Arial");
    HGDIOBJ old=SelectObject(dc,f);fontBase=glGenLists(96);wglUseFontBitmapsA(dc,32,96,fontBase);
    SelectObject(dc,old);DeleteObject(f);return textures::load();
}
void shutdownRenderer(){
    textures::shutdown();
    if(fontBase)glDeleteLists(fontBase,96);
    wglMakeCurrent(nullptr,nullptr);
    if(glrc)wglDeleteContext(glrc);
    if(dc)ReleaseDC(win,dc);
}

} // namespace game


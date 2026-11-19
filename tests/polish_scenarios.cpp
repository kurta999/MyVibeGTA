// Unit scenarios for the frame pacing, key binding and HUD compositing fixes.
#include "../src/frame_pacing.h"
#include "../src/key_bindings.h"
#include "../src/hud_pixels.h"
#include <cmath>
#include <cstdio>
#include <vector>

int main(){
    int failures=0;
    auto check=[&](bool ok,const char* name){if(!ok){++failures;std::printf("FAIL %s\n",name);}};
    constexpr double step=1.0/60.0;

    // Frame pacing: normal frames consume whole steps and keep the remainder.
    {
        double accumulator=step*2.5;
        auto plan=frame_pacing::consume(accumulator,step,5);
        check(plan.steps==2&&!plan.dropped,"two steps consumed");
        check(std::abs(accumulator-step*0.5)<1e-9,"fraction kept after normal frame");
    }
    // Exactly reaching the cap is not a dropped frame and keeps its fraction.
    {
        double accumulator=step*5.4;
        auto plan=frame_pacing::consume(accumulator,step,5);
        check(plan.steps==5&&!plan.dropped,"cap reached without backlog");
        check(std::abs(accumulator-step*0.4)<1e-9,"fraction kept at cap");
    }
    // A real backlog is dropped, but interpolation keeps a valid fraction.
    {
        double accumulator=step*9.25;
        auto plan=frame_pacing::consume(accumulator,step,5);
        check(plan.steps==5&&plan.dropped,"backlog reported");
        check(accumulator>=0&&accumulator<step,"backlog trimmed below one step");
        check(std::abs(accumulator-step*0.25)<1e-9,"backlog keeps fraction");
    }
    {
        double accumulator=-1;
        auto plan=frame_pacing::consume(accumulator,step,5);
        check(plan.steps==0&&accumulator==0,"negative accumulator clamped");
        check(frame_pacing::alpha(step*0.5,step)==0.5f,"alpha midpoint");
        check(frame_pacing::alpha(step*3,step)==1.0f,"alpha clamped");
    }

    // Key bindings: invalid and reserved keys fall back to defaults.
    {
        const std::array<int,6> defaults{{'W','S','A','D',0x10,'E'}};
        std::array<int,6> keys{{0,'S',0x1B,'D',300,'E'}};
        check(key_bindings::sanitize(keys,defaults),"sanitize reports changes");
        check(keys==defaults,"invalid keys restored");
        // Duplicates are resolved so each action keeps a distinct key.
        keys={{'W','W','A','D',0x10,'E'}};
        key_bindings::sanitize(keys,defaults);
        check(keys[0]=='W'&&keys[1]=='S',"duplicate restored to default");
        keys={{'S','S','A','D',0x10,'E'}};
        key_bindings::sanitize(keys,defaults);
        bool distinct=true;
        for(int i=0;i<6;++i)for(int j=0;j<i;++j)distinct&=keys[i]!=keys[j];
        check(distinct,"duplicates never survive sanitize");
        check(!key_bindings::sanitize(keys,defaults),"sanitized bindings are stable");
        // Assigning a used key swaps the two actions.
        keys=defaults;
        check(key_bindings::assign(keys,0,'S'),"assign accepted");
        check(keys[0]=='S'&&keys[1]=='W',"assign swaps conflicting action");
        check(!key_bindings::assign(keys,2,0x1B),"escape cannot be bound");
        check(!key_bindings::assign(keys,2,0x73),"F4 cannot be bound");
        check(!key_bindings::assign(keys,9,'Z'),"out of range action rejected");
    }

    // HUD compositing: black is transparent, drawn pixels become opaque.
    {
        std::vector<unsigned char> source{0,0,0,0, 10,20,30,0, 0,0,1,0, 255,255,255,0};
        std::vector<unsigned char> output(source.size(),77);
        hud_pixels::finalize(source.data(),output.data(),4);
        check(output[3]==0&&output[0]==0,"black stays transparent");
        check(output[4]==10&&output[5]==20&&output[6]==30&&output[7]==255,"drawn pixel opaque");
        check(output[11]==255,"dim pixel opaque");
        check(output[15]==255&&output[12]==255,"white pixel opaque");
    }
    // Damage vignette: tints edges, leaves the centre and opaque widgets alone.
    {
        const int width=160,height=90;
        std::vector<unsigned char> pixels(std::size_t(width)*height*4,0);
        unsigned char* widget=&pixels[(std::size_t(2)*width+2)*4];
        widget[0]=1;widget[1]=2;widget[2]=3;widget[3]=255;
        hud_pixels::applyDamageVignette(pixels.data(),width,height,1.0f);
        const unsigned char* corner=&pixels[(std::size_t(height-1)*width+width-1)*4];
        const unsigned char* centre=&pixels[(std::size_t(height/2)*width+width/2)*4];
        check(corner[3]>100&&corner[2]>corner[1],"corner tinted red");
        check(centre[3]==0,"centre stays clear");
        check(widget[0]==1&&widget[3]==255,"opaque widget untouched");
        std::vector<unsigned char> untouched(std::size_t(width)*height*4,0);
        hud_pixels::applyDamageVignette(untouched.data(),width,height,0.0f);
        bool clear=true;for(auto value:untouched)clear&=value==0;
        check(clear,"zero intensity is a no-op");
        check(hud_pixels::vignette(width/2,height/2,width,height,1)==0,"vignette centre zero");
    }
    std::printf("Polish scenarios: %d failures\n",failures);
    return failures?1:0;
}

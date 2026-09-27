#include "../src/dx11_texture_mips.h"
#include <cmath>
#include <cstdint>
#include <cstdio>

using dx11::texture::Kind;
using dx11::texture::generate;
int main(){
    // Average of black and white in linear light encodes near sRGB 188.
    const std::uint8_t color[]={0,0,0,255,255,255,255,255};
    auto colorMips=generate(2,1,color,Kind::Color);
    if(colorMips.size()!=2||colorMips[1].width!=1||colorMips[1].height!=1||
       std::abs(int(colorMips[1].pixels[0])-188)>2){
        std::puts("linear-light color mip failed");return 1;
    }
    // Two opposing perturbations should normalize to a flat normal while
    // retaining their variance in alpha for roughness adjustment.
    const std::uint8_t normals[]={128,128,255,255,128,128,0,255};
    auto normalMips=generate(2,1,normals,Kind::Normal);
    if(normalMips.size()!=2||normalMips[1].pixels[0]<245||
       normalMips[1].pixels[3]>20){
        std::puts("normal mip renormalization failed");return 1;
    }
    const std::uint8_t cutout[]={255,255,255,255,0,0,0,0,
                                 0,0,0,0,255,255,255,255};
    auto maskedMips=generate(2,2,cutout,Kind::MaskedColor);
    if(maskedMips.size()!=2||maskedMips[1].pixels[3]<128||
       maskedMips[1].pixels[0]<200){
        std::puts("cutout alpha or edge color failed");return 1;
    }
    const std::uint8_t odd[]={0,0,0,255,0,0,0,255,255,255,255,255};
    auto oddMips=generate(3,1,odd,Kind::Linear);
    if(oddMips.size()!=2||std::abs(int(oddMips[1].pixels[0])-85)>1){
        std::puts("odd-size mip missed edge texels");return 1;
    }
    std::puts("texture mip checks passed");
}

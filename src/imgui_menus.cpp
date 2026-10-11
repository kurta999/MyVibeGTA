#include "imgui_menus.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include "game.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "commerce.h"
#include "debug_menu.h"
#include "weather.h"
#include "weapons.h"
#include "builder.h"
#include "ordnance.h"
#include "savegame.h"
#include "dx11_assets.h"
#include <d3d11.h>
#include <gdiplus.h>
#include <wrl/client.h>
#include <filesystem>
#include <map>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <cstring>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace menus {
namespace {
bool initialized=false;
ID3D11Device* device=nullptr;
std::map<int,Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> icons;
bool reviewing=false;
std::map<std::string,ImVec2> reviewItems;
struct ReviewEvent {ImVec2 p;bool down,right,shift;};
std::vector<ReviewEvent> reviewEvents;
ImVec2 reviewPoint{};
void Record(const std::string& label){
    if(!reviewing||!ImGui::IsItemVisible())return;
    auto low=ImGui::GetItemRectMin(),high=ImGui::GetItemRectMax();
    reviewItems[label]={(low.x+high.x)*.5f,(low.y+high.y)*.5f};
}
bool Button(const char* label,ImVec2 size){bool result=ImGui::Button(label,size);Record(label);return result;}
bool SliderInt(const char* label,int* value,int low,int high,const char* format="%d"){
    bool changed=ImGui::SliderInt(label,value,low,high,format);Record(label);return changed;
}
bool Checkbox(const char* label,bool* value){bool changed=ImGui::Checkbox(label,value);Record(label);return changed;}
bool Combo(const char* label,int* value,const char* items){
    std::vector<const char*> entries;
    for(auto item=items;*item;item+=std::strlen(item)+1)entries.push_back(item);
    bool changed=false;
    bool opened=ImGui::BeginCombo(label,entries[std::clamp(*value,0,int(entries.size())-1)]);Record(label);
    if(opened){
        for(int i=0;i<int(entries.size());++i){
            bool selected=*value==i;
            if(ImGui::Selectable(entries[i],selected)){*value=i;changed=true;}
            Record(std::string(label)+"/"+entries[i]);
            if(selected)ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}
bool Modal(){return ui::paused()||debug_menu::open||commerce::menu()!=commerce::Menu::None||builder::modal()||ordnance::timerOpen();}
void Help(){
    if(!ui::showHelp||Modal())return;
    const auto& io=ImGui::GetIO();
    ImGui::SetNextWindowPos({12,86},ImGuiCond_Always);
    ImGui::SetNextWindowSize({std::min(760.0f,io.DisplaySize.x-24),0},ImGuiCond_Always);
    if(ImGui::Begin("Controls - F1",nullptr,ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize)){
        // keyName uses a shared character buffer, so keep a copy of each label.
        std::array<std::string,6> keys;
        for(int i=0;i<int(keys.size());++i)keys[i]=ui::keyName(ui::bindings[i]);
        ImGui::TextWrapped("Move: %s / %s / %s / %s | Sprint: %s | Vehicle: %s",
            keys[0].c_str(),keys[1].c_str(),keys[2].c_str(),keys[3].c_str(),keys[4].c_str(),keys[5].c_str());
        ImGui::TextWrapped("Alt slow | Ctrl crouch | Right mouse aim | Left mouse fire | R reload");
        ImGui::TextWrapped("1-9 / Q weapons | C camera | B telescope | F interact | Tab choose | G carry");
        ImGui::TextWrapped("F4 debug | F5 builder | X C4 detonate | K knife takedown | Esc pause");
        ImGui::TextWrapped("Builder: E inventory | Left mouse mine | Right mouse place | R repair | Shift click transfer");
        ImGui::TextWrapped("Swimming: Left mouse dive / Right mouse surface | Driving: Space drift, H lights, mouse wheel radio");
    }
    ImGui::End();
}
bool Window(const char* title,float width,float height){
    const auto& io=ImGui::GetIO();
    ImGui::SetNextWindowPos({io.DisplaySize.x*.5f,io.DisplaySize.y*.5f},ImGuiCond_Always,{.5f,.5f});
    ImGui::SetNextWindowSize({std::min(width,io.DisplaySize.x-24),std::min(height,io.DisplaySize.y-24)},ImGuiCond_Always);
    return ImGui::Begin(title,nullptr,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings);
}
void Back(){if(Button("Back",{-1,32})){ui::waitingForBinding=-1;ui::page=ui::Page::Main;}}
void Pause(){
    if(!ui::paused())return;
    const char* title=ui::page==ui::Page::Main?"Mini City 3D - Paused":ui::page==ui::Page::Graphics?"Graphics":ui::page==ui::Page::Controls?"Controls":"Audio";
    if(Window(title,600,ui::page==ui::Page::Graphics?840:500)){
        bool changed=false;
        ImGui::PushItemWidth(-220);
        if(ui::page==ui::Page::Main){
            for(int i=0;i<7;++i){
                const char* labels[]={"Resume","Graphics","Controls","Audio","Save game","Load game","Exit game"};
                if(Button(labels[i],{-1,45})){ui::selection=i;ui::handleKey(VK_RETURN);}
                if(i==0)ImGui::SetItemDefaultFocus();
            }
        }else if(ui::page==ui::Page::Graphics){
            ImGui::BeginChild("graphics options",{0,-45},ImGuiChildFlags_None);
            changed|=Combo("Scene quality",&ui::graphicsQuality,"Low\0Medium\0High\0");
            if(Combo("Window size",&ui::windowChoice,"1280 x 720\0" "1600 x 900\0" "1920 x 1080\0")){ui::applyWindow();changed=true;}
            if(Combo("Display mode",&ui::windowMode,"Windowed\0Borderless\0Fullscreen\0")){ui::applyWindow();changed=true;}
            changed|=Combo("Vegetation",&ui::vegetationDensity,"Low\0Medium\0High\0");
            changed|=Combo("Effects",&ui::effectsQuality,"Low\0Medium\0High\0");
            changed|=Combo("Shadows",&ui::shadowQuality,"Off\0Medium\0High\0");
            changed|=Combo("Reflections (SSR)",&ui::reflectionQuality,"Off\0Medium\0High\0");
            changed|=Combo("Anti-aliasing",&ui::antiAliasingQuality,"FXAA Low\0FXAA\0TAA\0");
            changed|=Combo("Ambient occlusion",&ui::aoQuality,"Off\0Medium\0High\0");
            changed|=Combo("Texture quality",&ui::textureQuality,"Low\0Medium\0High\0");
            changed|=Combo("Anisotropic filtering",&ui::filteringQuality,"4x\0" "8x\0" "16x\0");
            changed|=SliderInt("Draw distance",&ui::drawDistance,0,100,"%d%%");
            ImGui::TextDisabled("Visible distance: %.0f units",1250*ui::drawDistanceScale());
            changed|=SliderInt("LOD distance",&ui::lodDistance,0,100,"%d%%");
            changed|=SliderInt("Grass distance",&ui::grassDistance,0,100,"%d%%");
            ImGui::TextDisabled("Grass: %.0f units (0 disables grass)",ui::grassDistanceUnits());
            changed|=SliderInt("Grass LOD",&ui::grassLodDistance,0,100,"%d%%");
            ImGui::TextDisabled("Detailed grass: %.0f units",ui::grassLodDistanceUnits());
            ImGui::EndChild();
            Back();
        }else if(ui::page==ui::Page::Controls){
            changed|=SliderInt("Mouse sensitivity",&ui::mouseSensitivity,1,20);
            changed|=Checkbox("Invert vertical mouse",&ui::invertY);
            const char* labels[]={"Forward","Backward","Left","Right","Sprint","Enter / exit vehicle"};
            for(int i=0;i<int(ui::bindings.size());++i){
                ImGui::PushID(i);ImGui::TextUnformatted(labels[i]);ImGui::SameLine(300);
                std::string key=ui::waitingForBinding==i?"Press a key...":ui::keyName(ui::bindings[i]);
                if(Button(key.c_str(),{180,28}))ui::waitingForBinding=i;
                ImGui::PopID();
            }
            ImGui::TextWrapped("F interacts with people, shops and mission markers. Escape cancels key binding.");
            Back();
        }else{
            if(SliderInt("Master volume",&ui::masterVolume,0,100,"%d%%")){audio::setVolume(ui::masterVolume);changed=true;}
            Back();
        }
        ImGui::PopItemWidth();if(changed)ui::save();
    }
    ImGui::End();
}
void Commerce(){
    if(commerce::menu()==commerce::Menu::None||ui::paused())return;
    if(Window(commerce::menuTitle().c_str(),650,600)){
        ImGui::Text("Cash $%d | Houses %d / 10",game::money,commerce::ownedHouseCount());
        ImGui::BeginChild("stock",{0,-45},ImGuiChildFlags_None);
        auto entries=commerce::menuEntries();
        for(int i=0;i<int(entries.size());++i){
            ImGui::PushID(i);ImGui::BeginDisabled(!entries[i].available);
            std::string label=entries[i].label+(entries[i].price>0?"  $"+std::to_string(entries[i].price):"");
            if(Button(label.c_str(),{-1,32}))commerce::activate(i);
            ImGui::EndDisabled();ImGui::PopID();
        }
        ImGui::EndChild();
        if(Button("Close",{-1,32}))commerce::close();
    }
    ImGui::End();
}
void DebugAction(const char* label,int row){if(Button(label,{-1,30})){debug_menu::selection=row;debug_menu::handleKey(VK_RETURN);}}
void Debug(){
    if(!debug_menu::open||ui::paused())return;
    if(Window("Debug - F4",650,640)){
        ImGui::BeginChild("actions",{0,-45},ImGuiChildFlags_None);
        Checkbox("God mode",&debug_menu::godMode);
        bool fly=debug_menu::flyMode;if(Checkbox("Fly (Space up, Ctrl down)",&fly)){debug_menu::selection=1;debug_menu::handleKey(VK_RETURN);}
        DebugAction("Restore health",2);
        ImGui::SliderFloat("Time of day",&game::gameHour,0,23.99f,"%.2f");
        if(ImGui::BeginCombo("Weather",weather::current().name.c_str())){
            for(const auto& state:weather::states())if(ImGui::Selectable(state.name.c_str(),state.id==weather::current().id))weather::set(state.id);
            ImGui::EndCombo();
        }
        bool unlimited=debug_menu::infiniteAmmo;
        if(Checkbox("Infinite ammo",&unlimited)){debug_menu::selection=5;debug_menu::handleKey(VK_RETURN);}
        DebugAction("Add $100,000",6);DebugAction("Reset wanted level",7);DebugAction("Add 1,000 ammo to all weapons",8);
        ImGui::SeparatorText("Weapons and tools");
        for(int i=0;i<weapons::count();++i){ImGui::PushID(i);DebugAction(weapons::stats(i).name.c_str(),debug_menu::WEAPONS_START+i);ImGui::PopID();}
        ImGui::EndChild();
        if(Button("Close",{-1,32}))debug_menu::open=false;
    }
    ImGui::End();
}
ID3D11ShaderResourceView* Icon(int item){
    if(item<0)return nullptr;
    auto found=icons.find(item);if(found!=icons.end())return found->second.Get();
    auto& view=icons[item];const auto& entry=builder::items()[item];const auto* mesh=dx11::mesh("builder/"+entry.id);if(!mesh)return nullptr;
    auto file=std::filesystem::path(mesh->textureFile).parent_path()/(entry.id+".icon.png");
    Gdiplus::Bitmap image(file.c_str());if(image.GetLastStatus()!=Gdiplus::Ok)return nullptr;
    Gdiplus::Rect area(0,0,image.GetWidth(),image.GetHeight());Gdiplus::BitmapData pixels{};
    if(image.LockBits(&area,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&pixels)!=Gdiplus::Ok)return nullptr;
    D3D11_TEXTURE2D_DESC desc{};desc.Width=image.GetWidth();desc.Height=image.GetHeight();desc.MipLevels=desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA source{pixels.Scan0,UINT(pixels.Stride),0};Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HRESULT result=device->CreateTexture2D(&desc,&source,&texture);image.UnlockBits(&pixels);
    if(SUCCEEDED(result))device->CreateShaderResourceView(texture.Get(),nullptr,&view);return view.Get();
}
void Stack(const builder::Stack& stack,int index){
    ImGui::PushID(index);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(4,4));
    auto icon=Icon(stack.item);bool activated;
    if(icon)activated=ImGui::ImageButton("slot",ImTextureRef(ImTextureID(reinterpret_cast<uintptr_t>(icon))),{44,44});
    else activated=Button("##slot",{52,52});
    Record("slot-"+std::to_string(index));
    if(activated)builder::clickSlot(index,false,ImGui::GetIO().KeyShift);
    if(ImGui::IsItemClicked(ImGuiMouseButton_Right))builder::clickSlot(index,true,ImGui::GetIO().KeyShift);
    if(stack.item>=0){
        const auto& item=builder::items()[stack.item];
        if(ImGui::IsItemHovered()){
            ImGui::BeginTooltip();ImGui::TextUnformatted(item.name.c_str());
            if(item.durability)ImGui::Text("Durability: %d / %d | Tier %d",stack.durability,item.durability,item.tier);
            else ImGui::Text("Count: %d",stack.count);
            ImGui::TextUnformatted("Right click: split/place one | Shift click: transfer");ImGui::EndTooltip();
        }
        auto low=ImGui::GetItemRectMin(),high=ImGui::GetItemRectMax();
        auto* draw=ImGui::GetWindowDrawList();
        std::string amount=std::to_string(stack.count);draw->AddText({high.x-20,high.y-18},IM_COL32_WHITE,amount.c_str());
        if(item.durability){float ratio=float(stack.durability)/item.durability;
            draw->AddRectFilled({low.x+3,high.y-5},{low.x+3+(high.x-low.x-6)*ratio,high.y-2},IM_COL32(110,224,152,255));}
    }
    ImGui::PopStyleVar();ImGui::PopID();
}
void Inventory(){
    if(!builder::inventoryOpen()||ui::paused())return;
    if(Window("Builder inventory and storage",1000,660)){
        ImGui::BeginChild("slots",{560,-45},ImGuiChildFlags_None);
        if(auto chest=builder::chest()){
            ImGui::SeparatorText("Chest / processing storage");
            for(int i=0;i<27;++i){if(i%9)ImGui::SameLine();Stack((*chest)[i],i+36);}
        }
        ImGui::SeparatorText("Inventory");
        for(int i=9;i<36;++i){if((i-9)%9)ImGui::SameLine();Stack(builder::inventory()[i],i);}
        ImGui::SeparatorText("Hotbar (1-9)");
        for(int i=0;i<9;++i){if(i%9)ImGui::SameLine();Stack(builder::inventory()[i],i);}
        const auto& cursor=builder::cursor();
        if(cursor.item>=0)ImGui::Text("Carrying %d x %s",cursor.count,builder::items()[cursor.item].name.c_str());
        ImGui::EndChild();ImGui::SameLine();
        ImGui::BeginChild("crafting",{0,-45},ImGuiChildFlags_None);
        if(!builder::chest()){
            ImGui::SeparatorText("Crafting");
            for(int i=0;i<int(builder::recipes().size());++i){
                const auto& recipe=builder::recipes()[i];ImGui::PushID(i);ImGui::BeginDisabled(!builder::canCraft(i));
                std::string label=builder::items()[recipe.result].name+" x"+std::to_string(recipe.count);
                if(Button(label.c_str(),{-1,30}))builder::craft(i);
                ImGui::EndDisabled();
                std::string cost;for(const auto& ingredient:recipe.inputs){if(!cost.empty())cost+=" + ";cost+=std::to_string(ingredient.count)+" "+ingredient.id;}
                ImGui::TextWrapped("%s | Station: %s",cost.c_str(),recipe.station.c_str());ImGui::PopID();
            }
            ImGui::SeparatorText("Tool repair");ImGui::BeginDisabled(!builder::canRepair());
            if(Button("Repair selected tool",{-1,32}))builder::repair();ImGui::EndDisabled();
            ImGui::TextWrapped("%s",builder::repairHint().c_str());
        }else ImGui::TextWrapped("Click to pick up or store items. Right click splits a stack or places one item. Shift click transfers a stack.");
        ImGui::EndChild();
        if(Button("Close inventory (E / Escape)",{-1,32}))builder::closeInventory();
    }
    ImGui::End();
}
void Timer(){
    if(!ordnance::timerOpen())return;
    if(Window("Timed bomb",460,210)){
        int seconds=std::clamp(std::atoi(ordnance::timerText().c_str()),1,600);
        if(SliderInt("Delay (seconds)",&seconds,1,600))ordnance::setTimerSeconds(seconds);
        if(Button("Place and arm",{200,36}))ordnance::timerKey(VK_RETURN);
        ImGui::SameLine();if(Button("Cancel",{180,36}))ordnance::timerKey(VK_ESCAPE);
        ImGui::TextWrapped("Stand on solid ground to place the bomb.");
    }
    ImGui::End();
}
}
bool Init(HWND window,ID3D11Device* graphicsDevice,ID3D11DeviceContext* context){
    Shutdown();IMGUI_CHECKVERSION();ImGui::CreateContext();
    auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;io.IniFilename=nullptr;
    char windows[MAX_PATH]{};GetWindowsDirectoryA(windows,MAX_PATH);
    std::string fontPath=std::string(windows)+"\\Fonts\\segoeui.ttf";
    if(!io.Fonts->AddFontFromFileTTF(fontPath.c_str(),18)){
        ImFontConfig font;font.SizePixels=18;io.Fonts->AddFontDefault(&font);
    }
    ImGui::StyleColorsDark();auto& style=ImGui::GetStyle();style.WindowPadding={20,18};style.FramePadding={8,6};style.ItemSpacing={10,10};style.WindowRounding=8;style.FrameRounding=4;
    style.Colors[ImGuiCol_Button]={.20f,.36f,.42f,1};style.Colors[ImGuiCol_ButtonHovered]={.30f,.51f,.57f,1};
    style.Colors[ImGuiCol_WindowBg]={.055f,.075f,.095f,1};
    device=graphicsDevice;
    if(!ImGui_ImplWin32_Init(window)){ImGui::DestroyContext();device=nullptr;return false;}
    if(!ImGui_ImplDX11_Init(graphicsDevice,context)){ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();device=nullptr;return false;}
    initialized=true;return true;
}
void Shutdown(){
    icons.clear();if(initialized){ImGui_ImplDX11_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();}
    initialized=false;device=nullptr;
}
void Draw(){
    if(!initialized)return;
    ImGui_ImplDX11_NewFrame();ImGui_ImplWin32_NewFrame();
    for(const auto& event:reviewEvents){
        auto& io=ImGui::GetIO();io.AddFocusEvent(true);io.AddMousePosEvent(event.p.x,event.p.y);
        io.AddKeyEvent(ImGuiMod_Shift,event.shift);io.AddMouseButtonEvent(event.right?1:0,event.down);
    }
    reviewEvents.clear();reviewItems.clear();ImGui::NewFrame();
    if(Modal()){
        input::releaseAim();game::leftMouse=false;std::fill(std::begin(game::keys),std::end(game::keys),false);input::syncLookCapture();
        ImGui::GetBackgroundDrawList()->AddRectFilled({0,0},ImGui::GetIO().DisplaySize,IM_COL32(8,14,20,150));
    }
    Help();Pause();Commerce();Debug();Inventory();Timer();
    ImGui::Render();ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}
void EnableReview(){reviewing=true;reviewItems.clear();reviewEvents.clear();}
bool ReviewPointer(const std::string& label,bool down,bool right,bool shift){
    auto item=reviewItems.find(label);
    if(down&&item==reviewItems.end())return false;
    if(item!=reviewItems.end())reviewPoint=item->second;
    reviewEvents.push_back({reviewPoint,down,right,shift});return true;
}
bool HandleMessage(HWND window,UINT message,WPARAM wparam,LPARAM lparam){
    if(!initialized)return false;
    ImGui_ImplWin32_WndProcHandler(window,message,wparam,lparam);
    if(!Modal())return false;
    if(message==WM_KEYDOWN||message==WM_SYSKEYDOWN){
        if(ui::waitingForBinding>=0){ui::handleKey(int(wparam));return true;}
        if(wparam==VK_ESCAPE&&ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId))return true;
        if(wparam==VK_ESCAPE||wparam==VK_F4||wparam==VK_F11||(wparam==VK_RETURN&&(lparam&(1<<29))))return false;
        if(builder::inventoryOpen()&&(wparam=='E'||wparam==VK_F5||(wparam>='1'&&wparam<='9')||wparam=='R'))return false;
        return true;
    }
    return message==WM_CHAR||message==WM_KEYUP||message==WM_SYSKEYUP||message==WM_LBUTTONDOWN||message==WM_LBUTTONUP||message==WM_RBUTTONDOWN||message==WM_RBUTTONUP||message==WM_MOUSEMOVE||message==WM_MOUSEWHEEL||message==WM_INPUT;
}
}

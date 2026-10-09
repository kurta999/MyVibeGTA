#include "../src/dx12_backend.h"
#include "../src/dx12_skin_shader.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

void dx12SkinScenarios(dx12::Device& device,dx12::Context& commands){
    auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    dx12::ComPtr<ID3DBlob> code,error;
    auto result=D3DCompile(dx12SkinShader,std::strlen(dx12SkinShader),"skin",nullptr,nullptr,"CS","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&error);
    if(FAILED(result)&&error)std::fprintf(stderr,"%s\n",static_cast<const char*>(error->GetBufferPointer()));
    check(SUCCEEDED(result),"extended skin shader compilation");
    dx12::Shader* shader=nullptr;check(SUCCEEDED(device.CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&shader)),"skin shader creation");
    dx11::SkinMesh mesh;mesh.jointCount=6;mesh.bodyPartForJoint={0,1,2,3,4,5};
    struct Input {dx11::SkinVertex vertex;unsigned parts;};std::vector<Input> inputs;
    // All body parts, below/above knees, mixed weights and nonuniform scaling.
    for(unsigned i=0;i<96;++i){dx11::SkinVertex v{};
        v.base={float(int(i%5)-2)*.2f,float(i%16)*.13f,float(int(i%7)-3)*.1f,.2f,.8f,.4f,.2f,.7f,.8f,.6f,.4f,1};
        unsigned parts=0;for(unsigned j=0;j<4;++j){v.joints[j]=(i+j)%6;v.weights[j]=(j+1)*.1f;parts|=unsigned(v.joints[j])<<(j*8);}
        mesh.vertices.push_back(v);inputs.push_back({v,parts});
    }
    dx12::Buffer *source=nullptr,*constants=nullptr,*output=nullptr,*priorOutput=nullptr,*readback=nullptr,*priorReadback=nullptr;
    dx12::View *sourceView=nullptr,*outputView=nullptr,*priorView=nullptr;
    dx12::BufferDesc desc{};desc.ByteWidth=UINT(inputs.size()*sizeof(Input));desc.BindFlags=dx12::ShaderInput;desc.MiscFlags=dx12::Structured;desc.StructureByteStride=sizeof(Input);
    dx12::InitialData initial{inputs.data()};check(SUCCEEDED(device.CreateBuffer(&desc,&initial,&source)),"skin input");check(SUCCEEDED(device.CreateShaderResourceView(source,nullptr,&sourceView)),"skin input view");
    desc={};desc.ByteWidth=sizeof(Dx12SkinConstants);desc.BindFlags=dx12::Constant;check(SUCCEEDED(device.CreateBuffer(&desc,nullptr,&constants)),"skin constants");
    auto outputBuffer=[&](unsigned stride,dx12::Buffer** buffer,dx12::View** view,dx12::Buffer** read){
        desc={};desc.ByteWidth=UINT(inputs.size()*stride);desc.BindFlags=dx12::Unordered;desc.MiscFlags=dx12::Raw;
        check(SUCCEEDED(device.CreateBuffer(&desc,nullptr,buffer)),"skin output");
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;uav.Format=DXGI_FORMAT_R32_TYPELESS;uav.Buffer.NumElements=desc.ByteWidth/4;uav.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
        check(SUCCEEDED(device.CreateUnorderedAccessView(*buffer,&uav,view)),"skin output view");
        desc.BindFlags=desc.MiscFlags=0;desc.Usage=dx12::Readback;check(SUCCEEDED(device.CreateBuffer(&desc,nullptr,read)),"skin readback");
    };
    outputBuffer(48,&output,&outputView,&readback);outputBuffer(16,&priorOutput,&priorView,&priorReadback);
    float maxError=0;
    for(int scenario=0;scenario<20;++scenario){
        int mode=scenario%10;bool validHistory=scenario<10;
        dx11::SkinInstance pose;pose.source=&mesh;pose.scale={14,17,11,0};pose.origin={0,-.1f,0,0};pose.transform={100,20,-90,0};pose.yaw={std::cos(.7f),std::sin(.7f),0,0};
        for(unsigned j=0;j<mesh.jointCount;++j)pose.palette.push_back({1,0,0,0,0,1,0,0,0,0,1,0,float(j)*.01f,.02f,0,1});
        pose.deformation.motion={float(mode%9),std::sin(.8f),std::cos(.8f),.7f};pose.deformation.parameters={2,.55f,34,mode==9?1.0f:0.0f};
        pose.deformation.attachmentOrigin={103,18,-95,0};
        pose.deformation.attachmentRotation={std::sin(.13f),0,0,std::cos(.13f)};
        for(int part=0;part<6;++part){float angle=.13f*(part+1);
            pose.deformation.bodyRest[part]={100,float(part)*5,-90,0};pose.deformation.bodyPosition[part]={110,float(part)*4,-80,0};
            pose.deformation.bodyRotation[part]={0,std::sin(angle),0,std::cos(angle)};
        }
        auto previous=pose;previous.transform[0]-=3;previous.deformation.motion[1]=std::sin(.3f);previous.deformation.motion[2]=std::cos(.3f);previous.deformation.parameters[1]=-.35f;
        for(auto& b:previous.deformation.bodyPosition)b[2]-=2;
        Dx12SkinConstants data{};std::copy(pose.palette.begin(),pose.palette.end(),data.palette);std::copy(previous.palette.begin(),previous.palette.end(),data.previousPalette);
        data.scale=pose.scale;data.origin=pose.origin;data.transform=pose.transform;data.yaw=pose.yaw;data.deformation=pose.deformation;
        data.previousScale=previous.scale;data.previousOrigin=previous.origin;data.previousTransform=previous.transform;data.previousYaw=previous.yaw;data.previousDeformation=previous.deformation;
        data.count=UINT(inputs.size());data.joints=mesh.jointCount;data.padding=validHistory?1:0;
        if(mode==0){dx12::ComPtr<ID3D12ShaderReflection> reflection;check(SUCCEEDED(D3DReflect(code->GetBufferPointer(),code->GetBufferSize(),IID_PPV_ARGS(&reflection))),"skin reflection");
            const char* names[]={"deformation","previousDeformation","previousTranslation","vertexCount"};
            const size_t offsets[]={offsetof(Dx12SkinConstants,deformation),offsetof(Dx12SkinConstants,previousDeformation),offsetof(Dx12SkinConstants,previousTransform),offsetof(Dx12SkinConstants,count)};
            for(int i=0;i<4;++i){D3D12_SHADER_VARIABLE_DESC v{};check(SUCCEEDED(reflection->GetConstantBufferByName("Skin")->GetVariableByName(names[i])->GetDesc(&v))&&v.StartOffset==offsets[i],"CPU/shader skin constant layout");}}
        commands.UpdateSubresource(constants,0,nullptr,&data,0,0);commands.CSSetShader(shader,nullptr,0);commands.CSSetConstantBuffers(0,1,&constants);commands.CSSetShaderResources(0,1,&sourceView);
        dx12::View* outputs[]={outputView,priorView};commands.CSSetUnorderedAccessViews(0,2,outputs,nullptr);commands.Dispatch((data.count+63)/64,1,1);
        commands.CopyResource(readback,output);commands.CopyResource(priorReadback,priorOutput);
        std::vector<dx11::Vertex> expected,prior;dx11::deformSkinCpu(pose,expected);dx11::deformSkinCpu(validHistory?previous:pose,prior);
        dx12::MappedData mapped{};check(SUCCEEDED(commands.Map(readback,0,dx12::MapRead,0,&mapped)),"skin map");
        for(size_t i=0;i<expected.size();++i){float reference[12];std::memcpy(reference,&expected[i],48);
            for(int f=0;f<12;++f){float value=static_cast<float*>(mapped.pData)[i*12+f],delta=std::abs(value-reference[f]);maxError=std::max(maxError,delta);
                if(!std::isfinite(value)||delta>(f<3?.003f:.00003f)){std::fprintf(stderr,"Skin mode %d vertex %zu field %d: %.8f / %.8f\n",mode,i,f,value,reference[f]);check(false,"GPU skin current pose mismatch");}}
        }commands.Unmap(readback,0);
        check(SUCCEEDED(commands.Map(priorReadback,0,dx12::MapRead,0,&mapped)),"prior skin map");
        for(size_t i=0;i<prior.size();++i){float reference[]={prior[i].x,prior[i].y,prior[i].z,float(data.padding)};
            for(int f=0;f<4;++f){float value=static_cast<float*>(mapped.pData)[i*4+f];if(!std::isfinite(value)||std::abs(value-reference[f])>=.003f){std::fprintf(stderr,"Prior skin mode %d vertex %zu field %d: %.8f / %.8f\n",mode,i,f,value,reference[f]);check(false,"GPU skin previous pose mismatch");}}}
        commands.Unmap(priorReadback,0);
    }
    commands.ClearState();commands.CSSetShader(nullptr,nullptr,0);
    sourceView->Release();outputView->Release();priorView->Release();source->Release();constants->Release();output->Release();priorOutput->Release();readback->Release();priorReadback->Release();shader->Release();
    std::printf("PASS GPU skin readback: 9 procedural modes, aiming, ragdoll, previous poses; max error %.8f\n",maxError);
}

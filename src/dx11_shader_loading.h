#pragma once
#include <d3dcompiler.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dx11::shader {
struct Request {std::string source,entry,profile;};
struct Compiled {std::shared_ptr<ID3DBlob> blob;std::string error;double seconds=0;};
struct Stats {unsigned workers=1,peakActive=0;std::size_t completed=0;double wallSeconds=0,cpuSeconds=0;};
// No D3D context, UI, or logging access on workers. All jobs join before return.
bool compileBatch(const std::vector<Request>& requests,unsigned workers,
    const std::function<bool(std::size_t,std::size_t)>& progress,
    std::vector<Compiled>& outputs,Stats& stats);
}

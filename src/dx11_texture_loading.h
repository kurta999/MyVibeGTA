#pragma once
#include "dx11_texture_mips.h"
#include <atomic>
#include <functional>
#include <string>

namespace dx11::texture {
struct Request {std::wstring file;Kind kind=Kind::Color;};
struct Prepared {
    std::vector<Level> levels;
    std::string error;
    double decodeSeconds=0,mipSeconds=0;
};
struct LoadingStats {
    unsigned workers=1,peakActive=0;
    std::size_t completed=0,peakPending=0;
    double wallSeconds=0,decodeSeconds=0,mipSeconds=0,consumeSeconds=0;
};
unsigned defaultLoadingWorkers();
// GDI+ must be initialized before calling, and remain alive until return.
Prepared prepare(const Request& request,const std::atomic<bool>* cancelled=nullptr);
// Workers only prepare private CPU data. consume/progress run on the caller.
// At most workers results are outstanding; cancellation joins every worker.
bool prepareBatch(const std::vector<Request>& requests,unsigned workers,
    const std::function<bool(std::size_t,const Prepared&)>& consume,
    const std::function<bool(std::size_t,std::size_t)>& progress,LoadingStats& stats);
}

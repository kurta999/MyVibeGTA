#pragma once
#include "save_status.h"
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace savegame {
// One active write and at most one pending snapshot. Only owned strings cross
// the thread boundary; gameplay state is captured by the caller.
class Writer {
public:
    using Write=std::function<bool(const std::string&,const std::string&)>;
    explicit Writer(Write write):write(std::move(write)),worker([this]{run();}){}
    ~Writer(){
        {std::lock_guard<std::mutex> lock(mutex);stopping=true;}
        changed.notify_all();worker.join();
    }
    Writer(const Writer&)=delete;
    Writer& operator=(const Writer&)=delete;
    WriteReceipt submit(std::string path,std::string text){
        WriteReceipt receipt{std::make_shared<std::atomic<WriteStatus>>(WriteStatus::Pending)};
        {std::lock_guard<std::mutex> lock(mutex);
            if(pending)pending->receipt.complete(WriteStatus::Superseded);
            pending=Snapshot{std::move(path),std::move(text),receipt};}
        changed.notify_all();
        return receipt;
    }
    bool flush(){
        std::unique_lock<std::mutex> lock(mutex);
        changed.wait(lock,[this]{return !busy&&!pending;});return lastResult;
    }
    bool takeFailure(){std::lock_guard<std::mutex> lock(mutex);bool result=failed;failed=false;return result;}
private:
    void run(){
        for(;;){
            Snapshot snapshot;
            {
                std::unique_lock<std::mutex> lock(mutex);
                changed.wait(lock,[this]{return stopping||pending.has_value();});
                if(!pending)return;
                snapshot=std::move(*pending);pending.reset();busy=true;
            }
            bool ok=false;try{ok=write(snapshot.path,snapshot.text);}catch(...){ok=false;}
            {std::lock_guard<std::mutex> lock(mutex);lastResult=ok;failed|=!ok;busy=false;
                snapshot.receipt.complete(ok?WriteStatus::Succeeded:WriteStatus::Failed);}
            changed.notify_all();
        }
    }
    Write write;
    struct Snapshot {std::string path,text;WriteReceipt receipt;};
    std::mutex mutex;
    std::condition_variable changed;
    std::optional<Snapshot> pending;
    bool busy=false,stopping=false,lastResult=true,failed=false;
    // Last member: all state exists before the thread starts.
    std::thread worker;
};
}

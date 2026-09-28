#pragma once
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
    void submit(std::string path,std::string text){
        {std::lock_guard<std::mutex> lock(mutex);pending=std::make_pair(std::move(path),std::move(text));}
        changed.notify_all();
    }
    bool flush(){
        std::unique_lock<std::mutex> lock(mutex);
        changed.wait(lock,[this]{return !busy&&!pending;});return lastResult;
    }
    bool takeFailure(){std::lock_guard<std::mutex> lock(mutex);bool result=failed;failed=false;return result;}
private:
    void run(){
        for(;;){
            std::pair<std::string,std::string> snapshot;
            {
                std::unique_lock<std::mutex> lock(mutex);
                changed.wait(lock,[this]{return stopping||pending.has_value();});
                if(!pending)return;
                snapshot=std::move(*pending);pending.reset();busy=true;
            }
            bool ok=false;try{ok=write(snapshot.first,snapshot.second);}catch(...){ok=false;}
            {std::lock_guard<std::mutex> lock(mutex);lastResult=ok;failed|=!ok;busy=false;}
            changed.notify_all();
        }
    }
    Write write;
    std::mutex mutex;
    std::condition_variable changed;
    std::optional<std::pair<std::string,std::string>> pending;
    bool busy=false,stopping=false,lastResult=true,failed=false;
    // Last member: all state exists before the thread starts.
    std::thread worker;
};
}

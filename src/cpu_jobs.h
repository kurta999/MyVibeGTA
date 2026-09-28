#pragma once
#include <algorithm>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

namespace cpu {
// A one-worker pool executes inline, providing an identical serial reference.
// Callers bound outstanding jobs; each job owns its output until publication.
class Pool {
public:
    explicit Pool(unsigned concurrency):count(std::max(1u,concurrency)){
        try{
            if(count>1)for(unsigned i=0;i<count;++i)threads.emplace_back([this]{run();});
        }catch(...){stop();throw;}
    }
    ~Pool(){stop();}
    Pool(const Pool&)=delete;
    Pool& operator=(const Pool&)=delete;
    unsigned concurrency() const{return count;}
    template<class F> auto submit(F&& function)->std::future<std::invoke_result_t<F>>{
        using Result=std::invoke_result_t<F>;
        auto task=std::make_shared<std::packaged_task<Result()>>(std::forward<F>(function));
        auto future=task->get_future();
        if(count==1)(*task)();
        else{
            {std::lock_guard<std::mutex> lock(mutex);jobs.emplace([task]{(*task)();});}
            ready.notify_one();
        }
        return future;
    }
private:
    void stop(){
        {std::lock_guard<std::mutex> lock(mutex);stopping=true;}
        ready.notify_all();
        for(auto& thread:threads)if(thread.joinable())thread.join();
    }
    void run(){
        for(;;){
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(mutex);
                ready.wait(lock,[this]{return stopping||!jobs.empty();});
                if(jobs.empty())return;
                job=std::move(jobs.front());jobs.pop();
            }
            job();
        }
    }
    unsigned count;
    bool stopping=false;
    std::mutex mutex;
    std::condition_variable ready;
    std::queue<std::function<void()>> jobs;
    std::vector<std::thread> threads;
};
}

#include "../src/save_jobs.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <future>
#include <vector>
#include <stdexcept>
#include <cstdio>

int main(){
    std::promise<void> entered,release;auto gate=release.get_future().share();
    std::vector<std::string> written;
    const auto caller=std::this_thread::get_id();
    {
        savegame::Writer writer([&](const std::string& path,const std::string& text){
            assert(std::this_thread::get_id()!=caller&&path=="save.ini");
            if(text=="first"){entered.set_value();gate.wait();}
            written.push_back(text);return true;
        });
        writer.submit("save.ini","first");entered.get_future().wait();
        // The worker is deliberately blocked: submissions must still finish,
        // and superseded snapshots must not build an unbounded disk queue.
        for(int i=0;i<1000;++i)writer.submit("save.ini",std::to_string(i));
        release.set_value();assert(writer.flush());
        assert((written==std::vector<std::string>{"first","999"}));
        writer.submit("save.ini","last");
    } // Destruction drains the final request.
    assert(written.back()=="last");
    savegame::Writer writer([](const auto&,const auto& text){
        if(text=="throw")throw std::runtime_error("write failed");return text!="fail";
    });
    writer.submit("save.ini","fail");assert(!writer.flush()&&writer.takeFailure());
    assert(!writer.takeFailure());
    writer.submit("save.ini","throw");assert(!writer.flush()&&writer.takeFailure());
    writer.submit("save.ini","ok");assert(writer.flush()&&!writer.takeFailure());
    std::puts("save worker bounded queue, snapshot ordering, shutdown, failure and recovery passed");
}

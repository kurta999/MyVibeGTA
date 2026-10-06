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
        auto first=writer.submit("save.ini","first");entered.get_future().wait();
        assert(first.status()==savegame::WriteStatus::Pending);
        // The worker is deliberately blocked: submissions must still finish,
        // and superseded snapshots must not build an unbounded disk queue.
        auto superseded=writer.submit("save.ini","replaced");
        savegame::WriteReceipt latest;
        for(int i=0;i<1000;++i)latest=writer.submit("save.ini",std::to_string(i));
        assert(superseded.status()==savegame::WriteStatus::Superseded&&latest.status()==savegame::WriteStatus::Pending);
        release.set_value();assert(writer.flush());
        assert(first.status()==savegame::WriteStatus::Succeeded&&latest.status()==savegame::WriteStatus::Succeeded);
        assert((written==std::vector<std::string>{"first","999"}));
        writer.submit("save.ini","last");
    } // Destruction drains the final request.
    assert(written.back()=="last");
    savegame::Writer writer([](const auto&,const auto& text){
        if(text=="throw")throw std::runtime_error("write failed");return text!="fail";
    });
    auto failed=writer.submit("save.ini","fail");assert(!writer.flush()&&writer.takeFailure());
    assert(failed.status()==savegame::WriteStatus::Failed);
    assert(!writer.takeFailure());
    auto thrown=writer.submit("save.ini","throw");assert(!writer.flush()&&writer.takeFailure());
    assert(thrown.status()==savegame::WriteStatus::Failed);
    auto recovered=writer.submit("save.ini","ok");assert(writer.flush()&&!writer.takeFailure());
    assert(recovered.status()==savegame::WriteStatus::Succeeded&&failed.status()==savegame::WriteStatus::Failed);
    std::puts("save worker bounded queue, exact-snapshot receipts, shutdown, failure and recovery passed");
}

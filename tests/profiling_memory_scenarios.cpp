#include <new>
#include <cstdint>
#include <limits>
#include <thread>
#include <cstdio>
#include <initializer_list>

int main() {
    int failures=0;
    auto check=[&](bool ok){if(!ok)++failures;};
    void* zero=::operator new(0);check(zero!=nullptr);::operator delete(zero);
    void* sized=::operator new(32);::operator delete(sized,std::size_t(32));
    void* array=::operator new[](64);::operator delete[](array,std::size_t(64));
    for(auto alignment:{std::size_t(32),std::size_t(64),std::size_t(256)}) {
        auto align=std::align_val_t(alignment);
        void* p=::operator new(96,align);check(reinterpret_cast<std::uintptr_t>(p)%alignment==0);
        // Containers can transfer owned allocations between workers.
        std::thread worker([p,align]{::operator delete(p,std::size_t(96),align);});worker.join();
        p=::operator new[](96,align,std::nothrow);check(p&&reinterpret_cast<std::uintptr_t>(p)%alignment==0);
        ::operator delete[](p,align,std::nothrow);
    }
    auto previous=std::set_new_handler([]{throw std::bad_alloc();});
    // Impossible allocation checks throwing and nothrow contracts without OOM.
    check(::operator new(std::numeric_limits<std::size_t>::max()/2,std::nothrow)==nullptr);
    bool threw=false;
    try {void* p=::operator new(std::numeric_limits<std::size_t>::max()/2);::operator delete(p);}
    catch(const std::bad_alloc&) {threw=true;}
    check(threw);std::set_new_handler(previous);
    ::operator delete(nullptr);::operator delete(nullptr,std::align_val_t(64));
    std::printf("Profiling allocation scenarios: %d failures\n",failures);
    return failures?1:0;
}

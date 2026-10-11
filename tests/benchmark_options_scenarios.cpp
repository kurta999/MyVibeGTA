#include "../src/benchmark_options.h"
#include <cstdio>
#include <initializer_list>
int main() {
    int failures=0;
    auto check=[&](bool ok){if(!ok)++failures;};
    int n=120;
    check(benchmark::ReadCount(nullptr,"--benchmark-frames=",1,36000,n)&&n==120);
    check(benchmark::ReadCount("--smoke --day","--benchmark-frames=",1,36000,n)&&n==120);
    check(benchmark::ReadCount("--smoke --benchmark-frames=240 --day","--benchmark-frames=",1,36000,n)&&n==240);
    check(benchmark::ReadCount("--benchmark-frames=36000","--benchmark-frames=",1,36000,n)&&n==36000);
    for(auto text:{"--benchmark-frames=0","--benchmark-frames=-1","--benchmark-frames=36001",
                  "--benchmark-frames=oops","--benchmark-frames=","--benchmark-frames=20x",
                  "--benchmark-frames=999999999999999999999999"})
        check(!benchmark::ReadCount(text,"--benchmark-frames=",1,36000,n));
    check(benchmark::ReadCount("--benchmark-warmup=0","--benchmark-warmup=",0,3600,n)&&n==0);
    check(!benchmark::ReadCount("--tracy-wait=61","--tracy-wait=",0,60,n));
    std::printf("Benchmark option scenarios: %d failures\n",failures);
    return failures?1:0;
}

# Realtime Blackboard

C++20 header-only realtime blackboard, benchmarked on Radxa Q6A.

## Building

``` bash
cmake -DCMAKE_BUILD_TYPE=Release -Bbuild .
cmake --build ./build -j4
# testing
cmake -B build-tsan -DRTBB_TSAN=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-tsan -j4
ctest --test-dir build-tsan --output-on-failure
```

## Usage

``` c++
#include <rtbb.h>

// ...

// create blackboard
rtbb::Blackboard bb;

auto k = bb.key<float>("audio/bearing");
bb.subscribe(k, [](const float& b) { std::printf("bearing %.1f\n", b); });
bb.set(k, 42.f);                      // prints "bearing 42.0"
if (auto v = bb.get(k)) use(*v);      // snapshot; null before first set()
```

## Roadmap

| Version | Features |
| --- | --- |
| v0 | global mutex (current) |
| v1 | SPSC subscriber queues; per-key seqlock slots (lock-free readers) |
| v2 | no runtime allocation, `mlockall`, CPU pinning |

## Results (Radxa Q6A)

TBD after 1 kHz benchmark

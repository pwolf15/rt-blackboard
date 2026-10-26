# Realtime Blackboard

C++20 Header-only realtime blackboard, benchmarked on Radxa Q6A.

## Building

``` bash
cmake -DCMAKE_BUILD_TYPE=Release -Bbuild .
cmake --build ./build -j4
ctest --test-dir ./build --output-on-failure # testing
```

## Usage

``` c++
#include <rtbb.h>

// ...

int counter = 0;

rtbb::Blackboard bb;
auto bearing = bb.key<float>("audio/bearing");
bb.subscribe(bearing, [&](const float&) { counter++; });
bb.subscribe(bearing, [&](const float&) { counter++; });
bb.set(bearing, 42.f);
bb.set(bearing, 84.f);
```

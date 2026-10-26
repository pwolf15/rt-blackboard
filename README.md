# Realtime Blackboard

C++20 Header-only realtime blackboard, benchmarked on Radxa Q6A.

# Building

``` bash
mkdir build
cmake -DCMAKE_BUILD_TYPE=Release -Bbuild .
cmake --build ./build -j4
```
# Testing

``` bash
mkdir build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -Bbuild .
cmake --build ./build -j4
ctest --test-dir ./build --output-on-failure
```

# API

| Call  | Description |
| --- | --- |
| Key<T> | returns typed handle to key |
| get | returns value of key in blackboard |
| set | sets value of key in blackboard

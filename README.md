# Realtime Blackboard

C++20 header-only realtime blackboard, benchmarked on Radxa Q6A.

## Background

A blackboard is a shared, named store that components read and write instead of talking to each other directly (Nii, 1986). BehaviorTree.CPP uses the same idea for passing data between nodes. rtbb focuses on timing under real-time load.

## Building

``` bash
cmake -DCMAKE_BUILD_TYPE=Release -Bbuild .
cmake --build ./build -j4
# testing
cmake -B build-tsan -DRTBB_TSAN=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-tsan -j4
ctest --test-dir build-tsan --output-on-failure
# bench
./build/bench [--seconds N] [--subs N] [--rt CPU]
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
| v2 | no allocation after startup in `set`/`get`; benchmark asserts zero page faults |

## Results (Radxa Q6A)

Setup:
* Release build
* CPU governor `performance`
* 1 kHz publisher, 1 subscriber, 60 s per run (first 100 samples dropped as warm-up).
* Stock Ubuntu kernel, `PREEMPT_DYNAMIC` in voluntary mode (not `PREEMPT_RT`). Times in µs.

```bash
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
./build/bench --seconds 60
sudo ./build/bench --seconds 60 --rt 6
```

`wake jitter` is how late the publisher wakes after its 1 ms deadline.
`latency` is the time from `set()` to the subscriber callback.

| Config | Wake jitter p50 / p99 / p99.9 / max | Latency p50 / p99 / p99.9 / max |
|---|---|---|
| default scheduler | 54.3 / 59.7 / 62.6 / 393.6 | 0.31 / 0.89 / 1.30 / 3.54 |
| `--rt 6` (Cortex-A78, 2.4 GHz) | 4.2 / 5.0 / 8.0 / 20.6 | 0.31 / 0.78 / 0.83 / 3.28 |
| `--rt 7` (Cortex-A78, 2.7 GHz) | 2.9 / 3.7 / 5.6 / 47.3 | 0.21 / 0.36 / 0.73 / 1.51 |
| `--rt 2` (Cortex-A55, 2.0 GHz) | 8.4 / 10.1 / 13.3 / 72.7 | 0.83 / 1.25 / 1.41 / 2.45 |

`--rt N` = `SCHED_FIFO` priority 80, pinned to CPU N, `mlockall`.

Observations:
* Without `--rt`, publisher wakes up ~54 µs late. Most of that is the kernel's default 50 µs timer slack for normal threads. `SCHED_FIFO` removes it.
* `--rt` doesn't change publish-to-callback latency. In v0 the callback runs on the publisher thread, so latency is determined by the lock, one allocation, and one function call. This depends on core speed, not scheduling.
* CPU 7 is faster at the median but has a worse max than CPU 6 in these runs.
* Switching the kernel to `preempt=full` did not change p50/p99. Max values vary between runs (21-49 µs on CPU 6), so single-run max should be read loosely.
* The CPU governor mattered most for latency: under `schedutil` the same build measured 2.3 µs p50, because the core clocks down between 1 ms ticks.
* Timer resolution on this board is ~52 ns (19.2 MHz counter).
* In v0, a low susbcriber delays every subscriber after it and the publisher itself. Once total callback work exceeds the period, the publisher falls behind without bound. RT throttling adds 50ms stalls. v1 moves callbacks off publisher thread.
* Overload (8 x 150 µs per 1 ms tick): the publisher falls behind 0.2 ms per tick. With the default RT throttling (`sched_rt_runtime_us=950000`) the 100%-busy `SCHED_FIFO` thread is paused ~50 ms per second (last-subscriber max 45.6 ms). With throttling idsabled the max is 1.09 ms and the backlog after 9,900 ticks shrinks from 2.6s to 2.1 s.

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

### Scheduling and core

* `wake jitter`: how late the publisher wakes after its 1 ms deadline.
* `latency`: time from `set()` to the subscriber callback.

| Config | Wake jitter p50 / p99 / p99.9 / max | Latency p50 / p99 / p99.9 / max |
|---|---|---|
| default scheduler | 54.3 / 59.7 / 62.6 / 393.6 | 0.31 / 0.89 / 1.30 / 3.54 |
| `--rt 6` (Cortex-A78, 2.4 GHz) | 4.2 / 5.0 / 8.0 / 20.6 | 0.31 / 0.78 / 0.83 / 3.28 |
| `--rt 7` (Cortex-A78, 2.7 GHz) | 2.9 / 3.7 / 5.6 / 47.3 | 0.21 / 0.36 / 0.73 / 1.51 |
| `--rt 2` (Cortex-A55, 2.0 GHz) | 8.4 / 10.1 / 13.3 / 72.7 | 0.83 / 1.25 / 1.41 / 2.45 |

`--rt N` = `SCHED_FIFO` priority 80, pinned to CPU N, `mlockall`.

### Callback work

* `--work-us N` busy-waits N µs inside each subscriber callback.
* `--rt 6` unless notes; times in µs.
* Disabling RT throttling: `echo -1 | sudo tee /proc/sys/kernel/sched_rt_runtime_us`

| Config | Wake jitter p50 / p99.9 / max | First sub p50 / max | Last sub p50 / p99.9 / max |
|---|---|---|---|
| 5 subs, 0 µs (no `--rt`, 5 s) | 54.4 / 58.8 / 64.2 | 0.47 / 3.59 | 0.78 / 5.47 / 7.50 |
| 5 subs × 50 µs (60 s) | 4.2 / 6.6 / 42.6 | 0.47 / 3.28 | 201.4 / 202.4 / 257.8 |
| 8 subs × 150 µs, overload (10 s) | 1.28 s / 2.62 s / 2.62 s | 0.73 / 9.84 | 1052 / 1060 / 45 619 |
| same, RT throttling off | 1.06 s / 2.09 s / 2.09 s | 0.73 / 4.43 | 1052 / 1056 / 1088 |

### Reader contention

* Readers (`--rt 6`, `performance` governor config, 60 s, 1 subscriber)

``` bash
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
sudo ./build/bench --seconds 60 --rt 6 --readers 0
sudo ./build/bench --seconds 60 --rt 6 --readers 4
```

| Readers | set() p50 / p99 / p99.9 / max | Latency p50 / p99 / p99.9 / max |
| --- | --- | --- |
| 0 | 0.52 / 0.68 / 1.15 / 3.39 | 0.31 / 0.42 / 0.83 / 2.97 |
| 4 (6.2 M reads/s) | 2.03 / 11.7 / 17.6 / 560 | 1.77 / 11.4 / 17.4 / 560 |

Observations:
* Without `--rt`, publisher wakes up ~54 µs late. Most of that is the kernel's default 50 µs timer slack for normal threads. `SCHED_FIFO` removes it.
* `--rt` doesn't change publish-to-callback latency. In v0 the callback runs on the publisher thread, so latency is determined by the lock, one allocation, and one function call. This depends on core speed, not scheduling.
* CPU 7 is faster at the median but has a worse max than CPU 6 in these runs.
* Switching the kernel to `preempt=full` did not change p50/p99. Max values vary between runs (21-49 µs on CPU 6), so single-run max should be read loosely.
* The CPU governor mattered most for latency: under `schedutil` the same build measured 2.3 µs p50, because the core clocks down between 1 ms ticks.
* Timer resolution on this board is ~52 ns (19.2 MHz counter).
* Overload (8 x 150 µs per 1 ms tick): the publisher falls behind 0.2 ms per tick. With the default RT throttling (`sched_rt_runtime_us=950000`) the 100%-busy `SCHED_FIFO` thread is paused ~50 ms per second (last-subscriber max 45.6 ms). With throttling disabled the max is 1.09 ms and the backlog after 9,900 ticks shrinks from 2.6s to 2.1 s.
* With 5 subscribers x 50 µs, the last subscriber waits 201 µs (4 x 50 µs) because v0 runs callbacks one after another on publisher thread; first stays at 0.5 µs.
* Readers slow down the writer: with 4 threads calling `get()`, `set()` gets 4x slower at p50, ~16x at p99/p99.9, and the max rises from 3.4 to 560 µs. The max reproduced in two runs (540, 560 µs), consistent with priority inversion. A normal-priority reader preempted while holding the mutex blocks the `SCHED_FIFO` publisher. v1's seqlock removes the lock from `get()`.
* Padding per-reader counters to separate cache lines made no measurable difference. Mutex is the bottleneck.

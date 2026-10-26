#include <pthread.h>
#include <rtbb.h>
#include <sched.h>
#include <sys/mman.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

inline int64_t to_ns(const timespec& t) {
  return int64_t(t.tv_sec) * 1'000'000'000 + t.tv_nsec;
}
inline int64_t now_ns() {
  timespec cur;
  clock_gettime(CLOCK_MONOTONIC, &cur);
  return to_ns(cur);
}

struct TestMessage {
  uint64_t seq;
  int64_t t_pub_ns;
  float payload[8];
};

void print_stats(const std::string& name, const std::vector<int64_t>& vec) {
  if (vec.empty()) {
    std::cout << name << " n=0\n";
    return;
  }

  std::vector<int64_t> v = vec;
  std::sort(v.begin(), v.end());

  // print p50, p99, p99.9
  auto pct = [&](double q) { return v[size_t(q * (v.size() - 1))] / 1e3; };
  std::cout << std::fixed << std::setprecision(2);
  std::cout << name << " n=" << v.size() << " p50 " << pct(0.5) << " p99 " << pct(0.99) << " p99.9 "
            << pct(0.999) << " max " << pct(1.0) << " us\n";
}

struct Args {
  int seconds = 2;
  int rt_cpu = -1;
  int readers = 0;
  int work_us = 0;
  int subs = 1;
};

Args parse(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    std::string_view f = argv[i];
    auto next = [&] {
      if (i + 1 >= argc) throw std::invalid_argument(std::string(f) + " needs a value");
      return std::stoi(argv[++i]);
    };
    if (f == "--seconds")
      a.seconds = next();
    else if (f == "--rt")
      a.rt_cpu = next();
    else if (f == "--readers")
      a.readers = next();
    else if (f == "--work-us")
      a.work_us = next();
    else if (f == "--subs")
      a.subs = next();
    else
      throw std::invalid_argument("unknown flag: " + std::string(f));
  }
  return a;
}

int main(int argc, char** argv) {
  Args a;
  try {
    a = parse(argc, argv);
  } catch (const std::exception& e) {
    std::cerr << e.what()
              << "\nusage: bench [--seconds N] [--subs N] [--readers N] [--work-us N] [--rt CPU]\n";
    return 2;
  }

  rtbb::Blackboard bb;
  auto k = bb.key<TestMessage>("test_message");

  // realtime config
  if (a.rt_cpu >= 0) {
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
      perror("mlockall");
      return 1;
    }
  }

  int seconds = a.seconds;
  uint64_t seq = 0;
  std::vector<int64_t> wake_jitter;
  wake_jitter.reserve(seconds * 1000);

  constexpr uint64_t kWarmup = 100;

  // publisher thread
  auto publish = [&]() {
    if (a.rt_cpu >= 0) {
      sched_param sp{};
      sp.sched_priority = 80;
      if (int e = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp)) {
        std::cerr << "SCHED_FIFO: " << strerror(e) << "\n";
        std::exit(1);
      }
      cpu_set_t set;
      CPU_ZERO(&set);
      CPU_SET(a.rt_cpu, &set);
      if (int e = pthread_setaffinity_np(pthread_self(), sizeof(set), &set)) {
        std::cerr << "affinity: " << strerror(e) << "\n";
        std::exit(1);
      }
    }
    timespec start, deadline;
    clock_gettime(CLOCK_MONOTONIC, &start);
    deadline = start;

    int64_t end = to_ns(start) + seconds * 1'000'000'000LL;

    while (to_ns(deadline) < end) {
      while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, nullptr) == EINTR) {
      }
      const int64_t now = now_ns();

      if (seq >= kWarmup) wake_jitter.push_back(now - to_ns(deadline));

      bb.set(k, TestMessage{seq++, now_ns(), {}});

      deadline.tv_nsec += 1'000'000;  // sleep 1 ms
      if (deadline.tv_nsec >= 1'000'000'000) {
        deadline.tv_nsec -= 1'000'000'000;
        ++deadline.tv_sec;
      }
    }
  };

  std::vector<std::vector<int64_t>> latency(a.subs);
  for (auto& v : latency) v.reserve(seconds * 1000);
  for (int i = 0; i < a.subs; ++i) {
    bb.subscribe(k, [&, i](const TestMessage& m) {
      if (m.seq >= kWarmup) latency[i].push_back(now_ns() - m.t_pub_ns);
      // simulate work with busy-wait
      if (a.work_us > 0) {
        const int64_t until = now_ns() + a.work_us * 1'000LL;
        while (now_ns() < until) {
        }
      }
    });
  }
  std::thread pub(publish);
  pub.join();

  print_stats("wake_jitter", wake_jitter);
  print_stats("latency first subscriber", latency[0]);
  print_stats("latency last subscriber", latency[latency.size() - 1]);

  return 0;
}

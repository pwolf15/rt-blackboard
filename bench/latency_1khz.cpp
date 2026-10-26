#include <rtbb.h>

#include <cerrno>
#include <cstdint>
#include <ctime>
#include <iostream>
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
  std::vector<int64_t> v = vec;
  std::sort(v.begin(), v.end());

  // print p50, p99, p99.9
  float q = 0.5f;
  float p50 = v[size_t(q * (v.size() - 1))];
  q = 0.99f;
  float p99 = v[size_t(q * (v.size() - 1))];
  q = 0.999f;
  float p999 = v[size_t(q * (v.size() - 1))];
  std::cout << name << " p50: " << p50 << " p99: " << p99 << " p99.9 " << p999 << "\n";
}

int main() {
  rtbb::Blackboard bb;
  auto k = bb.key<TestMessage>("test_message");
  int seconds = 2;
  uint64_t seq = 0;
  std::vector<int64_t> wake_jitter;
  wake_jitter.reserve(seconds * 1000);

  // publisher thread
  auto publish = [&]() {
    timespec start, deadline;
    clock_gettime(CLOCK_MONOTONIC, &start);
    deadline = start;

    int64_t end = to_ns(start) + seconds * 1'000'000'000LL;

    while (to_ns(deadline) < end) {
      while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, nullptr) == EINTR) {
      }
      const int64_t now = now_ns();
      wake_jitter.push_back(now - to_ns(deadline));
      bb.set(k, TestMessage{seq++, now_ns(), {}});

      deadline.tv_nsec += 1'000'000;  // sleep 1 ms
      if (deadline.tv_nsec >= 1'000'000'000) {
        deadline.tv_nsec -= 1'000'000'000;
        ++deadline.tv_sec;
      }
    }
  };

  std::vector<int64_t> latency;
  latency.reserve(seconds * 1000);
  bb.subscribe(k, [&](const TestMessage& m) { latency.push_back(now_ns() - m.t_pub_ns); });
  std::thread pub(publish);
  pub.join();

  print_stats("wake_jitter", wake_jitter);
  print_stats("latency", latency);

  return 0;
}

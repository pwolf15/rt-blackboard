#include <gtest/gtest.h>

#include <atomic>
#include <latch>
#include <thread>

#include "rtbb.h"

TEST(Blackboard, Basic) {
  // set, then get returns same value
  rtbb::Blackboard bb;
  auto bearing = bb.key<float>("audio/bearing");
  bb.set(bearing, 42.f);
  EXPECT_EQ(*bb.get(bearing), 42.f);
}

TEST(Blackboard, TypeMismatch) {
  rtbb::Blackboard bb;
  auto bearing = bb.key<float>("audio/bearing");
  bb.set(bearing, 42.f);
  EXPECT_THROW(bb.key<int>("audio/bearing"), std::invalid_argument);
}

TEST(Blackboard, Subscribers) {
  rtbb::Blackboard bb;
  auto bearing = bb.key<float>("audio/bearing");

  int counter = 0;
  bb.subscribe(bearing, [&](const float&) { counter++; });
  bb.subscribe(bearing, [&](const float&) { counter++; });
  bb.set(bearing, 42.f);
  bb.set(bearing, 84.f);
  EXPECT_EQ(counter, 4);
}

TEST(Blackboard, GetBeforeSet) {
  rtbb::Blackboard bb;
  auto k = bb.key<float>("audio/bearing");
  EXPECT_EQ(bb.get(k), nullptr);
}

TEST(Blackboard, OldSnapshot) {
  rtbb::Blackboard bb;
  auto bearing = bb.key<float>("audio/bearing");
  bb.set(bearing, 42.f);
  auto old = bb.get(bearing);
  bb.set(bearing, 84.f);
  EXPECT_EQ(*old, 42.f);
  EXPECT_EQ(*bb.get(bearing), 84.f);
}

TEST(Blackboard, ConcurrentTest) {
  rtbb::Blackboard bb;
  std::atomic<bool> stop{false};
  std::atomic<int> calls{0};
  auto k = bb.key<float>("audio/bearing");
  std::latch start{6};
  auto write = [&](int tid) {
    start.arrive_and_wait();
    while (!stop.load(std::memory_order_relaxed)) bb.set(k, tid * 42.f);
  };
  auto read = [&]() {
    start.arrive_and_wait();
    while (!stop.load(std::memory_order_relaxed)) {
      if (auto p = bb.get(k)) {
        EXPECT_TRUE(*p == 0.f || *p == 42.f);
      }
    }
  };
  const int NUM_SUBS = 100;
  auto subscribe = [&]() {
    start.arrive_and_wait();
    for (int i = 0; i < NUM_SUBS; ++i) {
      bb.subscribe(k, [&calls](const float&) { calls.fetch_add(1, std::memory_order_relaxed); });
    }
  };
  std::thread w0(write, 0), w1(write, 1), r0(read), r1(read), sub0(subscribe), sub1(subscribe);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  stop = true;

  w0.join();
  w1.join();
  r0.join();
  r1.join();
  sub0.join();
  sub1.join();
  EXPECT_GT(calls.load(), 0);
}

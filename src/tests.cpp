#include <gmock/gmock.h>

#include "rtbb.h"

TEST(Blackboard, Basic) {
  // set, then get returns same value
  Blackboard bb;
  auto bearing = bb.key<float>("audio/bearing");
  bb.set(bearing, 42.f);
  EXPECT_EQ(*bb.get(bearing), 42.f);
}

TEST(Blackboard, TypeMismatch) {
  // type mismatch throws
  try {
    Blackboard bb;
    auto bearing = bb.key<float>("audio/bearing");
    auto label = bb.key<std::string>("lidar1_label");
    bb.set(label, std::string("foobar"));
  } catch (std::invalid_argument const & ex) {
    std::cout << "Invalid argument " << ex.what() << "\n";
  }
}

TEST(Blackboard, Subscribers) {
  Blackboard bb;
  auto bearing = bb.key<float>("audio/bearing");
  bb.subscribe(bearing, [](const float& bval) {
    std::cout << "sub 1: " << bval << "\n";
  });
  bb.subscribe(bearing, [](const float& bval) {
    std::cout << "sub 2: " << bval << "\n";
  });
  bb.set(bearing, 42.f);
  bb.set(bearing, 84.f);
}


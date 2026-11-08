#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <type_traits>

#if defined(__SANITIZE_THREAD__)
#define RTBB_TSAN 1
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define RTBB_TSAN 1
#else
#define RTBB_TSAN 0
#endif
#else
#define RTBB_TSAN 0
#endif

#if RTBB_TSAN
#include <sanitizer/tsan_interface.h>
#endif

namespace rtbb {
template <class T>
  requires std::is_trivially_copyable_v<T>
class SeqSlot {
  public:
  void write(const T& v) {
    // memcpy into local array
    std::array<std::uint64_t, kWords> payload = {};
    std::memcpy(payload.data(), &v, sizeof(T));

    uint64_t s = seq_.load(std::memory_order_relaxed);
    seq_.store(s + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    tsan_release(&seq_);
    for (size_t i = 0; i < kWords; ++i) {
      payload_[i].store(payload[i], std::memory_order_relaxed);
    }
    seq_.store(s + 2, std::memory_order_release);
  }
  bool try_read(T& v) const {
    uint64_t seq0 = seq_.load(std::memory_order_acquire);
    if (!seq0) return false;
    if (seq0 & 1) return false;

    std::array<std::uint64_t, kWords> payload_arr = {};
    for (size_t i = 0; i < kWords; ++i) {
      payload_arr[i] = payload_[i].load(std::memory_order_relaxed);
    }
    std::atomic_thread_fence(std::memory_order_acquire);
    tsan_acquire(const_cast<void*>(static_cast<const void*>(&seq_)));
    uint64_t seq1 = seq_.load(std::memory_order_relaxed);
    bool torn_read = seq0 != seq1;
    if (!torn_read) std::memcpy(&v, payload_arr.data(), sizeof(T));
    return !torn_read;
  }
  T read() const {
    T out;
    do {
    } while (!try_read(out));
    return out;
  }
  uint64_t seq() const { return seq_.load(std::memory_order_relaxed); }

  private:
  static constexpr std::size_t kWords = (sizeof(T) + 7) / 8;
  std::array<std::atomic<std::uint64_t>, kWords> payload_;
  std::atomic<uint64_t> seq_;

  // tsan helpers
  static void tsan_release([[maybe_unused]] void* p) {
#if RTBB_TSAN
    __tsan_release(p);
#endif
  }
  static void tsan_acquire([[maybe_unused]] void* p) {
#if RTBB_TSAN
    __tsan_acquire(p);
#endif
  }
};
}  // namespace rtbb

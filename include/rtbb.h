#pragma once

#include <chrono>
#include <unordered_map>
#include <functional>
#include <typeindex>
#include <typeinfo>
#include <vector>

template <class T>
struct Key { std::size_t id; };

struct Slot {
  Slot(std::type_index t, std::string n): type(t), name(std::move(n)) {}
  std::type_index type;
  std::string name;                                   // for debugging/logging
  std::shared_ptr<const void> value;                  // latest snapshot
  std::chrono::time_point<std::chrono::steady_clock> stamp;
  std::uint64_t seq = 0;                              // update counter
  std::vector<std::function<void(const void*)>> subs;  // erased callbacks
};

class Blackboard {
public:

  // return a typed handle, Key<T>
  template <class T> Key<T> key(std::string_view name)
  {
    Key<T> k;
    std::lock_guard<std::mutex> slots_guard(slots_mutex_);
    std::lock_guard<std::mutex> index_guard(index_mutex_);

    // if key not found in index_, add to index_ and create new slot
    if (index_.find(std::string(name)) == index_.end()) {
      index_[std::string(name)] = slots_.size();
      slots_.push_back(std::make_unique<Slot>(std::type_index(typeid(T)), std::string(name)));
    }
    k.id = index_[std::string(name)];
    return k; 
  }

  // return value or empty, if not found
  template <class T> void set(Key<T> k, T value)
  {
    auto ptr = std::make_shared<const T>(std::move(value));
    std::vector<std::function<void(const void*)>> subs;   // allocate outside the lock
    {
      std::lock_guard<std::mutex> guard(slots_mutex_);
      assert(k.id < slots_.size());
      Slot& s = *slots_[k.id];
      assert(s.type == std::type_index(typeid(T)));
      s.value = ptr;                                      // implicit -> shared_ptr<const void>
      s.stamp = std::chrono::steady_clock::now();
      ++s.seq;
      subs = s.subs;                                      // snapshot the subscriber list
    }

    for (const auto& cb: slots_[k.id]->subs) cb(ptr.get());
  }

  template <class T> std::shared_ptr<const T> get(Key<T> k) const {
    if (k.id < 0 || k.id >= slots_.size()) return nullptr;
    return std::static_pointer_cast<const T>(slots_[k.id]->value);
  }

  template <class T> void subscribe(Key<T> k, std::type_identity_t<std::function<void(const T&)>> cb) {
    std::lock_guard<std::mutex> guard(slots_mutex_);
    if (k.id < 0 || k.id >= slots_.size()) return;
    slots_[k.id]->subs.push_back([cb = std::move(cb)](const void *p) { cb(*static_cast<const T*>(p)); });
  }
private:
  std::mutex slots_mutex_;
  std::mutex index_mutex_;
  std::unordered_map<std::string, std::size_t> index_;
  std::vector<std::unique_ptr<Slot>> slots_;
};

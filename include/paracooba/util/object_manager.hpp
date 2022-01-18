#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <forward_list>
#include <functional>
#include <memory>
#include <vector>

namespace parac::util {
template<typename T>
class ObjectManager {
  public:
  using Ptr = std::unique_ptr<T>;

  struct PtrWrapper {
    Ptr ptr;
    ObjectManager& mgr;
    size_t idx;
    volatile bool shouldReturn = true;

    explicit PtrWrapper(Ptr ptr, ObjectManager& mgr, size_t idx)
      : ptr(std::move(ptr))
      , mgr(mgr)
      , idx(idx) {
      assert(this->ptr);
    }
    PtrWrapper(PtrWrapper&& o) = default;
    PtrWrapper(const PtrWrapper& o) = delete;
    PtrWrapper(PtrWrapper& o) = delete;
    ~PtrWrapper() {
      if(shouldReturn)
        mgr.give(idx, std::move(ptr));
    }

    inline T* operator->() { return ptr.get(); }
    inline T& operator*() { return *ptr; }
  };

  using CreationFunc = std::function<Ptr(size_t)>;

  ObjectManager() = default;
  ~ObjectManager() {
    // Busy wait until all (created) pointers were returned, which should happen
    // fast.
    clearAll();
  }

  void terminateAll() {
    for(size_t i = 0; i < objs.size(); ++i) {
      auto& o = objs[i];
      if(o.created) {
        assert(o.nonOwningPtr);
        o.nonOwningPtr->terminate();
      }
    }
  }

  void clearAll() {
    while(std::any_of(objs.begin(), objs.end(), [](auto& o) -> bool {
      return o.created && o.ptr == nullptr;
    })) {
      terminateAll();
    }

    for(auto& o : objs) {
      o.nonOwningPtr = nullptr;
      o.created = false;
      o.ptr = nullptr;
    }
  }

  using ScheduleCB = std::function<void(ObjectManager&)>;

  void init(size_t size, CreationFunc create) {
    assert(objs.size() == 0);
    objs.resize(size);
    this->create = create;
  }

  PtrWrapper get(size_t idx) { return PtrWrapper(take(idx), *this, idx); }

  private:
  struct Internal {
    Ptr ptr;
    T* nonOwningPtr = nullptr;
    int64_t created = false;
  };

  std::vector<Internal> objs;
  CreationFunc create;

  Ptr take(size_t idx) {
    assert(idx < objs.size());
    auto& internal_obj = objs[idx];
    assert(internal_obj.ptr || !internal_obj.created);

    if(!internal_obj.created) {
      auto obj = create(idx);
      assert(obj);
      internal_obj.nonOwningPtr = obj.get();
      internal_obj.ptr = std::move(obj);
      internal_obj.created = true;
    }

    return std::move(internal_obj.ptr);
  }

  void give(size_t idx, Ptr obj) {
    assert(obj);
    assert(idx < objs.size());
    auto& internal_obj = objs[idx];
    assert(internal_obj.created);
    assert(!internal_obj.ptr);
    internal_obj.ptr = std::move(obj);
  }
};
}

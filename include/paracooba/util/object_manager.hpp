#pragma once

#include <cassert>
#include <cstddef>
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

    explicit PtrWrapper(Ptr ptr, ObjectManager& mgr, size_t idx)
      : ptr(std::move(ptr))
      , mgr(mgr)
      , idx(idx) {
      assert(this->ptr);
    }
    PtrWrapper(PtrWrapper&& o) = default;
    PtrWrapper(const PtrWrapper& o) = delete;
    PtrWrapper(PtrWrapper& o) = delete;
    ~PtrWrapper() { mgr.give(idx, std::move(ptr)); }

    inline T* operator->() { return ptr.get(); }
    inline T& operator*() { return *ptr; }
  };

  using CreationFunc = std::function<Ptr(size_t)>;

  ObjectManager() = default;
  ~ObjectManager() = default;

  void init(size_t size, CreationFunc create) {
    assert(objs.size() == 0);
    objs.resize(size);
    this->create = create;
  }

  PtrWrapper get(size_t idx) { return PtrWrapper(take(idx), *this, idx); }

  private:
  struct Internal {
    // Prevent false sharing by enlarging the struct
    Ptr ptr;
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

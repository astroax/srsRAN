/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2020 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#ifndef SRSLTE_MEM_POOL_H
#define SRSLTE_MEM_POOL_H

#include <cstdint>
#include <memory>
#include <mutex>

namespace srslte {

/// Stores provided mem blocks in a stack in an non-owning manner. Not thread-safe
class memblock_stack
{
  struct node {
    node* prev;
    explicit node(node* prev_) : prev(prev_) {}
  };

public:
  const static size_t min_memblock_size = sizeof(node);

  memblock_stack() = default;

  memblock_stack(const memblock_stack&) = delete;

  memblock_stack(memblock_stack&& other) noexcept : head(other.head) { other.head = nullptr; }

  memblock_stack& operator=(const memblock_stack&) = delete;

  memblock_stack& operator=(memblock_stack&& other) noexcept
  {
    head       = other.head;
    other.head = nullptr;
    return *this;
  }

  void push(uint8_t* block) noexcept
  {
    // printf("head: %ld\n", (long)head);
    node* next = ::new (block) node(head);
    head       = next;
    count++;
  }

  uint8_t* try_pop() noexcept
  {
    if (is_empty()) {
      return nullptr;
    }
    node* last_head = head;
    head            = head->prev;
    count--;
    return (uint8_t*)last_head;
  }

  bool is_empty() const { return head == nullptr; }

  size_t size() const { return count; }

  void clear() { head = nullptr; }

private:
  node*  head  = nullptr;
  size_t count = 0;
};

/// memblock stack that mutexes pushing/popping
class mutexed_memblock_stack
{
public:
  mutexed_memblock_stack() = default;

  mutexed_memblock_stack(const mutexed_memblock_stack&) = delete;

  mutexed_memblock_stack(mutexed_memblock_stack&& other) noexcept
  {
    std::unique_lock<std::mutex> lk1(other.mutex, std::defer_lock);
    std::unique_lock<std::mutex> lk2(mutex, std::defer_lock);
    std::lock(lk1, lk2);
    stack = std::move(other.stack);
  }

  mutexed_memblock_stack& operator=(const mutexed_memblock_stack&) = delete;

  mutexed_memblock_stack& operator=(mutexed_memblock_stack&& other) noexcept
  {
    std::unique_lock<std::mutex> lk1(other.mutex, std::defer_lock);
    std::unique_lock<std::mutex> lk2(mutex, std::defer_lock);
    std::lock(lk1, lk2);
    stack = std::move(other.stack);
    return *this;
  }

  void push(uint8_t* block) noexcept
  {
    std::lock_guard<std::mutex> lock(mutex);
    stack.push(block);
  }

  uint8_t* try_pop() noexcept
  {
    std::lock_guard<std::mutex> lock(mutex);
    uint8_t*                    block = stack.try_pop();
    return block;
  }

  bool is_empty() const noexcept { return stack.is_empty(); }

  size_t size() const noexcept
  {
    std::lock_guard<std::mutex> lock(mutex);
    return stack.size();
  }

  void clear()
  {
    std::lock_guard<std::mutex> lock(mutex);
    stack.clear();
  }

private:
  memblock_stack     stack;
  mutable std::mutex mutex;
};

/**
 * Non thread-safe object pool. Memory management is automatically handled. Relevant methods:
 * - ::make(...) - create an object whose memory is automatically managed by the pool. The object dtor returns the
 *                 allocated memory back to the pool
 * - ::reserve(N) - prereserve memory slots for faster object creation
 * @tparam T object type
 */
template <typename T, bool ThreadSafe = false>
class obj_pool
{
  const static size_t memblock_size = sizeof(T) > memblock_stack::min_memblock_size ? sizeof(T)
                                                                                    : memblock_stack::min_memblock_size;

  /// single-thread obj pool deleter
  struct obj_deleter {
    explicit obj_deleter(obj_pool<T, ThreadSafe>* pool_) : pool(pool_) {}
    void operator()(void* block)
    {
      static_cast<T*>(block)->~T();
      pool->stack.push(static_cast<uint8_t*>(block));
    }
    obj_pool<T, ThreadSafe>* pool;
  };

  // memory stack type derivation (thread safe or not)
  using stack_type = typename std::conditional<ThreadSafe, mutexed_memblock_stack, memblock_stack>::type;

  // memory stack to cache allocate memory chunks
  stack_type stack;

public:
  using obj_ptr = std::unique_ptr<T, obj_deleter>;

  ~obj_pool()
  {
    uint8_t* block = stack.try_pop();
    while (block != nullptr) {
      delete[] block;
      block = stack.try_pop();
    }
  }

  /// create new object with given arguments. If no memory is pre-reserved in the pool, malloc is called.
  template <typename... Args>
  obj_ptr make(Args&&... args)
  {
    uint8_t* block = stack.try_pop();
    if (block == nullptr) {
      block = new uint8_t[memblock_size];
    }
    new (block) T(std::forward<Args>(args)...);
    return obj_ptr(reinterpret_cast<T*>(block), obj_deleter(this));
  }

  /// Pre-reserve N memory chunks for future object allocations
  void reserve(size_t N)
  {
    for (size_t i = 0; i < N; ++i) {
      stack.push(new uint8_t[memblock_size]);
    }
  }

  size_t capacity() const { return stack.size(); }
};
template <typename T>
using mutexed_pool_obj = obj_pool<T, true>;

template <typename T>
using unique_pool_obj = typename obj_pool<T, false>::obj_ptr;
template <typename T>
using unique_mutexed_pool_obj = typename obj_pool<T, true>::obj_ptr;

} // namespace srslte

#endif // SRSLTE_MEM_POOL_H

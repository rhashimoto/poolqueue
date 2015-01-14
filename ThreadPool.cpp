/*
Copyright 2015 Shoestring Research, LLC.  All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include <cassert>
#include <map>
#include <queue>
#include <vector>

#include "ThreadPool.hpp"

namespace {
   constexpr size_t CacheLineSize = 64;

   // This concurrent queue follows "Simple, Fast, and Practical
   // Non-Blocking and Blocking Concurrent Queue Algorithms" by
   // Michael and Scott, plus tips on cache optimization from "Writing
   // A Generalized Concurrent Queue" by Herb Sutter. The primary
   // difference from those references is that when the queue is
   // empty, the head node points to itself. This allows push() to
   // know when the queue goes from empty to non-empty while
   // minimizing state contention with consumers.
   template<typename T>
   struct ConcurrentQueue {
      struct SpinLock {
         std::atomic<bool> locked_;
         char pad[CacheLineSize - sizeof(std::atomic<bool>)];
         
         SpinLock() : locked_(false) {}

         void lock() {
            while (locked_.exchange(true))
               ;
         }

         void unlock() {
            assert(locked_);
            locked_ = false;
         }
      };
      
      struct Node {
         template<typename V>
         Node(V&& value)
            : value_(std::forward<V>(value))
            , next_(nullptr) {
            static_assert(std::is_same<typename std::decay<V>::type, T>::value,
                          "inconsistent value type");
         }
         
         T value_;
         std::atomic<Node *> next_;
         char pad[CacheLineSize - sizeof(T) - sizeof(std::atomic<Node *>)];
      };

      ConcurrentQueue() {
         head_ = tail_ = new Node(T());
         head_->next_ = head_; // empty queue condition
      }

      ~ConcurrentQueue() {
         while (head_) {
            Node *node = head_;
            if (node->next_ == head_)
               node->next_ = nullptr;
            head_ = node->next_;
            delete node;
         }
      }

      // Append a new value to the tail of the queue. Returns true if
      // the queue was empty before the operation.
      template<typename X>
      bool push(X&& value) {
         Node *node = new Node(std::forward<X>(value));
         std::lock_guard<SpinLock> lock(tailLock_);

         const bool wasEmpty = tail_->next_.exchange(node);
         tail_ = node;
         return wasEmpty;
      }

      // Retrieve a value from the head of the queue into the
      // reference argument. Returns true if successful, i.e. if the
      // queue was not empty.
      bool pop(T& result) {
         using std::swap;
         std::unique_lock<SpinLock> lock(headLock_);
         
         Node *oldHead = head_;
         Node *oldNext = head_->next_;
         if (oldNext && oldNext != head_) {
            swap(result, oldNext->value_);
            head_ = oldNext;

            // Head points to self when empty.
            Node *null = nullptr;
            head_->next_.compare_exchange_strong(null, head_);

            lock.unlock();
            delete oldHead;
            return true;
         }
         else {
            return false;
         }
      }

      // Attempt to put each member variable on its own cache line.
      char pad[CacheLineSize];
      SpinLock headLock_;
      SpinLock tailLock_;
      union {
         Node *head_;
         char padHead[CacheLineSize];
      };
      union {
         Node *tail_;
         char padTail[CacheLineSize];
      };
   };
   
   struct Pimpl {
      std::vector<std::thread> threads_;
      std::map<std::thread::id, int> ids_;
      
      ConcurrentQueue<poolqueue::Promise> queue_;
      std::mutex mutex_;
      std::condition_variable condition_;
      
      Pimpl() {
         unsigned int nThreads = std::max(std::thread::hardware_concurrency(), 1U);
         setThreadCount(nThreads);
      }

      ~Pimpl() {
         setThreadCount(0);
      }

      void setThreadCount(size_t n) {
         // Wait until the queue is clear.
         synchronize().wait();

         // Add threads.
         {
            std::unique_lock<std::mutex> lock(mutex_);
            for (size_t i = threads_.size(); i < n; ++i) {
               threads_.emplace_back([this, i]() {
                     run(static_cast<int>(i));
                  });
            }
         }

         // Remove threads.
         std::vector<std::thread> remove(threads_.size() - n);
         if (!remove.empty()) {
            // Separate threads to remove.
            std::unique_lock<std::mutex> lock(mutex_);
            std::move(threads_.begin() + n, threads_.end(), remove.begin());
            threads_.erase(threads_.begin() + n, threads_.end());

            // Get every thread to test for deletion.
            auto count = std::make_shared<std::atomic<size_t>>(threads_.size() + remove.size());
            auto promise = std::make_shared<std::promise<void>>();
            std::shared_future<void> future(promise->get_future());
            for (size_t i = 0; i < threads_.size() + remove.size(); ++i) {
               poolqueue::Promise f;
               f.then([=]() {
                     if (--*count == 0)
                        promise->set_value();
                     else
                        future.wait();
                  });
               queue_.push(f);
            }
            condition_.notify_all();
         }
         
         // Wait for removed threads to exit.
         for (auto& t : remove) {
            ids_.erase(t.get_id());
            t.join();
         }
      }

      size_t getThreadCount() const {
         return threads_.size();
      }

      template<typename F>
      void enqueue(F&& f) {
         if (queue_.push(std::forward<F>(f))) {
            // If the queue was empty, we must take the lock to avoid
            // the race where all threads have found the queue empty
            // but not yet issued a wait.
            std::lock_guard<std::mutex> lock(mutex_);
            condition_.notify_one();
         }
         else {
            // We don't have to take the lock here because it can't
            // deadlock. If there are multiple jobs in the queue then
            // at least one thread is active, the thread to run the
            // first job. That thread does not need a notification to
            // run the next job, even if all the other threads miss
            // the notification (by being just before the wait when
            // the jobs are added). This may not be optimally parallel
            // but it should make progress.
            condition_.notify_one();
         }
      }

      void run(int i) {
         {
            std::lock_guard<std::mutex> lock(mutex_);
            ids_[std::this_thread::get_id()] = i;
         }
         
         poolqueue::Promise f;
         while (true) {
            // Attempt to run the next task from the queue.
            if (queue_.pop(f)) {
               f.resolve();
            }
            else {
               // The queue was empty so we will wait for a condition
               // notification, which requires a lock.
               std::unique_lock<std::mutex> lock(mutex_);

               // Check the queue in case an item was added and the
               // notification fired before the lock was acquired.
               if (queue_.pop(f)) {
                  // Don't call user code with the lock.
                  lock.unlock();
                  f.resolve();
               }

               // The queue is now known to be empty. If this thread
               // is still part of the pool then wait.
               else if (i < static_cast<int>(threads_.size())) {
                  condition_.wait(lock);
               }

               // Otherwise the thread is slated for deletion.
               else
                  break;
            }
         }
      }

      std::shared_future<void> synchronize() {
         // Return a completed future if there are no threads.
         if (threads_.empty()) {
            std::promise<void> promise;
            promise.set_value();
            return std::shared_future<void>(promise.get_future());
         }

         // Get every thread to test for deletion. Acquiring the lock
         // means that when it is released all threads will be active
         // outside the lock or waiting on the condition and newly
         // notified. Don't use enqueue() here as it does not
         // necessarily acquire the lock, which is okay for normal use
         // but can produce deadlock with this blocking lambda.
         std::unique_lock<std::mutex> lock(mutex_);
         auto count = std::make_shared<std::atomic<size_t>>(threads_.size());
         auto promise = std::make_shared<std::promise<void>>();
         std::shared_future<void> future(promise->get_future());
         for (size_t i = 0; i < threads_.size(); ++i) {
            poolqueue::Promise f;
            f.then([=]() {
                  // If this is the last thread to decrement the
                  // counter then release all threads.
                  if (--*count == 0)
                     promise->set_value();

                  // Otherwise block here to guarantee that every
                  // thread runs this lambda.
                  else
                     future.wait();
               });
            queue_.push(f);
         }
         condition_.notify_all();
         
         return future;
      }
      
      static Pimpl& singleton() {
         static Pimpl pimpl;
         return pimpl;
      }
   };
}

void
poolqueue::ThreadPool::enqueue(Promise&& f) {
   Pimpl& pimpl = Pimpl::singleton();
   pimpl.enqueue(std::move(f));
}

int
poolqueue::ThreadPool::threadId() {
   Pimpl& pimpl = Pimpl::singleton();
   auto i = pimpl.ids_.find(std::this_thread::get_id());
   return i != pimpl.ids_.end() ? i->second : -1;
}

void
poolqueue::ThreadPool::setThreadCount(int n) {
   Pimpl& pimpl = Pimpl::singleton();
   pimpl.setThreadCount(static_cast<size_t>(n));
}

int
poolqueue::ThreadPool::getThreadCount() {
   Pimpl& pimpl = Pimpl::singleton();
   return static_cast<int>(pimpl.getThreadCount());
}

std::shared_future<void>
poolqueue::ThreadPool::synchronize() {
   Pimpl& pimpl = Pimpl::singleton();
   return pimpl.synchronize();
}

struct poolqueue::ThreadPool::Strand::Pimpl {
   std::mutex mutex_;
   std::queue<Promise> queue_;
   std::condition_variable condition_;
   std::thread::id currentId_;

   template<typename F>
   void enqueue(F&& f) {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.push(std::forward<F>(f));
      if (queue_.size() == 1)
         ThreadPool::post([this]() { execute(); });
   }

   void execute() {
      // Get the Promise at the top of the queue. We move it from the
      // queue because we don't want to hold the queue lock. The queue
      // is not popped here so enqueue() will not post.
      Promise p;
      {
         std::lock_guard<std::mutex> lock(mutex_);
         currentId_ = std::this_thread::get_id();
         assert(!queue_.empty());
         p.swap(queue_.front());
      }

      // Always run this code after executing the function.
      struct Finally {
         std::function<void()> f_;
         Finally(const std::function<void()>& f) : f_(f) {}
         ~Finally() { f_(); }
      } finally([this]() {
            std::lock_guard<std::mutex> lock(mutex_);
            currentId_ = std::thread::id();
            queue_.pop();
            if (!queue_.empty())
               ThreadPool::post([this]() { execute(); });
            else
               condition_.notify_one();
         });

      // Execute the function.
      p.resolve();
   }
};

poolqueue::ThreadPool::Strand::Strand()
   : pimpl(new Strand::Pimpl) {
   // This call which apparently does nothing actually ensures that
   // statically created strands are destroyed before the thread pool.
   ThreadPool::getThreadCount();
}

poolqueue::ThreadPool::Strand::~Strand() {
   assert(ThreadPool::threadId() < 0);
   synchronize().wait();
}

std::thread::id
poolqueue::ThreadPool::Strand::currentId() const {
   std::lock_guard<std::mutex> lock(pimpl->mutex_);
   return pimpl->currentId_;
}

void
poolqueue::ThreadPool::Strand::enqueue(Promise&& f) {
   pimpl->enqueue(f);
}

std::shared_future<void>
poolqueue::ThreadPool::Strand::synchronize() {
   auto p = std::make_shared<std::promise<void>>();
   std::shared_future<void> result(p->get_future());
   post([=]() {
         p->set_value();
      });
   return result;
}

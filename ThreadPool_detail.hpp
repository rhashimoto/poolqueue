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

namespace poolqueue {
   namespace detail {

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

   }
}

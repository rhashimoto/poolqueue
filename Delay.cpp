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
#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <map>
#include <thread>
#include <vector>

#include "Delay.hpp"

using poolqueue::Delay;
using poolqueue::Promise;

namespace {
   
   struct Pimpl {
      std::multimap<std::chrono::time_point<std::chrono::steady_clock>, Promise> queue_;

      // A dedicated thread waits for the next timer expiration and
      // configuration change conditions.
      bool running_;
      std::mutex mutex_;
      std::condition_variable condition_;
      std::thread thread_;
      
      Pimpl()
         : running_(true)
         , thread_([this]() { run(); }) {
      }

      Pimpl(const Pimpl&) {
         assert(false);
      }
      
      ~Pimpl() {
         {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
            condition_.notify_all();
         }

         thread_.join();
      }
      
      void insert(const std::chrono::steady_clock::duration& duration, const Promise& p) {
         // Add the entry to the queue.
         std::lock_guard<std::mutex> lock(mutex_);
         queue_.insert(std::make_pair(std::chrono::steady_clock::now() + duration, p));

         // Notify the thread if the new entry is next.
         if (queue_.begin()->second == p)
            condition_.notify_one();
      }

      bool cancel(const Promise& p, const std::exception_ptr& e) {
         // If a match is found, it will be swapped with target and
         // the subsequent reject will be propagated to dependencies.
         // If a match is not found, the result is reset to false.
         bool result = true;
         Promise target;
         target.except([&](const std::exception_ptr&) {
               result = false;
            });
         
         {
            // Linear search over queue entries.
            std::lock_guard<std::mutex> lock(mutex_);
            auto i = std::find_if(
               queue_.begin(), queue_.end(),
               [&](const decltype(queue_)::value_type& value) {
                  return value.second == p;
               });
            if (i != queue_.end()) {
               target.swap(i->second);
               queue_.erase(i);
            }
         }

         target.reject(e);
         return result;
      }
      
      void run() {
         while (running_) {
            // Wait until signaled or the next timer expiration.
            std::unique_lock<std::mutex> lock(mutex_);
            if (queue_.empty())
               condition_.wait(lock);
            else
               condition_.wait_until(lock, queue_.begin()->first);

            // Remove expired entries, saving the callbacks.
            std::vector<Promise> ready;
            const auto now = std::chrono::steady_clock::now();
            const auto upper = queue_.upper_bound(now);
            for (auto i = queue_.begin(); i != upper; ++i)
               ready.push_back(std::move(i->second));
            queue_.erase(queue_.begin(), upper);
            
            // Execute the callbacks outside the lock.
            lock.unlock();
            for (auto& p : ready)
               p.resolve();
         }

         // Notify outstanding entries of cancellation.
         for (auto& value : queue_)
            value.second.reject(std::make_exception_ptr(Delay::cancelled()));
      }

      static Pimpl& singleton() {
         static Pimpl pimpl;
         return pimpl;
      }
   };
   
}

bool
poolqueue::Delay::cancel(const Promise& p, const std::exception_ptr& e) {
   Pimpl& pimpl = Pimpl::singleton();
   return pimpl.cancel(p, e);
}

void
poolqueue::Delay::createImpl(
   const std::chrono::steady_clock::duration& duration,
   const Promise& p) {
   Pimpl& pimpl = Pimpl::singleton();
   pimpl.insert(duration, p);
}

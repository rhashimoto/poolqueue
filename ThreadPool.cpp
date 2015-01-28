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
#include <deque>
#include <map>
#include <queue>
#include <vector>

#include "ThreadPool.hpp"

struct poolqueue::ThreadPool::Pimpl {
   std::vector<std::thread> threads_;
   std::deque<std::atomic<bool>> running_;
   std::map<std::thread::id, int> ids_;
      
   detail::ConcurrentQueue<poolqueue::Promise> queue_;
   std::mutex mutex_;
   std::condition_variable condition_;
      
   Pimpl(unsigned int nThreads) {
      if (!nThreads)
         nThreads = std::max(std::thread::hardware_concurrency(), 1U);
      setThreadCount(nThreads);
   }

   ~Pimpl() {
      setThreadCount(0);
   }

   void setThreadCount(size_t n) {
      // Add threads.
      const auto oldCount = threads_.size();
      if (n > oldCount) {
         synchronize().wait();

         // Use copy-and-swap for exception safety.
         decltype(ids_) ids = ids_;
         std::unique_lock<std::mutex> lock(mutex_);
         try {
            for (size_t i = oldCount; i < n; ++i) {
               // Any of these statements can throw.
               running_.emplace_back(true);
               threads_.emplace_back([this, i]() { run(i); });
               ids[threads_.back().get_id()] = static_cast<int>(i);
            }

            ids_.swap(ids);
         }
         catch (...) {
            // Tell newly launched threads to exit.
            running_.resize(oldCount);

            // Join any newly launched threads.
            lock.unlock();
            threads_.resize(oldCount);
            throw;
         }
      }

      // Remove threads.
      else if (n < oldCount) {
         synchronize().wait();

         // Separate threads to remove. A lock is not acquired because
         // we assume no tasks are simultaneously posted.
         std::vector<std::thread> remove(oldCount - n);
         std::move(threads_.begin() + n, threads_.end(), remove.begin());
         threads_.erase(threads_.begin() + n, threads_.end());

         // Signal threads. Here we need the lock to make sure that
         // all threads will test the condition.
         {
            std::lock_guard<std::mutex> lock(mutex_);
            for (size_t i = 0; i < remove.size(); ++i)
               running_[n + i] = false;
            condition_.notify_all();
         }
            
         // Wait for removed threads to exit.
         for (auto& t : remove) {
            ids_.erase(t.get_id());
            t.join();
         }

         running_.resize(n);
      }

      assert(threads_.size() == n);
      assert(running_.size() == n);
      assert(std::all_of(running_.begin(), running_.end(), [](bool b) { return b; }));
      assert(ids_.size() == n);
   }

   size_t getThreadCount() const {
      return threads_.size();
   }

   void enqueue(Promise& p) {
      if (queue_.push(p)) {
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

   void run(size_t i) {
      {
         // Exit cleanly if anything in start up failed.
         std::lock_guard<std::mutex> lock(mutex_);
         if (i >= running_.size())
            return;
      }
         
      auto& running = running_[i];
      poolqueue::Promise p;
      while (running) {
         // Attempt to run the next task from the queue.
         if (queue_.pop(p)) {
            p.settle();
         }
         else {
            // The queue was empty so we will wait for a condition
            // notification, which requires a lock.
            std::unique_lock<std::mutex> lock(mutex_);

            // Check the queue in case an item was added and the
            // notification fired before the lock was acquired.
            if (queue_.pop(p)) {
               // Don't call user code with the lock.
               lock.unlock();
               p.settle();
            }

            // The queue is now known to be empty.
            else if (running) {
               condition_.wait(lock);
            }
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
         poolqueue::Promise occupier;
         occupier.then([=]() {
               // If this is the last thread to decrement the
               // counter then release all threads.
               if (--*count == 0)
                  promise->set_value();

               // Otherwise block here to guarantee that every
               // thread runs this lambda.
               else
                  future.wait();
            });
         queue_.push(occupier);
      }
      condition_.notify_all();
         
      return future;
   }
};

poolqueue::ThreadPool::ThreadPool(unsigned int nThreads)
   : pimpl(new Pimpl(nThreads)) {
}

poolqueue::ThreadPool::~ThreadPool() {
}

void
poolqueue::ThreadPool::enqueue(Promise& f) {
   pimpl->enqueue(f);
}

int
poolqueue::ThreadPool::index() {
   auto i = pimpl->ids_.find(std::this_thread::get_id());
   return i != pimpl->ids_.end() ? i->second : -1;
}

void
poolqueue::ThreadPool::setThreadCount(int n) {
   if (n <= 0)
      throw std::invalid_argument("thread count must be > 0");
   pimpl->setThreadCount(static_cast<size_t>(n));
}

int
poolqueue::ThreadPool::getThreadCount() {
   return static_cast<int>(pimpl->getThreadCount());
}

std::shared_future<void>
poolqueue::ThreadPool::synchronize() {
   return pimpl->synchronize();
}

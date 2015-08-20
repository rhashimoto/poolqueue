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
#ifndef poolqueue_ThreadPool_hpp
#define poolqueue_ThreadPool_hpp

#include <cassert>
#include <deque>
#include <map>
#include <future>
#include <thread>
#include <vector>

#include "Promise.hpp"
#include "ThreadPool_detail.hpp"

namespace poolqueue {

   // Thread pool using Promises.
   //
   // This template class implements a ThreadPool that schedules
   // function objects and returns Promises that settle when the
   // associated function completes execution.
   //
   // The template argument selects a thread-safe queue class (in
   // certain situations a LIFO queue is preferable to the default
   // FIFO queue). It must be a default-constructible class with
   // methods:
   //
   //   bool push(Promise& p);
   //   bool pop(Promise& p);
   //
   // push() should return true if the queue was empty. pop() should
   // return true if successful.
   template<typename Q, bool FIFO = true>
   class ThreadPoolT {
   public:
      // Construct a pool.
      // @nThreads  Number of threads in the pool. The default
      //            is the hardware concurrency.
      ThreadPoolT(unsigned int nThreads = std::max(std::thread::hardware_concurrency(), 1U)) {
         setThreadCount(nThreads);
      }

      ThreadPoolT(const ThreadPoolT&) = delete;
      ThreadPoolT(ThreadPoolT&&) = default;
      ThreadPoolT& operator=(const ThreadPoolT&) = delete;
      ThreadPoolT& operator=(ThreadPoolT&&) = default;
      
      // Destructor.
      ~ThreadPoolT() {
         setThreadCountImpl(0);
      }

      // Post (enqueue) a job.
      // @f Function or functor to run.
      //
      // This static method enqueues a function to execute
      // asynchronously. A Promise is returned that either fulfils
      // with the value returned by the function or rejects with the
      // exception thrown by the function.
      //
      // Note that calling .then()/.except() on the returned Promise
      // will not necessary continue on a ThreadPool thread. If the
      // posted function happens to be executed before a dependent
      // Promise is attached, a callback on the dependent Promise
      // would be executed synchronously with attachment.
      //
      // @return Promise that fulfils or rejects with the outcome
      //         of the function argument.
      template<typename F>
      Promise post(F&& f) {
         typedef typename detail::CallableTraits<F>::ArgumentType Argument;
         typedef typename detail::CallableTraits<F>::ResultType Result;
         static_assert(std::is_same<Argument, void>::value,
                       "function must take no argument");
         static_assert(!std::is_same<Result, void>::value,
                       "function must return a value");
         
         Promise p(std::forward<F>(f));
         enqueue(p);
         return p;
      }

      // Ensure that a job runs in the thread pool.
      // @f Function or functor to run.
      //
      // Execute a function synchronously if currently running in a
      // ThreadPool thread, otherwise, post it.
      //
      // @return Promise that fulfils or rejects with the outcome
      //         of the function argument.
      template<typename F>
      Promise dispatch(F&& f) {
         typedef typename detail::CallableTraits<F>::ArgumentType Argument;
         typedef typename detail::CallableTraits<F>::ResultType Result;
         static_assert(std::is_same<Argument, void>::value,
                       "function must take no argument");
         static_assert(!std::is_same<Result, void>::value,
                       "function must return a value");

         if (index() >= 0)
            return Promise(std::forward<F>(f)).settle();
         else
            return post(std::forward<F>(f));
      }

      // Wrap a function.
      // @f Function or functor to wrap.
      //
      // Given an input function, return a function that runs the
      // input function in a ThreadPool thread.
      template<typename F>
      std::function<Promise()> wrap(const F& f) {
         typedef typename detail::CallableTraits<F>::ArgumentType Argument;
         typedef typename detail::CallableTraits<F>::ResultType Result;
         static_assert(std::is_same<Argument, void>::value,
                       "function must take no argument");
         static_assert(!std::is_same<Result, void>::value,
                       "function must return a value");

         return std::bind(&ThreadPoolT::dispatch<const F&>, this, f);
      }

      // Get thread index.
      //
      // If the current context is a ThreadPool thread, then return
      // its 0-based index, otherwise -1.
      int index() {
         auto i = ids_.find(std::this_thread::get_id());
         return i != ids_.end() ? i->second : -1;
      }

      // Get number of threads in the pool.
      //
      // @return Number of threads.
      unsigned int getThreadCount() {
         return static_cast<unsigned int>(threads_.size());
      }

      // Set number of threads in the pool.
      //
      // The number of threads can be dynamically adjusted. By default
      // it is the number of hardware threads determined by
      // std::thread::hardware_concurrency(). setThreadCount() must
      // not be called concurrently with any other member function.
      void setThreadCount(unsigned int n) {
         if (n <= 0)
            throw std::invalid_argument("count must be > 0");
         setThreadCountImpl(n);
      }

      // Synchronize threads.
      //
      // Ensure that any function scheduled before synchronize()
      // completes before any function scheduled afterwards
      // starts. wait() can be called on the returned future to
      // block until the queue is flushed, but only if not on a
      // ThreadPool thread (otherwise deadlock will occur).
      //
      // This works correctly only with a FIFO queue.
      //
      // @return Future whose result is set when the queue is empty.
      std::shared_future<void> synchronize() {
         if (!FIFO)
            throw std::logic_error("underlying queue is not FIFO");
         
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

                  return nullptr;
               });
            queue_.push(occupier);
         }
         condition_.notify_all();
         
         return future;
      }

   private:
      Q queue_;

      std::vector<std::thread> threads_;
      std::deque<std::atomic<bool>> running_;
      std::map<std::thread::id, int> ids_;
      
      std::mutex mutex_;
      std::condition_variable condition_;

      void setThreadCountImpl(unsigned int n) {
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
                  threads_.emplace_back(std::bind(&ThreadPoolT::run, this, i));
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

      void enqueue(Promise& p) {
         // If the queue was empty, we must take the lock to avoid the
         // race where all threads have found the queue empty but not
         // yet issued a wait.
         //
         // If the queue was not empty we don't have to take the lock
         // because it can't deadlock. If there are multiple jobs in
         // the queue then at least one thread is active, the thread
         // to run the first job. That thread does not need a
         // notification to run the next job, even if all the other
         // threads miss the notification (by being just before the
         // wait when the jobs are added). This may not be optimally
         // parallel but it should make progress.
         std::unique_lock<std::mutex> lock(mutex_, std::defer_lock);
         if (queue_.push(p))
            lock.lock();
         condition_.notify_one();
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
               // The queue was empty so we will wait
               // for a condition notification, which
               // requires a lock.
               std::unique_lock<std::mutex> lock(mutex_);

               // Check the queue in case an item was
               // added and the notification fired
               // before the lock was acquired.
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
   };

   typedef ThreadPoolT<detail::ConcurrentQueue<Promise> > ThreadPool;
} // namespace poolqueue

#endif // poolqueue_ThreadPool_hpp

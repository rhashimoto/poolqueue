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

#include <future>
#include <thread>

#include "Promise.hpp"

namespace poolqueue {

   // Thread pool using Promises.
   //
   // This class contains static member functions to access and
   // control a basic thread pool built using Promises.
   class ThreadPool {
   public:
      // Post (enqueue) a job.
      // @f Function or functor to run.
      //
      // This static method enqueues a function to execute
      // asynchronously. A Promise is returned that either resolves
      // with the value returned by the function or rejects with the
      // exception thrown by the function.
      //
      // Note that calling .then()/.except() on the returned Promise
      // will not necessary continue on a ThreadPool thread. If the
      // posted function happens to be executed before
      // .then()/.except(), the continuations will execute
      // synchronously.
      //
      // @return Promise that resolves or rejects with the outcome
      //         of the function argument.
      template<typename F>
      static Promise post(F&& f) {
         Promise p(std::forward<F>(f));
         enqueue(p);
         return p;
      }

      // Ensure that a job runs in the thread pool
      // @f Function or functor to run.
      //
      // Execute a function synchronously if currently running in a
      // ThreadPool thread, otherwise, post it.
      //
      // @return Promise that resolves or rejects with the outcome
      //         of the function argument.
      template<typename F>
      static Promise dispatch(F&& f) {
         if (threadId() >= 0)
            return Promise(std::forward<F>(f));
         else
            return post(std::forward<F>(f));
      }

      // Wrap a function.
      // @f Function or functor to wrap.
      //
      // Given an input function, return a function that runs the
      // input function in a ThreadPool thread.
      template<typename F>
      static std::function<Promise()> wrap(const F& f) {
         return [=]() {
            return dispatch(f);
         };
      }

      // Get thread id.
      //
      // If the current context is a ThreadPool thread, then return
      // its 0-based index, otherwise -1.
      static int threadId();

      // Get number of threads in the pool.
      //
      // @return Number of threads.
      static int getThreadCount();

      // Set number of threads in the pool.
      //
      // The number of threads can be dynamically adjusted. By
      // default it is the number of hardware threads determined by
      // std::thread::hardware_concurrency(). setThreadCount() must
      // not be called concurrently with any other ThreadPool
      // function.
      static void setThreadCount(int n);

      // Synchronize threads.
      //
      // Ensure that any function scheduled before synchronize()
      // completes before any function scheduled afterwards
      // starts. wait() can be called on the returned future to
      // block until the queue is flushed, but only if not on a
      // ThreadPool thread (otherwise deadlock will occur).
      //
      // @return Future whose result is set when the queue is empty.
      static std::shared_future<void> synchronize();

      // Serialized execution.
      //
      // Functions executed via a Strand instance are guaranteed to
      // run in the order they were posted without overlapping.
      class Strand {
      public:
         Strand();
         Strand(const Strand&) = delete;
         Strand(Strand&& other) = default;

         // Destructor.
         //
         // The destructor blocks until all queued functions have
         // been executed, so it must not be invoked on a ThreadPool
         // thread.
         ~Strand();

         // Post (enqueue) a job.
         // @f Function or functor to run.
         //
         // This methods works similarly to ThreadPool::post()
         // except execution must obey the Strand guarantees.
         //
         // @return Promise that resolves or rejects with the outcome
         //         of the function argument.
         template<typename F>
         Promise post(F&& f) {
            Promise p(std::forward<F>(f));
            enqueue(p);
            return p;
         }

         // Post (enqueue) a job.
         // @f Function or functor to run.
         //
         // This methods works similarly to ThreadPool::dispatch()
         // except execution must obey the Strand guarantees.
         //
         // @return Promise that resolves or rejects with the outcome
         //         of the function argument.
         template<typename F>
         Promise dispatch(F&& f) {
            if (std::this_thread::get_id() == currentId())
               return Promise().resolve().then(std::forward<F>(f));
            else
               return post(std::forward<F>(f));
         }

         // Wrap a function.
         // @f Function or functor to wrap.
         //
         // Given an input function, return a function that runs the
         // input function in the strand.
         template<typename F>
         std::function<Promise()> wrap(const F& f) {
            return [this, f]() {
               return dispatch(f);
            };
         }

         // Synchronize threads.
         //
         // Because a strand guarantees non-concurrency, it is
         // technically always synchronized. However, the returned
         // future provides a way to block until the strand is
         // flushed. Blocking on a ThreadPool thread is discouraged
         // as that may cause deadlock.
         //
         // @return Future whose result is set when the queue is empty.
         std::shared_future<void> synchronize();
            
         Strand& operator=(const Strand&) = delete;
         Strand& operator=(Strand&& other) = default;
            
         void swap(Strand& other) {
            pimpl.swap(other.pimpl);
         }

      private:
         struct Pimpl;
         std::unique_ptr<Pimpl> pimpl;

         std::thread::id currentId() const;
         void enqueue(Promise& f);
      };

   private:
      static void enqueue(Promise& f);
   };

   inline void swap(ThreadPool::Strand& a, ThreadPool::Strand& b) {
      a.swap(b);
   }
      
} // namespace poolqueue

#endif // poolqueue_ThreadPool_hpp

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
#include "ThreadPool_detail.hpp"

namespace poolqueue {

   // Thread pool using Promises.
   //
   // This class contains static member functions to access and
   // control a basic thread pool built using Promises.
   class ThreadPool {
   public:
      // Construct a pool.
      // @nThreads  Number of threads in the pool. The default
      //            is the hardware concurrency.
      ThreadPool(unsigned int nThreads = 0);

      // Destructor.
      ~ThreadPool();
      
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
         static_assert(std::is_same<Argument, void>::value,
                       "function must take no argument");
         
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
         static_assert(std::is_same<Argument, void>::value,
                       "function must take no argument");

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
         static_assert(std::is_same<Argument, void>::value,
                       "function must take no argument");

         return [=]() {
            return dispatch(f);
         };
      }

      // Get thread index.
      //
      // If the current context is a ThreadPool thread, then return
      // its 0-based index, otherwise -1.
      int index();

      // Get number of threads in the pool.
      //
      // @return Number of threads.
      int getThreadCount();

      // Set number of threads in the pool.
      //
      // The number of threads can be dynamically adjusted. By default
      // it is the number of hardware threads determined by
      // std::thread::hardware_concurrency(). setThreadCount() must
      // not be called concurrently with any other member function.
      void setThreadCount(int n);

      // Synchronize threads.
      //
      // Ensure that any function scheduled before synchronize()
      // completes before any function scheduled afterwards
      // starts. wait() can be called on the returned future to
      // block until the queue is flushed, but only if not on a
      // ThreadPool thread (otherwise deadlock will occur).
      //
      // @return Future whose result is set when the queue is empty.
      std::shared_future<void> synchronize();

   private:
      struct Pimpl;
      std::unique_ptr<Pimpl> pimpl;
      
      void enqueue(Promise& f);
   };

} // namespace poolqueue

#endif // poolqueue_ThreadPool_hpp

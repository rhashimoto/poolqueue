// Copyright 2014 Shoestring Research, LLC.  All rights reserved.
#ifndef poolqueue_ThreadPool_hpp
#define poolqueue_ThreadPool_hpp

#include <future>
#include <thread>

#include "Promise.hpp"

namespace poolqueue {

   class ThreadPool {
   public:
      // Execute a function asynchronously. A Promise is returned
      // that either resolves with the value returned by the
      // function or rejects with the exception thrown by the
      // function.
      //
      // Note that calling .then()/.except() on the returned
      // Promise will not necessary continue on a ThreadPool
      // thread. If the posted function happens to be executed
      // before .then()/.except(), the continuations will execute
      // synchronously.
      template<typename F>
      static Promise post(F&& f) {
         Promise p;
         Promise then = p.then(std::forward<F>(f));
         enqueue(std::move(p));
         return then;
      }

      // Execute a function synchronously if currently running in a
      // ThreadPool thread, otherwise, post it.
      template<typename F>
      static Promise dispatch(F&& f) {
         if (threadId() >= 0)
            return Promise().resolve().then(std::forward<F>(f));
         else
            return post(std::forward<F>(f));
      }

      // Given an input function, return a function that runs the
      // input function in a ThreadPool thread.
      template<typename F>
      static std::function<Promise()> wrap(const F& f) {
         return [=]() {
            return dispatch(f);
         };
      }

      // If the current context is a ThreadPool thread, then return
      // its 0-based index, otherwise -1.
      static int threadId();

      // The number of threads can be dynamically adjusted. By
      // default it is the number of hardware threads determined by
      // std::thread::hardware_concurrency(). setThreadCount() must
      // not be called concurrently with any other ThreadPool
      // function.
      static void setThreadCount(int n);
      static int getThreadCount();

      // Ensure that any function scheduled before synchronize()
      // completes before any function scheduled afterwards
      // starts. wait() can be called on the returned future to
      // block until the queue is flushed, but only if not on a
      // ThreadPool thread (otherwise deadlock will occur).
      static std::shared_future<void> synchronize();

      // Functions executed via a Strand instance are guaranteed to
      // run in the order they were posted without overlapping.
      class Strand {
      public:
         Strand();
         Strand(const Strand&) = delete;
         Strand(Strand&& other) = default;
            
         // The destructor blocks until all queued functions have
         // been executed, so it must not be invoked on a ThreadPool
         // thread.
         ~Strand();

         template<typename F>
         Promise post(F&& f) {
            Promise p;
            Promise then = p.then(std::forward<F>(f));
            enqueue(std::move(p));
            return then;
         }

         template<typename F>
         Promise dispatch(F&& f) {
            if (std::this_thread::get_id() == currentId())
               return Promise().resolve().then(std::forward<F>(f));
            else
               return post(std::forward<F>(f));
         }

         template<typename F>
         std::function<Promise()> wrap(const F& f) {
            return [this, f]() {
               return dispatch(f);
            };
         }

         // Because a strand guarantees non-concurrency, it is
         // technically always synchronized. However, the returned
         // future provides a way to block until the strand is
         // flushed. Blocking on a ThreadPool thread is discouraged
         // as that may cause deadlock.
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
         void enqueue(Promise&& f);
      };

   private:
      static void enqueue(Promise&& f);
   };

   inline void swap(ThreadPool::Strand& a, ThreadPool::Strand& b) {
      a.swap(b);
   }
      
} // namespace poolqueue

#endif // poolqueue_ThreadPool_hpp

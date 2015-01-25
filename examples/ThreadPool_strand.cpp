#include <atomic>
#include <future>
#include <iostream>
#include <mutex>

#include <poolqueue/ThreadPool.hpp>

// A Strand invokes functions in the order they are posted
// asynchronously (on the ThreadPool) with no concurrency. This
// can be useful, for example, in managing access to a resource
// without blocking.
class Strand {
   std::mutex mutex_;

   // The last posted task provides the place to add the next task.
   poolqueue::Promise tail_;
public:
   Strand()
      : tail_(poolqueue::Promise().settle()) {
      // If you have a statically constructed instance of Strand, it
      // should be destroyed after the global ThreadPool state which
      // is also statically constructed. This call (which admittedly
      // looks meaningless) ensures that ordering.
      poolqueue::ThreadPool::getThreadCount();
   }
   
   Strand(const Strand&) = delete;
   Strand(Strand&& other) = default;

   template<typename F>
   poolqueue::Promise post(F&& f) {
      std::lock_guard<std::mutex> lock(mutex_);

      // When the previously last task completes...
      poolqueue::Promise p = tail_.then([this, f]() {
            // ...schedule the input function. The returned Promise
            // will settle p with the result of the function.
            return poolqueue::ThreadPool::post(f);
         });

      // This is a minor optimization that just guarantees we won't be
      // calling then() or except() on the Promise currently in tail_
      // from now on. The behavior is correct whether this line is
      // present or not.
      tail_.close();

      // Update the Promise to chain onto. We can't use the Promise
      // returned to the user because the user could close it, so
      // create a dependent Promise and use that.
      tail_ = p.then([](){}, [](){});
      return p;
   }
};

int main() {
   Strand strand;

   // Schedule a bunch of tasks on the strand. Verify that they
   // execute in order and that they do not overlap.
   const int n = 16;
   std::atomic<int> counter(0);
   for (int i = 0; i < n; ++i) {
      strand.post([=, &counter]() {
            if (counter != i)
               std::cout << "out of order\n";

            // I could access some resource here and be sure that no
            // other task in the strand is simultaneously using it.
            // For example, stdout won't be garbled here.
            std::cout << "task ";
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            std::cout << i;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            std::cout << " on thread ";
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            std::cout << std::this_thread::get_id();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            std::cout << '\n';
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
               
            if (counter != i)
               std::cout << "overlap\n";

            ++counter;

            // Throw an exception every now and then to make sure
            // constraints are still satisfied.
            if ((counter % 3) == 0)
               throw std::runtime_error("ignore me");
         }).except([](const std::exception_ptr& e){
               // Important! This code is not running in the strand!
               // Only function objects supplied to post() obey the
               // strand guarantees.
               try {
                  if (e)
                     std::rethrow_exception(e);
               }
               catch (const std::runtime_error& e) {
                  if (e.what() != std::string("ignore me"))
                     std::unexpected();
               }
            });
   }

   // Use a std::promise to block until all the tasks are
   // complete. std::promise is good at blocking synchronization while
   // PoolQueue Promise is good at non-blocking synchronization. Here
   // we want to block so std::promise is appropriate.
   auto done = std::make_shared<std::promise<void>>();
   strand.post([=]() {
         done->set_value();
      });
   done->get_future().wait();

   return 0;
}

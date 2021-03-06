#include <atomic>
#include <future>
#include <iostream>
#include <mutex>

#include <poolqueue/ThreadPool.hpp>

// A Strand invokes functions in the order they are posted
// asynchronously (on the ThreadPool) with no concurrency. This
// can be useful, for example, in managing access to a resource
// without blocking.
template<typename TP>
class Strand {
   TP& tp_;
   std::mutex mutex_;

   // The last posted task provides the place to add the next task.
   poolqueue::Promise tail_;
public:
   Strand(TP& threadPool)
      : tp_(threadPool)
      , tail_(poolqueue::Promise().settle()) {
   }

   // Movable but not copyable (std::mutex member can't be copied).
   Strand(Strand&&) = default;
   Strand& operator=(Strand&&) = default;
   
   template<typename F>
   poolqueue::Promise post(F&& f) {
      std::lock_guard<std::mutex> lock(mutex_);

      // When the previously last task completes...
      poolqueue::Promise p = tail_.then([this, f]() {
         // ...schedule the input function. The returned Promise
         // will settle p with the result of the function.
         return tp_.post(f);
      });

      // This is a minor optimization that just guarantees we won't be
      // calling then() or except() on the Promise currently in tail_
      // from now on. The behavior is correct whether this line is
      // present or not.
      tail_.close();

      // Update the Promise to chain onto. We can't use the Promise
      // returned to the user because the user could close it, so
      // create a dependent Promise and use that.
      tail_ = p.then([](){ return nullptr; }, [](){ return nullptr; });
      return p;
   }
};

int main() {
   poolqueue::ThreadPool tp;
   Strand<decltype(tp)> strand(tp);

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
         return i;
      }).then(
         [=](int&& value) {
            // Important! This code is not running in the Strand!
            // Chaining to Strand::post() obeys the Promise
            // guarantee of executing after the posted function
            // but may overlap execution with the next Strand
            // function.
            //
            // Declaring an rvalue argument implicitly closes the
            // previous Promise. This is an optional optimization,
            // included here to prove that closing it does not
            // prevent additional posts to the Strand.
            if (value != i)
               std::unexpected();
            
            return nullptr;
         },
         [](const std::exception_ptr& e){
            // Important! This code is not running in the strand!
            try {
               if (e)
                  std::rethrow_exception(e);
            }
            catch (const std::runtime_error& e) {
               if (e.what() != std::string("ignore me"))
                  std::unexpected();
            }

            return nullptr;
         });
   }

   // Use a std::promise to block until all the tasks are
   // complete. std::promise is good at blocking synchronization while
   // PoolQueue Promise is good at non-blocking synchronization. Here
   // we want to block so std::promise is appropriate.
   auto done = std::make_shared<std::promise<void>>();
   strand.post([=]() {
      done->set_value();
      return nullptr;
   });
   done->get_future().wait();

   return 0;
}

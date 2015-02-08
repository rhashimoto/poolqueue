// Copyright 2014 Shoestring Research, LLC.  All rights reserved.
#include <boost/iostreams/device/back_inserter.hpp>
#ifdef HAVE_LIBZ
#include <boost/iostreams/filter/zlib.hpp>
#endif
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/lexical_cast.hpp>

#ifdef HAVE_BOOST_MPI_HPP
#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <boost/dynamic_bitset.hpp>
#include <boost/mpi.hpp>
#include <boost/serialization/split_member.hpp>
#endif

#include "MPI.hpp"

using namespace poolqueue;

#ifdef HAVE_BOOST_MPI_HPP
static const auto DefaultServiceInterval = std::chrono::milliseconds(20);

// Procedure (remote call) subclasses for internal use.
namespace poolqueue {
   namespace detail {

      // Synchronize ranks by resolving when all promises have been
      // collected.
      class SyncFunction : public Function {
         static std::mutex m;
         static std::vector<Promise> promises;
            
         friend class boost::serialization::access;
         template<typename Archive>
         void serialize(Archive& ar, const unsigned int) {
            ar & boost::serialization::base_object<Function>(*this);
         }

      public:
         SyncFunction() {}
         Promise operator()() const;
      };
         
      // Return a Function result.
      class ResultProcedure : public Procedure {
         uint32_t tag_;
         Promise::Value result_;
            
         friend class boost::serialization::access;

         // Generic serialization is instantiated but never called.
         template<class Archive>
         void save(Archive&, unsigned int) const {
            throw std::logic_error("invalid archive");
         }

         template<class Archive>
         void load(Archive&, unsigned int) {
            throw std::logic_error("invalid archive");
         }

         // Only OArchive and IArchive serialization is actually
         // invoked.
         void save(poolqueue::MPI::OArchive& ar, unsigned int) const {
            ar & boost::serialization::base_object<Procedure>(*this);
            ar & tag_;

            // Dispatch on type to save Promise::Value.
            if (!result_.empty()) {
               auto f = poolqueue::MPI::getSaveFunc(result_.type());
               f(ar, result_);
            }
            else {
               const int32_t type = -1;
               ar << type;
            }
         }

         void load(poolqueue::MPI::IArchive& ar, unsigned int) {
            ar & boost::serialization::base_object<Procedure>(*this);
            ar & tag_;
               
            // Load type and dispatch to load Promise::Value.
            int32_t type;
            ar >> type;

            if (type >= 0) {
               auto f = poolqueue::MPI::getLoadFunc(type);
               f(ar, result_);
            }
         }
         BOOST_SERIALIZATION_SPLIT_MEMBER()

         ResultProcedure() {}
      public:
         ResultProcedure(uint32_t tag, const Promise::Value& result)
            : tag_(tag)
            , result_(result) {
         }
            
         void operator()() const;
      };

   }

   // Serialization specializations for Promise::Value containing void.
   template<>
   inline void MPI::saveValue<void>(OArchive&, const Promise::Value&) {
   }
         
   template<>
   inline void MPI::loadValue<void>(IArchive&, Promise::Value&) {
   }
      
}
BOOST_CLASS_EXPORT(poolqueue::detail::SyncFunction)
BOOST_CLASS_EXPORT(poolqueue::detail::ResultProcedure)

struct poolqueue::MPI::Pimpl {
   std::atomic<size_t> running_;
   std::thread t_;
   std::unique_ptr<boost::mpi::environment> env_;
   std::unique_ptr<boost::mpi::communicator> world_;
      
   std::string processName_;
   int rank_;
   int size_;
   std::chrono::microseconds interval_;
      
   std::mutex taskMutex_;
   std::condition_variable taskReady_;
   std::queue<std::function<void()> > tasks_;

   // Augment boost::mpi::request with a buffer.
   struct Request : public boost::mpi::request {
      std::unique_ptr<std::vector<char> > buffer_;

      // Send constructor.
      Request(int dstRank, std::vector<char>&& buffer);

      // Recv constructor.
      Request(boost::mpi::communicator& communicator, int srcRank);
      
      Request(const Request&) = delete;
      Request(Request&& other) = default;

      Request& operator=(const Request&) = delete;
      Request& operator=(Request&& other) = default;
   };
   std::vector<Request> sendRequests_;
   std::vector<Request> recvRequests_;
      
   std::mutex tagMutex_;
   uint32_t tag_;
   std::unordered_map<uint32_t, Promise> tagPromises_;

   std::unordered_map<std::type_index, SaveFunc> saveFuncs_;
   std::unordered_map<int32_t, LoadFunc> loadFuncs_;

   Promise syncPromise_;
   
   Pimpl()
      : interval_(DefaultServiceInterval)
      , tag_(0) {
      // Register primitive types for function return values.
#define REGISTER(TYPE)                                  \
      registerType(                                     \
         typeid(TYPE),                                  \
         &poolqueue::MPI::saveValue<TYPE>,      \
         &poolqueue::MPI::loadValue<TYPE>)

      REGISTER(bool);
      REGISTER(int8_t);
      REGISTER(int16_t);
      REGISTER(int32_t);
      REGISTER(int64_t);
      REGISTER(uint8_t);
      REGISTER(uint16_t);
      REGISTER(uint32_t);
      REGISTER(uint64_t);
#undef REGISTER

      // MPI::synchronize() blocks on its previous Promise has
      // resolved, so ensure the first invocation does not block.
      syncPromise_.settle();
      
      // Start the MPI thread and wait for initialization.
      running_ = 1;
      std::promise<void> ready;
      std::thread([this, &ready]() { run(ready); }).swap(t_);
      ready.get_future().wait();
   }

   ~Pimpl() {
      // Block until all ranks are in the destructor.
      std::promise<void> done;
      synchronize().then([&]() {
            done.set_value();
         });
      done.get_future().wait();
      pool().synchronize().wait();

      // Signal the thread and wait for it to exit.
      --running_;
      t_.join();
   }

   // Add a task to run in the MPI thread.
   void enqueue(const std::function<void()>& f) {
      std::lock_guard<std::mutex> lock(taskMutex_);
      ++running_;
      tasks_.push(f);
      taskReady_.notify_one();
   }

   // Serialize and send a functor.
   template<typename F>
   Promise call(int rank, const F& f) {
      constexpr bool isFunction = std::is_convertible<F *, Function *>::value;
      constexpr bool isProcedure = std::is_convertible<F *, Procedure *>::value;
      static_assert(isFunction || isProcedure, "functor must be Function or Procedure");

      // Determine the proper base class.
      typedef typename std::conditional<isFunction, Function, Procedure>::type Base;

      // Procedure calls get tag 0, Function calls get a unique non-zero tag.
      Promise p;
      uint32_t tag = 0;
      if (isFunction) {
         // Select an unused non-zero tag.
         std::lock_guard<std::mutex> lock(tagMutex_);
         tag = tag_;
         do {
            if (++tag_ == tag)
               throw std::runtime_error("too many outstanding Function calls");
         } while (tag_ == 0 || tagPromises_.count(tag_));

         // Store the Promise for the function return.
         tagPromises_[tag = tag_] = p;
         ++running_;
      }

      // Serialize functor.
      auto buffer = std::make_shared<std::vector<char> >();
      {
         boost::iostreams::filtering_ostream os;
#ifdef HAVE_LIBZ
         os.push(boost::iostreams::zlib_compressor());
#endif
         os.push(boost::iostreams::back_inserter(*buffer));
         OArchive oa(os);

         const Base *fp = &f;
         oa << tag;
         oa << fp;
      }

      // Queue message.
      enqueue([this, rank, buffer]() {
            this->sendRequests_.emplace_back(rank, std::move(*buffer));
         });
      return p;
   }

   // Handle the result from a remote Function call.
   void resolve(uint32_t tag, const Promise::Value& result) {
      Promise p;
      {
         std::lock_guard<std::mutex> lock(tagMutex_);
         p = std::move(tagPromises_[tag]);
         tagPromises_.erase(tag);
         --running_;
      }
      p.settle(result);
   }
      
   // MPI thread function.
   void run(std::promise<void>& ready) {
      env_.reset(new boost::mpi::environment);

      // Use a non-default communicator for a separate tag namespace.
      boost::mpi::communicator world;
      world_.reset(new boost::mpi::communicator(world, world.group()));

      char name[MPI_MAX_PROCESSOR_NAME];
      int length;
      MPI_Get_processor_name(name, &length);
      processName_ = std::string(name, length);
            
      rank_ = world_->rank();
      size_ = world_->size();
            
      // Listen for messages from all ranks.
      for (int i = 0; i < size_; ++i)
         recvRequests_.emplace_back(*world_, i);

      // Unblock the constructor.
      ready.set_value();

      // The event loop termination condition checks an atomic
      // counter, running_, for destructor entry, tasks, and tags
      // (remote Function calls that have not returned). sendRequests_
      // does not use the counter because it is only modified in this
      // thread.
      while (!sendRequests_.empty() || running_) {
         // Wait for a task notification or service interval expiration.
         {
            std::queue<std::function<void()> > tasks;
            {
               std::unique_lock<std::mutex> lock(taskMutex_);
               if (tasks_.empty())
                  taskReady_.wait_for(lock, interval_);

               using std::swap;
               swap(tasks, tasks_);
            }
            
            // Process task queue.
            running_ -= tasks.size();
            while (!tasks.empty()) {
               tasks.front()();
               tasks.pop();
            }
         }
            
         // Test recv requests for completion.
         for (auto& request : recvRequests_) {
            if (auto status = request.test()) {
               // Pass to the thread pool.
               auto buffer = std::make_shared<std::vector<char> >(std::move(*request.buffer_));
               pool().post([=]() {
                     // Build deserialization stream.
                     boost::iostreams::filtering_istream is;
#ifdef HAVE_LIBZ
                     is.push(boost::iostreams::zlib_decompressor());
#endif
                     is.push(boost::make_iterator_range(buffer->begin(), buffer->end()));
                     IArchive ia(is);

                     // Read the tag.
                     uint32_t tag;
                     ia >> tag;
                     if (tag) {
                        // Non-zero tag indicates function requiring a reply.
                        Function *f;
                        ia >> f;

                        (*f)()
                           .then([this, status, tag](const Promise::Value& value) {
                                 detail::ResultProcedure rp(tag, value);
                                 call(status->source(), rp);
                              });
                     }
                     else {
                        // Zero tag indicates procedure without a reply.
                        Procedure *f;
                        ia >> f;

                        (*f)();
                     }
                  });
                  
               // Listen again.
               request = Request(*world_, status->source());
            }
         }
            
         // Test send requests for completion.
         auto completed = boost::mpi::test_some(sendRequests_.begin(), sendRequests_.end());
         sendRequests_.erase(completed, sendRequests_.end());
      }
            
      for (auto& request : recvRequests_)
         request.cancel();
         
      world_.reset();
      env_.reset();
   }

   void setInterval(const std::chrono::microseconds& interval) {
      enqueue([this, interval]() {
            this->interval_ = interval;
         });
   }
      
   void registerType(
      const std::type_info& type,
      const SaveFunc& saveFunc,
      const LoadFunc& loadFunc) {
      if (saveFuncs_.count(type))
         return;

      const int32_t index = static_cast<int32_t>(saveFuncs_.size());
      saveFuncs_[type] = [=](OArchive& ar, const Promise::Value& value) {
         ar << index;
         saveFunc(ar, value);
      };
      loadFuncs_[index] = loadFunc;
   }
         
   static Pimpl& singleton() {
      // Ensure that the ThreadPool is not destroyed until after MPI.
      pool().getThreadCount();
         
      static Pimpl instance;
      return instance;
   }
};

poolqueue::MPI::Pimpl::Request::Request(int dstRank, std::vector<char>&& buffer)
   : buffer_(new std::vector<char>(std::move(buffer))) {
   auto& pimpl = Pimpl::singleton();
   assert(std::this_thread::get_id() == pimpl.t_.get_id());
   *static_cast<boost::mpi::request *>(this) = pimpl.world_->isend(dstRank, pimpl.rank_, *buffer_);
}

poolqueue::MPI::Pimpl::Request::Request(boost::mpi::communicator& communicator, int srcRank)
   : buffer_(new std::vector<char>) {
   *static_cast<boost::mpi::request *>(this) = communicator.irecv(srcRank, srcRank, *buffer_);
}

std::mutex poolqueue::detail::SyncFunction::m;
std::vector<Promise> poolqueue::detail::SyncFunction::promises;
Promise poolqueue::detail::SyncFunction::operator()() const {
   Promise promise;
   
   // Save promises until we have one from each rank.
   std::lock_guard<std::mutex> lock(m);
   promises.push_back(promise);
   if (promises.size() == static_cast<size_t>(poolqueue::MPI::size())) {
      // Then resolve them all at once.
      for (auto& p : promises)
         p.settle();
      promises.clear();
   }

   return promise;
}

void poolqueue::detail::ResultProcedure::operator()() const {
   poolqueue::MPI::Pimpl::singleton().resolve(tag_, result_);
}
#endif

int
poolqueue::MPI::rank() {
#ifdef HAVE_BOOST_MPI_HPP
   return Pimpl::singleton().rank_;
#else
   return 0;
#endif
}

int
poolqueue::MPI::size() {
#ifdef HAVE_BOOST_MPI_HPP
   return Pimpl::singleton().size_;
#else
   return 1;
#endif
}

std::string
poolqueue::MPI::processName() {
#ifdef HAVE_BOOST_MPI_HPP
   return Pimpl::singleton().processName_;
#else
   return "localhost";
#endif
}

void
poolqueue::MPI::call(int rank, const Procedure& f) {
#ifdef HAVE_BOOST_MPI_HPP
   auto& pimpl = Pimpl::singleton();
   pimpl.call(rank, f);
#else
   boost::ignore_unused_variable_warning(rank);
   
   // Clone f.
   std::vector<char> buffer;
   {
      boost::iostreams::filtering_ostream os(boost::iostreams::back_inserter(buffer));
      OArchive oa(os);
      const Procedure *pf = &f;
      oa << pf;
   }

   boost::iostreams::filtering_istream is(boost::make_iterator_range(buffer));
   IArchive ia(is);
   Procedure *pf = nullptr;
   ia >> pf;

   // Run the clone on the thread pool.
   std::shared_ptr<Procedure> clone(pf);
   pool().post([=]() {
         (*clone)();
      });
#endif
}

Promise
poolqueue::MPI::call(int rank, const Function& f) {
#ifdef HAVE_BOOST_MPI_HPP
   auto& pimpl = Pimpl::singleton();
   return pimpl.call(rank, f);
#else
   boost::ignore_unused_variable_warning(rank);
   
   // Clone f.
   std::vector<char> buffer;
   {
      boost::iostreams::filtering_ostream os(boost::iostreams::back_inserter(buffer));
      OArchive oa(os);
      const Function *pf = &f;
      oa << pf;
   }

   boost::iostreams::filtering_istream is(boost::make_iterator_range(buffer));
   IArchive ia(is);
   Function *pf = nullptr;
   ia >> pf;

   // Run the clone on the thread pool.
   Promise p;
   std::shared_ptr<Function> clone(pf);
   pool().post([=]() {
         (*clone)().then([=](const Promise::Value& value) {
               p.settle(value);
            });
      });

   return p;
#endif
}

ThreadPool<>&
poolqueue::MPI::pool() {
   static ThreadPool<> tp;
   return tp;
}

void
poolqueue::MPI::post(const std::function<void()>& f) {
#ifdef HAVE_BOOST_MPI_HPP
   auto& pimpl = Pimpl::singleton();
   pimpl.enqueue(f);
#else
   pool().post(f);
#endif
}

Promise
poolqueue::MPI::synchronize() {
#ifdef HAVE_BOOST_MPI_HPP
   auto& pimpl = Pimpl::singleton();

   // Block until any previous synchronize() is complete.
   std::promise<void> done;
   pimpl.syncPromise_.then([&]() {
         done.set_value();
      });
   done.get_future().wait();
   
   // Send sync to every other rank. Each rank will reply when it has
   // received syncs from all ranks.
   std::vector<Promise> promises;
   for (int i = 0; i < size(); ++i)
      promises.push_back(call(i, detail::SyncFunction()));

   pimpl.syncPromise_ = Promise::all(promises.begin(), promises.end());
   return pimpl.syncPromise_;
#else
   return Promise().settle();
#endif   
}

void
poolqueue::MPI::setPollInterval(const std::chrono::microseconds& interval) {
#ifdef HAVE_BOOST_MPI_HPP
   Pimpl::singleton().setInterval(interval);
#else
   boost::ignore_unused_variable_warning(interval);
#endif
}

void
poolqueue::MPI::registerType(
   const std::type_info& type,
   const SaveFunc& saveFunc,
   const LoadFunc& loadFunc) {
#ifdef HAVE_BOOST_MPI_HPP
   auto& pimpl = Pimpl::singleton();
   pimpl.registerType(type, saveFunc, loadFunc);
#else
   boost::ignore_unused_variable_warning(type);
   boost::ignore_unused_variable_warning(saveFunc);
   boost::ignore_unused_variable_warning(loadFunc);
#endif
}

const poolqueue::MPI::SaveFunc&
poolqueue::MPI::getSaveFunc(const std::type_info& type) {
#ifdef HAVE_BOOST_MPI_HPP
   auto& pimpl = Pimpl::singleton();
   auto i = pimpl.saveFuncs_.find(type);
   if (i != pimpl.saveFuncs_.end())
      return i->second;
#endif
   throw Function::UnregisteredReturnType(type.name());
}

const poolqueue::MPI::LoadFunc&
poolqueue::MPI::getLoadFunc(int32_t index) {
#ifdef HAVE_BOOST_MPI_HPP
   auto& pimpl = Pimpl::singleton();
   auto i = pimpl.loadFuncs_.find(index);
   if (i != pimpl.loadFuncs_.end())
      return i->second;
#endif
   throw Function::UnregisteredReturnType(boost::lexical_cast<std::string>(index));
}

// Copyright 2014 Shoestring Research, LLC.  All rights reserved.
#ifndef poolqueue_MPI_hpp
#define poolqueue_MPI_hpp

#include <chrono>
#include <functional>
#include <memory>
#include <typeinfo>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/access.hpp>
#include <boost/serialization/export.hpp>

#include "Promise.hpp"
#include "ThreadPool.hpp"

namespace poolqueue {
   namespace detail {
      class ExitProcedure;
      class ResultProcedure;
   }
      
   // Abstract base class for a serializable functor that does not
   // fulfil a completion Promise. A Procedure instance passed to
   // MPI::call() is fire-and-forget; i.e. it does not return a
   // value or completion notification.
   class Procedure {
      friend class boost::serialization::access;
      template<class Archive>
      void serialize(Archive&, const unsigned int) {
      }
   public:
      virtual ~Procedure() {}
      virtual void operator()() const = 0;
   };

   // Abstract base class for a serializable functor that *does*
   // fulfil a completion Promise. The type passed via the returned
   // Promise must be serializable and registered with
   // registerType<T>(). Exceptions cannot be returned.
   class Function {
      friend class boost::serialization::access;
      template<class Archive>
      void serialize(Archive&, const unsigned int) {
      }
   public:
      class UnregisteredReturnType : public std::bad_cast {
         const std::string msg_;
      public:
         UnregisteredReturnType(const std::string& msg)
            : msg_("Unregistered type returned from Function: " + msg) {
         }

         virtual const char *what() const noexcept {
            return msg_.c_str();
         }
      };
         
      virtual ~Function() {}
      virtual Promise operator()() const = 0;
   };

   class MPI {
   public:
      static int rank();
      static int size();
      static std::string processName();

      // To make a remote call, define a subclass of Procedure or
      // Function that can be serialized using boost::serialization
      // via a pointer to the base class. Details are in the Boost
      // docs, see especially:
      //
      // http://www.boost.org/doc/libs/1_57_0/libs/serialization/doc/serialization.html#derivedpointers
      // http://www.boost.org/doc/libs/1_57_0/libs/serialization/doc/special.html#export
      //
      // At runtime, use call() to invoke a functor instance on an
      // MPI rank. The original functor instance is not referenced
      // after call() returns (i.e. there is no dangling reference
      // if it is immediately destroyed). In the case of a
      // Function, the returned Promise can be used for
      // synchronization and access to the returned value.
      static void call(int rank, const Procedure& f);
      static Promise call(int rank, const Function& f);

      // Procedure and Function subclass instances execute on a
      // ThreadPool for each MPI rank. This function provides
      // access to the local pool.
      static ThreadPool& pool();

      // This invokes a function on the MPI thread for low-level
      // MPI (or boost::mpi) usage, as MPI implementations are
      // not necessarily thread-safe.
      static void post(const std::function<void()>& f);

      // This establishes a synchronization point for all MPI
      // ranks. The returned Promise is resolved when all ranks
      // have executed the call (note that the call itself is
      // non-blocking).
      static Promise synchronize();
         
      // Because the underlying MPI library may not be thread-safe,
      // polling is used to check for new messages (a check is also
      // made each time a message is sent). setPollInterval() allows
      // the default polling interval (20 ms) to be adjusted.
      template<typename Duration>
      static void setPollInterval(const Duration& interval) {
         const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(interval);
         setPollInterval(micros);
      }
      static void setPollInterval(const std::chrono::microseconds& interval);

      // This registers a serializable type so it can be returned
      // from a Function subclass via a Promise. Types must be
      // registered in the same order on all ranks.
      //
      // Care must be taken to ensure that a type is registered on
      // a rank before remote calls that use that type are received
      // from other ranks. The recommended pattern is to register
      // all types before any Functions are called, followed by a
      // synchronize().
      template<typename T>
      static void registerType() {
         registerType(
            typeid(T),
            &MPI::saveValue<T>,
            &MPI::loadValue<T>);
      }

   private:
      struct Pimpl;
      friend class detail::ExitProcedure;
      friend class detail::ResultProcedure;

      typedef boost::archive::binary_iarchive IArchive;
      typedef boost::archive::binary_oarchive OArchive;

      template<typename T>
      static void saveValue(OArchive& ar, const Promise::Value& value) {
         ar << value.cast<const T&>();;
      }

      template<typename T>
      static void loadValue(IArchive& ar, Promise::Value& value) {
         value = T();
         ar >> value.cast<T&>();
      }

      typedef std::function<void(OArchive&, const Promise::Value&)> SaveFunc;
      typedef std::function<void(IArchive&, Promise::Value&)> LoadFunc;
      static void registerType(
         const std::type_info& type,
         const SaveFunc& saveFunc,
         const LoadFunc& loadFunc);

      static const SaveFunc& getSaveFunc(const std::type_info& type);
      static const LoadFunc& getLoadFunc(const int32_t index);
   };

} // namespace poolqueue

#endif // poolqueue_MPI_hpp

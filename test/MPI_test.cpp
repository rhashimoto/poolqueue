#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE MPI

#include <mutex>
#include <boost/test/unit_test.hpp>

#include "MPI.hpp"

using namespace poolqueue;

class MyProcedure : public Procedure {
   int i_;

   friend class boost::serialization::access;
   template<typename Archive>
   void serialize(Archive& ar, const unsigned int version) {
      ar & boost::serialization::base_object<Procedure>(*this);
      ar & i_;
   }

   MyProcedure() {}
public:
   MyProcedure(int i) : i_(i) {}
   void operator()() const {
      std::mutex m;
      std::lock_guard<std::mutex> lock(m);
      BOOST_CHECK_GE(MPI::pool().index(), 0);
      BOOST_CHECK_EQUAL(i_, MPI::rank());
   }
};
BOOST_CLASS_EXPORT(MyProcedure)

BOOST_AUTO_TEST_CASE(procedure) {
   for (int i = 0; i < MPI::size(); ++i)
      MPI::call(i, MyProcedure(i));

   std::promise<void> done;
   MPI::synchronize()
      .then([&]() {
         done.set_value();
         return nullptr;
      });
   done.get_future().wait();
}

class MyFunction : public Function {
   int srcRank_;
   
   friend class boost::serialization::access;
   template<typename Archive>
   void serialize(Archive& ar, const unsigned int version) {
      ar & boost::serialization::base_object<Function>(*this);

      ar & srcRank_;
   }

   MyFunction() {}
public:
   MyFunction(int srcRank)
      : srcRank_(srcRank) {
   }

   Promise operator()() const {
      return Promise().settle(MPI::rank());
   }
};
BOOST_CLASS_EXPORT(MyFunction);

BOOST_AUTO_TEST_CASE(function) {
   std::vector<Promise> promises;
   
   const int srcRank = MPI::rank();
   for (int dstRank = 0; dstRank < MPI::size(); ++dstRank) {
      Promise p = MPI::call(dstRank, MyFunction(srcRank));
      promises.push_back(std::move(p));
   }

   std::promise<void> done;
   Promise::all(promises.begin(), promises.end())
      .then([&]() {
         for (size_t i = 0; i < promises.size(); ++i) {
            promises[i].then([=](int value) {
               BOOST_CHECK_EQUAL(value, i);
               return nullptr;
            });
         }
         done.set_value();
         return nullptr;
      });

   done.get_future().wait();
}

class ReturnClass : public Function {
   friend class boost::serialization::access;
   template<typename Archive>
   void serialize(Archive& ar, const unsigned int version) {
      ar & boost::serialization::base_object<Function>(*this);
   }

public:
   ReturnClass() {}

   Promise operator()() const {
      return Promise().settle(std::string("how now brown cow"));
   }
};
BOOST_CLASS_EXPORT(ReturnClass);

#include <boost/serialization/string.hpp>

BOOST_AUTO_TEST_CASE(return_class) {
   MPI::registerType<std::string>();
   MPI::synchronize();
   
   std::promise<void> done;
   MPI::call(0, ReturnClass())
      .then([&](const std::string& s) {
         BOOST_CHECK_EQUAL(s, "how now brown cow");
         done.set_value();
         return nullptr;
      });

   done.get_future().wait();
}

BOOST_AUTO_TEST_CASE(synchronize) {
   MPI::synchronize();
   MPI::synchronize();
   MPI::synchronize();
}

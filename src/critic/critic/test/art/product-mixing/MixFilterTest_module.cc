#include "cetlib/quiet_unit_test.hpp"

#include "art/Framework/Core/FileBlock.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Core/PtrRemapper.h"
#include "art/Framework/IO/ProductMix/MixHelper.h"
#include "art/Framework/Modules/MixFilter.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Services/Optional/RandomNumberGenerator.h"
#include "art/Persistency/Common/CollectionUtilities.h"
#include "art/test/TestObjects/ProductWithPtrs.h"
#include "art/test/TestObjects/ToyProducts.h"
#include "art_root_io/RootIOPolicy.h"
#include "canvas/Persistency/Common/PtrVector.h"
#include "canvas/Utilities/InputTag.h"
#include "cetlib/container_algorithms.h"
#include "cetlib/map_vector.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/ConfigurationTable.h"
#include "fhiclcpp/types/OptionalSequence.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <memory>
#include <unordered_set>

namespace arttest {
  class MixFilterTestDetail;
#if ART_TEST_EVENTS_TO_SKIP_CONST
#define ART_MFT MixFilterTestETSc
#define ART_TEST_EVENTS_TO_SKIP_CONST_TXT const
#elif defined ART_TEST_EVENTS_TO_SKIP_CONST
#define ART_MFT MixFilterTestETS
#define ART_TEST_EVENTS_TO_SKIP_CONST_TXT
#elif defined ART_TEST_OLD_STARTEVENT
#define ART_MT MixFilterTestOldStartEvent
#elif defined ART_TEST_NO_STARTEVENT
#define ART_MT MixFilterTestNoStartEvent
#else
// Normal case
#define ART_MFT MixFilterTest
#endif
  using ART_MFT = art::MixFilter<MixFilterTestDetail, art::RootIOPolicy>;
}

namespace {
  class SecondaryFileNameProvider {
  public:
    SecondaryFileNameProvider(std::vector<std::string>&& fileNames)
      : fileNames_{move(fileNames)}, fileNameIter_{fileNames_.cbegin()}
    {}

    SecondaryFileNameProvider(SecondaryFileNameProvider const& other)
      : fileNames_{other.fileNames_}
      , fileNameIter_{fileNames_.cbegin() +
                      (other.fileNameIter_ - other.fileNames_.cbegin())}
    {}

    SecondaryFileNameProvider&
    operator=(SecondaryFileNameProvider const& other)
    {
      SecondaryFileNameProvider tmp{other};
      std::swap(tmp, *this);
      return *this;
    }

    ~SecondaryFileNameProvider() noexcept = default;

    std::string
    operator()()
    {
      if (fileNameIter_ == fileNames_.end()) {
        return {};
      } else {
        return *(fileNameIter_++);
      }
    }

  private:
    std::vector<std::string> fileNames_;
    decltype(fileNames_.cbegin()) fileNameIter_;
  };
} // namespace

using namespace fhicl;

class arttest::MixFilterTestDetail {
public:
  using mv_t = cet::map_vector<unsigned int>;
  using mvv_t = mv_t::value_type;
  using mvm_t = mv_t::mapped_type;

  struct Config {
    Atom<size_t> numSecondaries{Name{"numSecondaries"}, 1};
    Atom<bool> testRemapper{Name{"testRemapper"}, true};
    Atom<bool> testZeroSecondaries{Name{"testZeroSecondaries"}, false};
    Atom<bool> testPtrFailure{Name{"testPtrFailure"}, false};
    Atom<bool> testEventOrdering{Name{"testEventOrdering"}, false};
    Atom<bool> testNoLimEventDupes{Name{"testNoLimEventDupes"}, false};
    Atom<bool> compactMissingProducts{Name{"compactMissingProducts"}, false};
    Atom<size_t> expectedRespondFunctionCalls{
      Name{"expectedRespondFunctionCalls"},
      4ul};
    OptionalSequence<std::string> fileNamesToProvide{
      Name{"fileNamesToProvide"}};
    Atom<std::string> mixProducerLabel{Name{"mixProducerLabel"}, "mixProducer"};
  };

  using Parameters = art::MixFilterTable<Config>;

  // Constructor is responsible for registering mix operations with
  // MixHelper::declareMixOp() and bookkeeping products with
  // MixHelper::produces().
  MixFilterTestDetail(Parameters const& p, art::MixHelper& helper);

  MixFilterTestDetail(MixFilterTestDetail const&) = delete;
  MixFilterTestDetail& operator=(MixFilterTestDetail const&) = delete;

  ~MixFilterTestDetail();

#ifdef ART_TEST_OLD_STARTEVENT
  // Old startEvent signature -- check it still works
  void startEvent();
#elif !defined ART_TEST_NO_STARTEVENT
  // Optional startEvent(Event const&): initialize state for each event,
  void startEvent(art::Event const&);
#endif

  // Return the number of secondaries to read this time. Declare const
  // if you don't plan to change your class' state.
  size_t nSecondaries() const;

  // Optional eventsToSkip(): number of events to skip at the start of
  // the file.
#ifdef ART_TEST_EVENTS_TO_SKIP_CONST
  size_t
  eventsToSkip() ART_TEST_EVENTS_TO_SKIP_CONST_TXT
  {
    return 7;
  }
#endif

  // Optional processEventIDs(): after the generation of the event
  // sequence, this function will be called if it exists to provide
  // the sequence of EventIDs.
  void processEventIDs(art::EventIDSequence const& seq);

  // Optional processEventAuxiliaries(): after the generation of the
  // event sequence, this function will be called if it exists to
  // provide the sequence of EventAuxiliaries.
  void processEventAuxiliaries(art::EventAuxiliarySequence const&);

  // Optional.finalizeEvent(): (eg) put bookkeeping products in
  // event. Do *not* place mix products into the event: this will
  // already have been done for you.
  void finalizeEvent(art::Event& t);

  // Optional respondToXXXfunctions, called at the right time if they
  // exist.
  void respondToOpenInputFile(art::FileBlock const& fb);
  void respondToCloseInputFile(art::FileBlock const& fb);
  void respondToOpenOutputFiles(art::FileBlock const& fb);
  void respondToCloseOutputFiles(art::FileBlock const& fb);

  // Optional {begin,end}{Sub,}Run functions, called at the right time
  // if they exist.
  void beginSubRun(art::SubRun const& sr);
  void endSubRun(art::SubRun& sr);
  void beginRun(art::Run const& r);
  void endRun(art::Run& r);

  // Mixing functions. Note that they do not *have* to be member
  // functions of this detail class: they may be member functions of a
  // completely unrelated class; free functions or function objects
  // provided they (or the function object's operator()) have the
  // expected signature.
  template <typename PROD, typename OPROD = PROD>
  bool mixByAddition(std::vector<PROD const*> const&,
                     OPROD&,
                     art::PtrRemapper const&);

  bool aggregateDoubleCollection(
    std::vector<std::vector<double> const*> const& in,
    std::vector<double>& out,
    art::PtrRemapper const&);

  bool aggregate_map_vector(std::vector<mv_t const*> const& in,
                            mv_t& out,
                            art::PtrRemapper const&);

  bool mixPtrs(std::vector<std::vector<art::Ptr<double>> const*> const& in,
               std::vector<art::Ptr<double>>& out,
               art::PtrRemapper const& remap);

#ifndef ART_NO_MIX_PTRVECTOR
  bool mixPtrVectors(std::vector<art::PtrVector<double> const*> const& in,
                     art::PtrVector<double>& out,
                     art::PtrRemapper const& remap);
#endif

  bool mixProductWithPtrs(
    std::vector<arttest::ProductWithPtrs const*> const& in,
    arttest::ProductWithPtrs& out,
    art::PtrRemapper const& remap);

  bool mixmap_vectorPtrs(
    std::vector<std::vector<
      art::Ptr<cet::map_vector<unsigned int>::value_type>> const*> const& in,
    std::vector<art::Ptr<cet::map_vector<unsigned int>::value_type>>& out,
    art::PtrRemapper const& remap);

  bool mixSRProduct(std::vector<double const*> const& in,
                    DoubleProduct& out,
                    art::PtrRemapper const&);

  bool mixRProduct(std::vector<double const*> const& in,
                   DoubleProduct& out,
                   art::PtrRemapper const&);

  template <typename COLL>
  void verifyInSize(COLL const& in) const;

private:
  size_t const nSecondaries_;
  bool const testRemapper_;
  std::vector<size_t> doubleVectorOffsets_{};
  std::vector<size_t> map_vectorOffsets_{};
  std::unique_ptr<art::EventIDSequence> eIDs_{};
  bool startEvent_called_{false};
  bool processEventIDs_called_{false};
  bool processEventAuxiliaries_called_{false};
  size_t beginSubRunCounter_{};
  size_t endSubRunCounter_{};
  size_t beginRunCounter_{};
  size_t endRunCounter_{};
  int currentEvent_{-1};
  bool const testZeroSecondaries_;
  bool const testPtrFailure_;
  bool const testEventOrdering_;
  bool const testNoLimEventDupes_;
  bool const compactMissingProducts_;
  size_t const expectedRespondFunctionCalls_;
  art::MixHelper::Mode const readMode_;

  size_t respondFunctionsSeen_{};

  // For testing no_replace mode only:
  std::vector<int> allEvents_{};
  std::unordered_set<int> uniqueEvents_{};

  // For testing run and subrun mixing.
  double subRunInfo_{};
  double runInfo_{};
};

template <typename COLL>
inline void
arttest::MixFilterTestDetail::verifyInSize(COLL const& in) const
{
  BOOST_TEST_REQUIRE((
    in.size() == (currentEvent_ == 2 && testZeroSecondaries_) ? 0 :
                                                                nSecondaries_));
}

arttest::MixFilterTestDetail::MixFilterTestDetail(Parameters const& p,
                                                  art::MixHelper& helper)
  : nSecondaries_{p().numSecondaries()}
  , testRemapper_{p().testRemapper()}
  , testZeroSecondaries_{p().testZeroSecondaries()}
  , testPtrFailure_{p().testPtrFailure()}
  , testEventOrdering_{p().testEventOrdering()}
  , testNoLimEventDupes_{p().testNoLimEventDupes()}
  , compactMissingProducts_{p().compactMissingProducts()}
  , expectedRespondFunctionCalls_{p().expectedRespondFunctionCalls()}
  , readMode_{helper.readMode()}
{
  std::vector<std::string> fnToProvide;
  if (p().fileNamesToProvide(fnToProvide)) {
    std::cerr << "Calling registerSecondaryFileNameProvider.\n";
    std::copy(fnToProvide.cbegin(),
              fnToProvide.cend(),
              std::ostream_iterator<std::string>(std::cerr, ", "));
    std::cerr << "\n";
    helper.registerSecondaryFileNameProvider(
      SecondaryFileNameProvider{move(fnToProvide)});
  }

  // Test createEngine when it should have already been created
  if (readMode_ > art::MixHelper::Mode::SEQUENTIAL) {
    auto const& default_engine_kind =
      art::ServiceHandle<art::RandomNumberGenerator const>()
        ->defaultEngineKind();
    helper.createEngine(123456);
    helper.createEngine(123456, default_engine_kind);
    helper.createEngine(123456, default_engine_kind, "");
    // Cannot assign empty label to different type
    BOOST_REQUIRE_THROW(helper.createEngine(123456, "MTwistEngine"),
                        art::Exception);
    // Cannot specify type that is not supported.
    BOOST_REQUIRE_THROW(helper.createEngine(123456, "HEPJamesRandom", "Edna"),
                        cet::exception);
  }

  auto const mixProducerLabel = p().mixProducerLabel();
  helper.produces<std::string>();           // "Bookkeeping"
  helper.produces<art::EventIDSequence>();  // "Bookkeeping"
  helper.produces<double, art::InSubRun>(); // SubRun product test.
  helper.produces<double, art::InRun>();    // Run product test.
  helper.declareMixOp(art::InputTag{mixProducerLabel, "doubleLabel"},
                      &MixFilterTestDetail::mixByAddition<double>,
                      *this);
  helper.declareMixOp(art::InputTag{mixProducerLabel, "IntProductLabel"},
                      &MixFilterTestDetail::mixByAddition<arttest::IntProduct>,
                      *this);
  helper.declareMixOp(art::InputTag{mixProducerLabel, "stringLabel", "SWRITE"},
                      &MixFilterTestDetail::mixByAddition<std::string>,
                      *this);
  helper.declareMixOp(art::InputTag{mixProducerLabel, "doubleCollectionLabel"},
                      &MixFilterTestDetail::aggregateDoubleCollection,
                      *this);
  helper.declareMixOp(art::InputTag{mixProducerLabel, "doubleVectorPtrLabel"},
                      &MixFilterTestDetail::mixPtrs,
                      *this);
#ifndef ART_NO_MIX_PTRVECTOR
  helper.declareMixOp(art::InputTag{mixProducerLabel, "doublePtrVectorLabel"},
                      &MixFilterTestDetail::mixPtrVectors,
                      *this);
#endif
  helper.declareMixOp(art::InputTag{mixProducerLabel, "ProductWithPtrsLabel"},
                      &MixFilterTestDetail::mixProductWithPtrs,
                      *this);
  helper.declareMixOp(art::InputTag{mixProducerLabel, "mapVectorLabel"},
                      &MixFilterTestDetail::aggregate_map_vector,
                      *this);
  helper.declareMixOp(art::InputTag{mixProducerLabel, "intVectorPtrLabel"},
                      &MixFilterTestDetail::mixmap_vectorPtrs,
                      *this);
  art::MixFunc<IntProduct> mixfunc(
    [this](std::vector<IntProduct const*> const& in,
           IntProduct,
           art::PtrRemapper const&) -> bool {
      auto const sz = in.size();
      auto expected = nSecondaries_;
      if (compactMissingProducts_) {
        expected -= std::count_if(
          eIDs_->begin(), eIDs_->end(), [](art::EventID const& eID) {
            return (eID.event() % 100) == 0;
          });
      }
      for (auto i = 0ul; i < sz; ++i) {
        if (!compactMissingProducts_ && ((*eIDs_)[i].event() % 100) == 0) {
          // Product missing
          BOOST_TEST_REQUIRE(in[i] == nullptr);
        } else {
          BOOST_TEST_REQUIRE(in[i] != nullptr);
        }
      }
      BOOST_TEST_REQUIRE(
        (sz == (currentEvent_ == 2 && testZeroSecondaries_) ? 0ul : expected));
      return false;
    });
  helper.declareMixOp(
    art::InputTag{mixProducerLabel, "SpottyProductLabel"}, mixfunc, false);
  // SubRun mixing.
  helper.declareMixOp<art::InSubRun>(
    art::InputTag{mixProducerLabel, "DoubleSRLabel"},
    "SRInfo",
    &MixFilterTestDetail::mixSRProduct,
    *this);
  // Run mixing.
  helper.declareMixOp<art::InRun>(
    art::InputTag{mixProducerLabel, "DoubleRLabel"},
    "RInfo",
    &MixFilterTestDetail::mixRProduct,
    *this);
}

arttest::MixFilterTestDetail::~MixFilterTestDetail()
{
  if (readMode_ == art::MixHelper::Mode::RANDOM_LIM_REPLACE &&
      testNoLimEventDupes_ == false) {
    // Require dupes across the job.
    BOOST_TEST(allEvents_.size() > uniqueEvents_.size());
  } else if (readMode_ == art::MixHelper::Mode::RANDOM_NO_REPLACE) {
    // Require no dupes across the job.
    BOOST_TEST(allEvents_.size() == uniqueEvents_.size());
  }
  BOOST_TEST(respondFunctionsSeen_ == expectedRespondFunctionCalls_);
  BOOST_TEST(beginSubRunCounter_ == 1ull);
  BOOST_TEST(endSubRunCounter_ == 1ull);
  BOOST_TEST(beginRunCounter_ == 1ull);
  BOOST_TEST(endRunCounter_ == 1ull);
}

#ifndef ART_TEST_NO_STARTEVENT
void
arttest::MixFilterTestDetail::
#ifdef ART_TEST_OLD_STARTEVENT
  startEvent()
#else
    // Normal case
  startEvent(art::Event const&)
#endif
{
  startEvent_called_ = true;
  eIDs_.reset();
  ++currentEvent_;
}
#endif

size_t
arttest::MixFilterTestDetail::nSecondaries() const
{
  return (currentEvent_ == 2 && testZeroSecondaries_) ? 0 : nSecondaries_;
}

void
arttest::MixFilterTestDetail::processEventIDs(art::EventIDSequence const& seq)
{
#ifdef ART_TEST_NO_STARTEVENT
  // Need to deal with this here
  ++currentEvent_;
#endif
  processEventIDs_called_ = true;
  eIDs_ = std::make_unique<art::EventIDSequence>(seq);
  if (!testEventOrdering_) {
    return;
  }
  switch (readMode_) {
    case art::MixHelper::Mode::SEQUENTIAL: {
      auto count(1);
      for (auto const& eid : seq) {
        BOOST_TEST_REQUIRE(eid.event() ==
                           currentEvent_ * nSecondaries() + count++);
      }
    } break;
    case art::MixHelper::Mode::RANDOM_REPLACE: {
      // We should have a duplicate within the secondaries.
      std::unordered_set<int> s;
      cet::transform_all(seq,
                         std::inserter(s, s.begin()),
                         [](art::EventID const& eid) { return eid.event(); });
      BOOST_TEST(seq.size() > s.size());
    } break;
    case art::MixHelper::Mode::RANDOM_LIM_REPLACE:
      if (testNoLimEventDupes_) {
        // We should have no duplicate within the secondaries.
        std::unordered_set<int> s;
        cet::transform_all(
          seq, std::inserter(s, s.begin()), [](art::EventID const& eid) {
            return eid.event() + eid.subRun() * 100 + (eid.run() - 1) * 500;
          });
        BOOST_TEST(seq.size() == s.size());
      } else { // Require dupes over 2 events.
        auto checkpoint(allEvents_.size());
        cet::transform_all(
          seq, std::back_inserter(allEvents_), [](art::EventID const& eid) {
            return eid.event() + eid.subRun() * 100 + (eid.run() - 1) * 500;
          });
        uniqueEvents_.insert(allEvents_.cbegin() + checkpoint,
                             allEvents_.cend());
        // Test at end job for duplicates.
      }
      break;
    case art::MixHelper::Mode::RANDOM_NO_REPLACE: {
      auto checkpoint(allEvents_.size());
      cet::transform_all(
        seq, std::back_inserter(allEvents_), [](art::EventID const& eid) {
          return eid.event() + eid.subRun() * 100 + (eid.run() - 1) * 500;
        });
      uniqueEvents_.insert(allEvents_.cbegin() + checkpoint, allEvents_.cend());
      // Test at end job for no duplicates.
    } break;
    default:;
  }
}

void
arttest::MixFilterTestDetail::processEventAuxiliaries(
  art::EventAuxiliarySequence const& seq)
{
  processEventAuxiliaries_called_ = true;
  // We only need a very simple test to check that there actually
  // are auxiliaries in the sequence.  No need for full coverage,
  // the event ids test takes care of that.
  if ((readMode_ == art::MixHelper::Mode::SEQUENTIAL) && (currentEvent_ == 0)) {
    BOOST_TEST_REQUIRE(seq.size() == nSecondaries_);
    size_t offset = 1;
    for (auto const& val : seq) {
      BOOST_TEST_REQUIRE(val.event() == offset);
      ++offset;
    }
  }
}

void
arttest::MixFilterTestDetail::finalizeEvent(art::Event& e)
{
  e.put(std::make_unique<std::string>("BlahBlahBlah"));
  e.put(std::move(eIDs_));
#ifndef ART_TEST_NO_STARTEVENT
  BOOST_TEST_REQUIRE(startEvent_called_);
  startEvent_called_ = false;
#endif
  BOOST_TEST_REQUIRE(processEventIDs_called_);
  processEventIDs_called_ = false;
  BOOST_TEST_REQUIRE(processEventAuxiliaries_called_);
  processEventAuxiliaries_called_ = false;
}

void
arttest::MixFilterTestDetail::respondToOpenInputFile(art::FileBlock const&)
{
  ++respondFunctionsSeen_;
}

void
arttest::MixFilterTestDetail::respondToCloseInputFile(art::FileBlock const&)
{
  ++respondFunctionsSeen_;
}

void
arttest::MixFilterTestDetail::respondToOpenOutputFiles(art::FileBlock const&)
{
  ++respondFunctionsSeen_;
}

void
arttest::MixFilterTestDetail::respondToCloseOutputFiles(art::FileBlock const&)
{
  ++respondFunctionsSeen_;
}

void
arttest::MixFilterTestDetail::beginSubRun(art::SubRun const&)
{
  ++beginSubRunCounter_;
  subRunInfo_ = 0.0;
}

void
arttest::MixFilterTestDetail::endSubRun(art::SubRun& sr)
{
  ++endSubRunCounter_;
  sr.put(std::make_unique<double>(subRunInfo_));
}

void
arttest::MixFilterTestDetail::beginRun(art::Run const&)
{
  ++beginRunCounter_;
  runInfo_ = 0.0;
}

void
arttest::MixFilterTestDetail::endRun(art::Run& r)
{
  ++endRunCounter_;
  r.put(std::make_unique<double>(runInfo_));
}

template <typename PROD, typename OPROD>
bool
arttest::MixFilterTestDetail::mixByAddition(std::vector<PROD const*> const& in,
                                            OPROD& out,
                                            art::PtrRemapper const&)
{
  verifyInSize(in);
  for (auto const* prod : in) {
    if (prod != nullptr) {
      out += *prod;
    }
  }
  return true; //  Always want product in event.
}

bool
arttest::MixFilterTestDetail::aggregateDoubleCollection(
  std::vector<std::vector<double> const*> const& in,
  std::vector<double>& out,
  art::PtrRemapper const&)
{
  verifyInSize(in);
  art::flattenCollections(in, out, doubleVectorOffsets_);
  return true; //  Always want product in event.
}

bool
arttest::MixFilterTestDetail::aggregate_map_vector(
  std::vector<mv_t const*> const& in,
  mv_t& out,
  art::PtrRemapper const&)
{
  verifyInSize(in);
  art::flattenCollections(in, out, map_vectorOffsets_);
  return true; //  Always want product in event.
}

bool
arttest::MixFilterTestDetail::mixPtrs(
  std::vector<std::vector<art::Ptr<double>> const*> const& in,
  std::vector<art::Ptr<double>>& out,
  art::PtrRemapper const& remap)
{
  verifyInSize(in);
  remap(in, std::back_inserter(out), doubleVectorOffsets_);
  if (testPtrFailure_) {
    BOOST_REQUIRE_THROW(*out.front(), art::Exception);
  }
  return true; //  Always want product in event.
}

#ifndef ART_NO_MIX_PTRVECTOR
bool
arttest::MixFilterTestDetail::mixPtrVectors(
  std::vector<art::PtrVector<double> const*> const& in,
  art::PtrVector<double>& out,
  art::PtrRemapper const& remap)
{
  verifyInSize(in);
  remap(in, std::back_inserter(out), doubleVectorOffsets_);
  return true; //  Always want product in event.
}
#endif

bool
arttest::MixFilterTestDetail::mixProductWithPtrs(
  std::vector<arttest::ProductWithPtrs const*> const& in,
  arttest::ProductWithPtrs& out,
  art::PtrRemapper const& remap)
{
  verifyInSize(in);
#ifndef ART_NO_MIX_PTRVECTOR
  remap(in,
        std::back_inserter(out.ptrVectorDouble()),
        doubleVectorOffsets_,
        &arttest::ProductWithPtrs::ptrVectorDouble);
#endif
  remap(in,
        std::back_inserter(out.vectorPtrDouble()),
        doubleVectorOffsets_,
        &arttest::ProductWithPtrs::vectorPtrDouble);
  // Throw-away object to test non-standard remap interface.
  arttest::ProductWithPtrs tmp;
  remap(in,
        std::back_inserter(tmp.vectorPtrDouble()),
        doubleVectorOffsets_,
        &arttest::ProductWithPtrs::vpd_);
  return true; //  Always want product in event.
}

bool
arttest::MixFilterTestDetail::mixmap_vectorPtrs(
  std::vector<std::vector<
    art::Ptr<cet::map_vector<unsigned int>::value_type>> const*> const& in,
  std::vector<art::Ptr<cet::map_vector<unsigned int>::value_type>>& out,
  art::PtrRemapper const& remap)
{
  verifyInSize(in);
  remap(in, std::back_inserter(out), map_vectorOffsets_);
  return true; //  Always want product in event.
}

bool
arttest::MixFilterTestDetail::mixSRProduct(std::vector<double const*> const& in,
                                           DoubleProduct& out,
                                           art::PtrRemapper const& remap)
{
  mixByAddition(in, out, remap);
  subRunInfo_ += out.value;
  return true;
}

bool
arttest::MixFilterTestDetail::mixRProduct(std::vector<double const*> const& in,
                                          DoubleProduct& out,
                                          art::PtrRemapper const& remap)
{
  mixByAddition(in, out, remap);
  runInfo_ += out.value;
  return true;
}

DEFINE_ART_MODULE(arttest::ART_MFT)

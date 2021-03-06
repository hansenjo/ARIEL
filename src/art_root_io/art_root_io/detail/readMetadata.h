#ifndef art_root_io_detail_readMetadata_h
#define art_root_io_detail_readMetadata_h

#include "TBranch.h"
#include "art/Framework/Core/InputSourceMutex.h"
#include "art_root_io/detail/getObjectRequireDict.h"
#include "canvas/Persistency/Provenance/rootNames.h"
#include "canvas/Utilities/TypeID.h"

namespace art {
  namespace detail {
    template <typename T>
    T
    readMetadata(TTree* md, bool const requireDict = true)
    {
      InputSourceMutexSentry sentry;
      auto branch = md->GetBranch(art::rootNames::metaBranchRootName<T>());
      assert(branch != nullptr);
      auto mdField = requireDict ? root::getObjectRequireDict<T>() : T{};
      auto field_ptr = &mdField;
      branch->SetAddress(&field_ptr);
      input::getEntry(branch, 0);
      branch->SetAddress(nullptr);
      return mdField;
    }

    template <typename T>
    bool
    readMetadata(TTree* md, T& field, bool const requireDict = true)
    {
      InputSourceMutexSentry sentry;
      auto branch = md->GetBranch(art::rootNames::metaBranchRootName<T>());
      if (branch == nullptr) {
        return false;
      }
      auto mdField = requireDict ? root::getObjectRequireDict<T>() : T{};
      auto field_ptr = &mdField;
      branch->SetAddress(&field_ptr);
      input::getEntry(branch, 0);
      branch->SetAddress(nullptr);
      std::swap(mdField, field);
      return true;
    }
  } // namespace detail
} // namespace art

#endif /* art_root_io_detail_readMetadata_h */

// Local Variables:
// mode: c++
// End:

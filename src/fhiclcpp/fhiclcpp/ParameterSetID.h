#ifndef fhiclcpp_ParameterSetID_h
#define fhiclcpp_ParameterSetID_h

// ======================================================================
//
// ParameterSetID
//
// ======================================================================

#include "cetlib/sha1.h"
#include "fhiclcpp/fwd.h"

#include <cstdlib>
#include <ostream>
#include <string>

namespace fhicl {
  std::ostream& operator<<(std::ostream&, ParameterSetID const&);
}

// ----------------------------------------------------------------------

class fhicl::ParameterSetID {
public:
  // c'tor's:
  ParameterSetID() noexcept;
  explicit ParameterSetID(ParameterSet const&);
  explicit ParameterSetID(std::string const& id);

  // observers:
  bool is_valid() const noexcept;
  std::string to_string() const;
  static constexpr std::size_t max_str_size() noexcept;

  // mutators:
  void invalidate() noexcept;
  void reset(ParameterSet const&);
  void swap(ParameterSetID&);

  // comparators:
  bool operator==(ParameterSetID const&) const noexcept;
  bool operator!=(ParameterSetID const&) const noexcept;
  bool operator<(ParameterSetID const&) const noexcept;
  bool operator>(ParameterSetID const&) const noexcept;
  bool operator<=(ParameterSetID const&) const noexcept;
  bool operator>=(ParameterSetID const&) const noexcept;

private:
  bool valid_;
  cet::sha1::digest_t id_;

}; // ParameterSetID

inline constexpr std::size_t
fhicl::ParameterSetID::max_str_size() noexcept
{
  // Two hex digits per byte.
  return 2 * cet::sha1::digest_sz;
}

// ======================================================================

#endif /* fhiclcpp_ParameterSetID_h */

// Local Variables:
// mode: c++
// End:

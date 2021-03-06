#ifndef Utilities_sqrtOrThrow_h
#define Utilities_sqrtOrThrow_h

//
//  Take the sqrt of its argument but protect against
//  roundoff error that can take the argument negative.
//
//  If the argument is only a little negative, assume
//  that this is round off error and set the answer
//  to zero.  If the argument is very negative, throw.
//
//  See also: safeSqrt.hh
//

#include <cmath>

namespace tex {

  // Some helper functions.
  // These are found in ../src/sqrtOrThrow.cc and are used to avoid code bloat
  // in the instantiated templates.
  void throwHelperForSqrtOrThrow(double, double);
  void throwHelperForSqrtOrThrow(float, float);

  template <typename T>
  T
  sqrtOrThrow(T x, T eps)
  {
    T retval(0.);
    if (x > 0.) {
      retval = sqrt(x);
    } else if (x < -eps) {
      throwHelperForSqrtOrThrow(x, eps);
    }
    return retval;
  }

}
#endif /* GeneralUtilities_sqrtOrThrow_h */

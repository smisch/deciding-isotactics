#ifndef Job_hpp
#define Job_hpp

#include <algorithm> // std::max_element
#include <vector> // std::vector
#include <boost/multiprecision/cpp_int.hpp> // boost::multiprecision::cpp_int
#include "IsoSearch.hpp" // IsoStatus

struct Job
{
  /*
  Job(boost::multiprecision::cpp_int binaryRelationCode, short permissiveness, IsoStatus isoStatus = unknown, short parentBestComplexity = -1) : binaryRelationCode(binaryRelationCode), permissiveness(permissiveness), isoStatus(isoStatus), parentBestComplexity(parentBestComplexity)
  {}
  */
  
  /**
   * Binary representation of the binary relation.
   */
  boost::multiprecision::cpp_int binaryRelationCode;
  
  /**
   * This binary relation's permissiveness.
   */
  short permissiveness;
  
  /**
   * iso: this job has already been determined to be iso.
   * notIso: this job has already been determinzed to be not iso.
   * unknown: this job's isoStatus is unknown.
   */
  IsoStatus isoStatus = unknown;
  
  /**
   * If the parent's iso status is iso, we can pass down the best complexity for
   * which the parent's alignment was iso.
   *
   * This value can be used when the seach space is iterated by growing the binary relation.
   */
  short parentBestComplexity = -1;
};

#endif /* Job_hpp */

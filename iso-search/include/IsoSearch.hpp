#ifndef __ISO_SEARCH_HPP__
#define __ISO_SEARCH_HPP__

#include <string>
#include <vector>
#include <utility> // std::pair
#include <mutex>

#include <boost/multiprecision/cpp_int.hpp>

using boost::multiprecision::cpp_int;

using symbol_set_t            = std::vector<std::string>;
using binary_relation_t       = std::vector<std::pair<std::string, std::string>>;
using binary_relation_short_t = std::vector<std::pair<short, short>>;

enum IsoStatus { iso, notIso, unknown, skipped };

struct IsoSearch
{
  /**
   * @example "m1.dot"
   */
  std::string m1;
  
  /**
   * @example "m2.dot"
   */
  std::string m2;

  /**
   * @example {a,b}
   */
  symbol_set_t s1;
  
  /**
   * @example {x,y,z}
   */
  symbol_set_t s2;

  /**
   * @example {(a,x), (b,y), (b,z)}
   */
  binary_relation_t R_all;

  /**
   * @example {(0,0), (1,1), (1,2)}
   */
  binary_relation_short_t R_all_short;
  
  /**
   * Number of subsets of S_1 x S_2. Comes to 2^(|S_1| + |S_2|).
   *
   * @example 2^(2+3) = 2^5 = 32
   */
  cpp_int maxBinaryRelationCode;

  /**
   * global maxmimum complexity, comes to |S_1| * |S_2|
   */
  short kMax;

  /**
   * Stores the currently best known alignment,
   * meaning the one with minimal max_pc.
   */
  cpp_int best_BinaryRelationCode;

  /**
   * Permissiveness of the currently best known alignment.
   */
  short best_permissiveness;
  
  /**
   * Complexity of the currently best known alignment.
   */
  short best_complexity;
  
  /**
   * Size of the currently best known alignment.
   */
  short best_max_pc;
  
  /**
   * Mutex to synchronize access to the best known alignment.
   *
   * Prevents two threads from both setting a new best concurrently.
   */
  std::mutex accessMutex;
  
  /**
   * Statistic: number of total isotactics checks
   */
  long stats_iso_tests = 0;
  
  /**
   * Statistic: number of microseconds spent in iso-decision,
   * as opposed to iterating the search space
   */
  double inDecision = 0;
  
  IsoSearch (symbol_set_t s1, symbol_set_t s2) : s1(s1), s2(s2)
  {
    kMax = s1.size() * s2.size();
    best_permissiveness = s1.size() + s2.size();
    best_complexity = s1.size() * s2.size();
    best_max_pc = best_complexity;
    
    maxBinaryRelationCode = (1 << (s1.size() * s2.size()));

    fillRelations();
  }
  
  bool set_best_max_pc (short new_best_max_pc, cpp_int binaryRelationCode)
  {
    std::lock_guard<std::mutex> accessLock(accessMutex);
    
    // only set if it is really smaller.
    if (new_best_max_pc < best_max_pc)
    {
      best_max_pc = new_best_max_pc;
      best_BinaryRelationCode = binaryRelationCode;
      
      return true;
    }
    
    return false;
  }
  
  short get_best_max_pc ()
  {
    return best_max_pc;
  }

private:
  void fillRelations ()
  {
    for(short i2 = 0, i2_max = s2.size(); i2 < i2_max; ++i2)
    {
      for(short i1 = 0, i1_max = s1.size(); i1 < i1_max; ++i1)
      {
        R_all.push_back({s1[i1], s2[i2]});
        R_all_short.push_back({i1, i2});
      }
    }
  }
};

#endif // __ISO_SEARCH__

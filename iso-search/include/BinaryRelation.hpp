#ifndef __BINARY_RELATION_HPP__
#define __BINARY_RELATION_HPP__

#define DRAW_NODE_LABELS_IN_LINE true

#include <boost/multiprecision/cpp_int.hpp>

#include "IsoSearch.hpp"

using boost::multiprecision::cpp_int;

using binary_relation_t       = std::vector<std::pair<std::string, std::string>>;

struct BinaryRelation
{
  cpp_int binaryRelationCode;
  
  binary_relation_t binaryRelation;
  
  IsoSearch& isoSearch;
  
  short permissiveness;
  
  BinaryRelation (cpp_int binaryRelationCode, IsoSearch& isoSearch) : binaryRelationCode(binaryRelationCode), isoSearch(isoSearch)
  {
    for(int i = 0, i_max = isoSearch.R_all.size(); i < i_max; ++i)
    {
      // if new node includes this pair
      if (binaryRelationCode & (1 << i))
        // push it into R
        binaryRelation.push_back( isoSearch.R_all[i] );
    }
    
    permissiveness = calculatePermissiveness();
  }
  
  BinaryRelation (cpp_int binaryRelationCode, short permissiveness, IsoSearch& isoSearch) : binaryRelationCode(binaryRelationCode), isoSearch(isoSearch), permissiveness(permissiveness)
  {
    for(int i = 0, i_max = isoSearch.R_all.size(); i < i_max; ++i)
    {
      // if new node includes this pair
      if (binaryRelationCode & (1 << i))
        // push it into R
        binaryRelation.push_back( isoSearch.R_all[i] );
    }
  }
  
  short calculatePermissiveness ()
  {
    return calculatePermissiveness(binaryRelationCode);
  }
  short calculatePermissiveness (cpp_int binaryRelationCode)
  {
    short s1Size = isoSearch.s1.size();
    
    // create a vector where each element represents a symbol
    std::vector<short> partnerCount(s1Size + isoSearch.s2.size());
    
    // for each symbol count how often it is present in a pair
    for(int i = 0, i_max = isoSearch.R_all_short.size(); i < i_max; ++i)
    {
      // if node contains this pair
      if (binaryRelationCode & (1 << i))
      {
        ++partnerCount[ isoSearch.R_all_short[i].first ];
        ++partnerCount[ isoSearch.R_all_short[i].second + s1Size ];
      }
    }
    
    // return the maximum over all symbols, which is the permissiveness
    return *std::max_element(partnerCount.begin(), partnerCount.end());
  }
  
  bool containsAllSymbols ()
  {
    return relationContainsAllSymbolsLeft()
        && relationContainsAllSymbolsRight();
  }
  
  std::string toString ()
  {
    if (binaryRelationCode == 0)
    {
      return "";
    }
  
    // buffer to write the node string
    std::ostringstream outbuffer;
    
    bool firstPairPrinted = false;
    // for all symbols in non-binary representation (we use them to access vector s)
    for(long j = 0, jmax = isoSearch.R_all_short.size(); j < jmax; ++j)
    {
      // if j is set in i, add it to the output
      if (binaryRelationCode & (1 << j))
      {
        if (firstPairPrinted)
        {
#if DRAW_NODE_LABELS_IN_LINE
          outbuffer << ",";
#else
          outbuffer << "\n";
#endif
        }
        
        outbuffer << "(" << isoSearch.R_all[ j ].first << "," << isoSearch.R_all[ j ].second << ")";
        
        firstPairPrinted = true;
      }
    }
    
    return outbuffer.str();
  }
  
private:
  bool relationContainsAllSymbolsLeft ()
  {
    for(auto& l : isoSearch.s1)
    {
      // current symbol not found
      int found = false;
      
      // look for it
      for(auto& pair : binaryRelation)
      {
        // found it?
        if (l == pair.first)
        {
          // found it!
          found = true;
          
          // break out of pair loop to continue with next symbol
          break;
        }
      }
      
      // in case we didn't find this symbol, abort
      if (! found)
        return false;
    }
    
    // all symbols were found
    return true;
  }
  
  bool relationContainsAllSymbolsRight ()
  {
    for(auto& l : isoSearch.s2)
    {
      // current symbol not found
      int found = false;
      
      // look for it
      for(auto& pair : binaryRelation)
      {
        // found it?
        if (l == pair.second)
        {
          // found it!
          found = true;
          
          // break out of pair loop to continue with next symbol
          break;
        }
      }
      
      // in case we didn't find this symbol, abort
      if (! found)
        return false;
    }
    
    // all symbols were found
    return true;
  }
};

#endif // __BINARY_RELATION_HPP__

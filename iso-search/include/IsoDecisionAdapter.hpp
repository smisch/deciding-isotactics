#ifndef __ISO_DECISION_ADAPTER_HPP__
#define __ISO_DECISION_ADAPTER_HPP__

#include <mutex>
#include <boost/graph/copy.hpp>

#include "AlignmentUtils.hpp"
#include "CompareUtils.hpp"
#include "GraphUtils.hpp"
#include "Utils.hpp"
#include "WitnessUtils.hpp"
#include "MatchUtils.hpp"
#include "DetWitnessUtils.hpp"
#include "DetGraph.hpp"


bool isCached = false;
Graph_t g1Cache;
Graph_t g2Cache;
std::mutex cacheWriteMutex;

bool isIsotactic (std::string m1, std::string m2, alignment alm)
{
  if (! isCached)
  {
    std::lock_guard<std::mutex> lock(cacheWriteMutex);
    
    if (! isCached)
    {
      // read and parse the input automatons
      g1Cache = Graph::parse(m1);
      g2Cache = Graph::parse(m2);
    
      isCached = true;
    }
  }
  
  Graph_t g1;
  Graph_t g2;
  
  copy_graph(g1Cache, g1);
  copy_graph(g2Cache, g2);
  
  // extract the sets of alignment groups for both the left and right side of the alignment.
  // for each label assign the groups it is contained in.
  labelGroupingMap lgm1 = Helper::LabelGroupingMap(g1, Alm::Lhs(alm));
  labelGroupingMap lgm2 = Helper::LabelGroupingMap(g2, Alm::Rhs(alm));

  // helper: get just the alignment groups without knowing to which label they belong.
  edgeLabelSet els1 = Helper::lgmFlatten(lgm1);
  edgeLabelSet els2 = Helper::lgmFlatten(lgm2);

  // in g1/g2 for each edge fill the "gp" property that contains
  // the set of alignment groups, that contain the edge label.
  // this will be used in DG::determinize to remove non-determinism
  // w.r.t. the alignment
  Helper::labelsToGroupings(g1, lgm1);
  Helper::labelsToGroupings(g2, lgm2);

  // remove non-determinism wrt. the alignment:
  // - merge edges (and vertices) that have the same set of alignment groups
  // - eliminate edges that have a label not contained in the alignment (epsilon-closure)
  DG_t dg1 = DG::determinize(g1, els1);
  DG_t dg2 = DG::determinize(g2, els2);

  WG_t wg = WG::create(dg1, dg2, lgm1, lgm2, alm);

  DWG_t dwg1 = DWG::createLhs(wg, els1);
  bool leftEqual = Cmp::isEqual(dg1, dwg1, lgm1);
  
  // if ! leftEqual, exit here and don't compute rightEqual
  if (! leftEqual)
    return false;
  
  // assert(leftEqual)
  
  DWG_t dwg2 = DWG::createRhs(wg, els2);
  bool rightEqual = Cmp::isEqual(dg2, dwg2, lgm2);

  // leftEqual was found to be true. if rightEqual then they are both true.
  return rightEqual;
}

#endif // __ISO_DECISION_ADAPTER_HPP__

#ifndef __WITNESSUTILS_HPP__
#define __WITNESSUTILS_HPP__

#include <string>
#include <vector>

#include <boost/graph/graphviz.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/range/iterator_range.hpp>

#include "AlignmentUtils.hpp"
#include "DetGraph.hpp"
#include "HelperMaps.hpp"
#include "MatchUtils.hpp"
#include "Utils.hpp"


struct wgVertexProps {
  std::string name;
  std::string v1Name;
  std::string v2Name;
  matchSet ms;

  std::string role;
};

struct wgEdgeProps {
  std::string name;

  alignmentGrouping gp1;
  alignmentGrouping gp2;
};

struct wgProps {
  std::string name;
};

using WG_t = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS,
                                    wgVertexProps, wgEdgeProps, wgProps>;

using vName = std::string;

namespace WG {
  struct Vertex {
    std::string name;
    std::string v1Name;
    std::string v2Name;
    matchSet ms;

    std::string role;
  };

  using vDesc = WG_t::vertex_descriptor;
  using eDesc = WG_t::edge_descriptor;

  using vIter = WG_t::vertex_iterator;
  using vIterPair = std::pair<vIter, vIter>;

  using eIter = WG_t::edge_iterator;
  using eIterPair = std::pair<eIter, eIter>;

  using oeIter = WG_t::out_edge_iterator;
  using oeIterPair = std::pair<oeIter, oeIter>;

  WG_t create(const DG_t &g1, const DG_t &g2, labelGroupingMap &lgm1,
              labelGroupingMap &lgm2, const alignment &alm);

  WG::Vertex createVertex(const vName &v1, const vName &v2, const matchSet &ms);
  WG::vDesc getVertex(const WG::Vertex &v, const WG_t &wg);
  WG::vDesc addVertex(const WG::Vertex &v, WG_t &wg);

  WG::eDesc addEdge(WG::vDesc &v1, const alignmentGrouping &gp1,
                    const alignmentGrouping &gp2, WG::vDesc &v2, WG_t &wg);

  std::string getVertexName(const WG::Vertex &v);
  WG::vDesc getStart(const WG_t &wg);

  bool isFinalState(const WG::vDesc &wgv);

  bool hasVertex(const WG::Vertex &v, const WG_t &wg);
  bool vertexEqual(const WG::Vertex &v1, const WG::Vertex &v2);

  bool hasEdgeLhs(const std::vector<WG::eDesc> &oes,const alignmentGrouping &gp, const WG_t &wg);
  bool hasEdgeRhs(const std::vector<WG::eDesc> &oes, const alignmentGrouping &gp, const WG_t &wg);

  bool hasEmptyTransitionLhs(const Range<WG::oeIter> &oes, const WG_t &wg);
  bool hasEmptyTransitionRhs(const Range<WG::oeIter> &oes, const WG_t &wg);

  bool hasEmptyTransitionLhs(const std::vector<WG::eDesc> &oes, const WG_t &wg);
  bool hasEmptyTransitionRhs(const std::vector<WG::eDesc> &oes, const WG_t &wg);

  std::vector<WG::eDesc> getEdgesWithLabelLhs(const std::vector<WG::eDesc> &oes, const alignmentGrouping &gp, const WG_t &wg);
  std::vector<WG::eDesc> getEdgesWithLabelRhs(const std::vector<WG::eDesc> &oes, const alignmentGrouping &gp, const WG_t &wg);


  std::vector<WG::vDesc> getDestinations(const std::vector<WG::eDesc> &es, const WG_t &wg);

  std::vector<WG::eDesc> getEmptyEdgesLhs(const Range<WG::oeIter> &oes, const WG_t &wg);
  std::vector<WG::eDesc> getEmptyEdgesRhs(const Range<WG::oeIter> &oes, const WG_t &wg);




  void print(const WG_t &wg);
  void write(const WG_t &wg, std::ostream& target);
  void writeOutEdges(const WG_t &wg, const WG::vDesc &v, std::ostream& target);
  void writeOutEdge(const WG_t &wg, const WG::eDesc &e, std::ostream& target);

  void printDebug(const WG_t &wg);
  void printOutEdgesDebug(const WG_t &wg, const WG::vDesc &v);
  void printOutEdgeDebug(const WG_t &wg, const WG::eDesc &e);
}

#endif // __WITNESSUTILS_HPP__

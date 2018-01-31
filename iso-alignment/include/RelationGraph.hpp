#ifndef __RELATION_GRAPH_HPP__
#define __RELATION_GRAPH_HPP__

// false iff only the pair of nodes that created this
// node should have edges to it. otherwise draw all
// edges (takes longer).
#define DRAW_ALL_EDGES false

// true iff you want labels on the edges of the *.dot output
#define DRAW_EDGE_LABELS false

// cycle through some colors to draw edges and their labels
#define DRAW_EDGES_IN_COLOR false

// true iff you want label names be a list of comma-separated pairs.
// will put all pairs below one another otherwise
#define DRAW_NODE_LABELS_IN_LINE true

// unordered_map (=hash map) performs better than map (red-black tree),
// but messes up the order in which the nodes are printed into dot.
// the performance boost is minimal.
#define USE_PERFORMANCE_TYPE false

// unordered_map faster than map, but messes with node order
#if USE_PERFORMANCE_TYPE
  // hash map has O(1) inserts and finds
  // output order is undefined (on my machine reverse of insert order)
  #define MAP_TYPE unordered_map
#else
  // red-black tree has O(log n) inserts and finds
  // output order is defined via key from smallest to largest
  #define MAP_TYPE map
#endif

#define VISIT_ALL_R_NODES true



#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <stdlib.h>       // system

#include <memory>         // std::shared_ptr, std::weak_ptr
#include <queue>          // std::queue
#include <map>            // std::map, std::multimap
#include <unordered_map>  // std::unordered_map
#include <chrono>         // for time measurement
#include <cmath>          // for log2 (colors for edges)

#include "AlignmentGraph.hpp"


#include "AlignmentUtils.hpp"
#include "CompareUtils.hpp"
#include "GraphUtils.hpp"
#include "Utils.hpp"
#include "WitnessUtils.hpp"
#include "MatchUtils.hpp"
#include "DetWitnessUtils.hpp"
#include "DetGraph.hpp"

namespace RG
{
  // forward declaration used by following type aliasing
  struct RelationsNode;

  // owning: contains a shared_ptr that keeps the object alive
  // non-owning: contains a weak_ptr that does not keep the object alive
  //             if all other references are deleted
  using owning_node_t           = std::shared_ptr<RelationsNode>;
  using owning_node_list_t      = std::MAP_TYPE<long, owning_node_t>;
  using non_owning_node_t       = std::weak_ptr<RelationsNode>;
  using non_owning_node_list_t  = std::MAP_TYPE<long, non_owning_node_t>;
  using i_t                     = std::vector<non_owning_node_t>::size_type;

  using non_owning_node_queue_t = std::queue<non_owning_node_t>;
  using non_owning_node_map_t   = std::multimap<long, non_owning_node_t>;

  using symbol_set_t            = std::vector<std::string>;
  using binary_relation_t       = std::vector<std::pair<std::string, std::string>>;
  using binary_relation_short_t = std::vector<std::pair<short, short>>;

  struct RelationsNode
  {
    long id;

    std::MAP_TYPE<long, std::pair<std::shared_ptr<RelationsNode>, short>> nextNodes;

    RelationsNode(long id)
      : id(id)
    {}

    short getK()
    {
      return __builtin_popcountll(id);
    }
  };

  struct RelationsGraph
  {
    std::string m1;
    std::string m2;

    symbol_set_t s1;
    symbol_set_t s2;

    binary_relation_t R_all;
    binary_relation_short_t R_all_short;

    std::shared_ptr<RelationsNode> root;
    std::MAP_TYPE<long, std::weak_ptr<RelationsNode>> nodes;

    std::unordered_map<long, short> isotacticRelations;

    short kMax;

    non_owning_node_queue_t nextToProccess;

    std::unordered_map<long, std::shared_ptr<AG::AlignmentGraph>> alignmentGraphs;

    std::vector<std::string> colors{"tomato", "cornflowerblue", "forestgreen",
      "darkviolet", "goldenrod", "deeppink"};

    short best_permissiveness;
    short best_complexity;
    short best_max_pc;

    std::streambuf *cerrBuffer;


    double AGTime = 0;

    int stats_iso_tests_max;
    int stats_iso_depth_max;

    int stats_iso_tests = 0;
    int stats_iso_tests_R = 0;
    int stats_iso_yes = 0;
    int stats_iso_no = 0;
    int stats_iso_segfault = 0;
    int stats_skip_1 = 0; // skip R-nodes that have max_p > best_max_p
    int stats_skip_2 = 0; // skip R-k-nodes that have max_pc >= best_max_pc
    int stats_skip_3 = 0; // skip R-nodes that have max_p >= best_max_pc
    int stats_skip_4 = 0; // skip R-nodes where not all labels are assigned due to segfaults

    RelationsGraph (symbol_set_t s1, symbol_set_t s2)
      : s1(s1), s2(s2)
    {
      kMax = s1.size() * s2.size();
      best_permissiveness = s1.size() + s2.size();
      best_complexity = s1.size() * s2.size();
      best_max_pc = best_complexity;

      fillRelations();

      stats_iso_tests_max = 1 << R_all.size();
      stats_iso_depth_max = s1.size() * s2.size();

      root = std::make_shared<RelationsNode>(0);

      nodes.insert({root->id, root});
    }

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

    void populateInitial ()
    {
      for(short i = 0, i_max = R_all_short.size(); i < i_max; ++i)
      {
        auto node = std::make_shared<RelationsNode>(1 << i);

        root->nextNodes.insert({node->id, {node, 0}});

        nodes.insert({node->id, node});

        nextToProccess.push(node);
      }
    }

    void populateRecursive ()
    {
      auto start = std::chrono::high_resolution_clock::now();
      double previousSecond = -1;

      while (! nextToProccess.empty())
      {
        auto currentNodeWeak = nextToProccess.front();
        nextToProccess.pop();

        if (auto currentNode = currentNodeWeak.lock())
        {
          for(short i = 0, i_max = R_all_short.size(); i < i_max; ++i)
          {
            double diff = std::chrono::duration_cast<std::chrono::seconds>( std::chrono::high_resolution_clock::now() - start ).count();

            if (diff > previousSecond)
            {
              double testsPerSecond = stats_iso_tests / diff;
              double diffMicro = std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - start ).count();

              // output stats
              std::cerr
                << "tests: " << stats_iso_tests << "/" << stats_iso_tests_max
                << " (" << std::to_string(stats_iso_tests / (double)stats_iso_tests_max * 100) << "%)"
                << ", yes: " << stats_iso_yes
                << ", depth: " << currentNode->getK() << "/" << stats_iso_depth_max
                << ", tests/sec: " << testsPerSecond
                << ", sec to finished: " << (stats_iso_tests_max / testsPerSecond)
                << ", timeInIso: " << std::to_string(AGTime / diffMicro * 100) << "%"
              << std::endl;

              previousSecond = diff;
            }

            long newPairId = 1 << i;

            // test if this pair is already in this node's list
            if (currentNode->id & newPairId)
              // if it is, continue with next pair
              continue;

            long newNodeId = currentNode->id | newPairId;

            auto nodesIterator = nodes.find(newNodeId);

            if (nodesIterator == nodes.end())
            {
              // create new node
              auto node = std::make_shared<RelationsNode>(newNodeId);

              // associate new node with its parents
              currentNode->nextNodes.insert({node->id, {node, newPairId}});

              // insert new node in global node list
              nodes.insert({node->id, node});

              // in case we defined some automatons m1 and m2, queue the actual job
              if (m1 != "")
              {
                bool continueWithChildren = newRJob(currentNode, node, newPairId);

                if (continueWithChildren)
                {

                  // insert new node into queue to process its children next
                  nextToProccess.push(node);
                }
              }
            }
            #if DRAW_ALL_EDGES
            else
            {
              auto oldNode = (*nodesIterator).second.lock();

              currentNode->nextNodes.insert({oldNode->id, {oldNode, newPairId}});
            }
            #endif
          }
        }
      }
    }

    /**
     * returns true if we should continue with this node's children, false
     * if we should stop searching on this node and discard it's children.
     */
    bool newRJob(std::shared_ptr<RelationsNode>& currentNode, std::shared_ptr<RelationsNode>& newNode, short newPair)
    {
      // calculate the permissiveness for this relation
      short permissiveness = calculatePermissiveness(newNode->id);

      #if false
      // skip this relation if our best hit has a better permissiveness
      if (best_permissiveness < permissiveness)
      {
        /*
        ;// std::cerr << "skipping due permissiveness R={"
          << toString(newNode->id) << "}"
          << " permissiveness=" << permissiveness
          << std::endl;
        */

        ++stats_skip_1;
        return false;
      }
      #endif

      #if false
      // skip this relation if our best hit has a better max_pc
      if (best_max_pc <= permissiveness)
      {
        ++stats_skip_2;
        return false;
      }
      #endif

      // todo: look for old graph and expand it for the new
      // one instead of creating a new one
      // alignmentGraphs.find(currentNode)
      binary_relation_t R;

      for(int i = 0, i_max = R_all.size(); i < i_max; ++i)
      {
        // if new node includes this pair
        if (newNode->id & (1 << i))
          // push it into R
          R.push_back( R_all[i] );
      }

      #if true
      // this is a temporary workaround until iso-decision accepts alignemnts
      // where some symbols are missing. currently it fails with a seg fault.
      if (! relationContainsAllSymbols(R))
      {
        // ;// std::cerr << "skipping relation that doesn't have all symbols" << std::endl;
        ++stats_skip_4;
        isotacticRelations.insert({newNode->id, 3});
        return true;
      }
      #endif

      auto ag = std::make_shared<AG::AlignmentGraph>(s1, s2, R, kMax);
      ag->populateInitial();
      ag->populateRecursive();

      int largestK = ag->getLargestK();

      ++stats_iso_tests_R;

      int returnCode = executeIsoDecision(m1, m2, ag->toDecisionAlignment(largestK));

      if (returnCode == IS_ISO)
      {
        // if the permissiveness for these alignments is smaller than our
        // current best, overwrite it.
        if (permissiveness < best_permissiveness)
        {
          best_permissiveness = permissiveness;
          // ;// std::cerr << "new best permissiveness: " << best_permissiveness << "\n";
        }

        int max_pc = permissiveness * largestK;
        if (max_pc < best_max_pc)
        {
          best_max_pc = max_pc;
        }

        isotacticRelations.insert({newNode->id, 1});

        #if true
        std::cerr << "  !!     isotactics: "
          << "R={" << toString(newNode->id) << "}"
          << "Alignment={" << ag->getSortedAlignment() << "}"
          << " permissiveness=" << permissiveness
          << " complexity=" << largestK
          << std::endl << std::flush;
        #endif
      }
      // (1 >> 8) == 256 meaning return code was "1"
      else if (returnCode == IS_NOT_ISO)
      {
        isotacticRelations.insert({newNode->id, 0});

        #if false
        ;// std::cerr << "not iso: "
          << "R={" << toString(newNode->id) << "}"
          << "Alignment={" << ag->getSortedAlignment() << "}"
          << " permissiveness=" << permissiveness
          << std::endl;
        #endif
      }
      // record everything else as segfault
      else
      {
        isotacticRelations.insert({newNode->id, 2});

        #if false
        ;// std::cerr << "segfault: "
          << "R={" << toString(newNode->id) << "}"
          << "Alignment={" << ag->getSortedAlignment() << "}"
          << " permissiveness=" << permissiveness
          << std::endl;
        #endif
      }
      // save the alignment graph for later. only useful if you actually
      // reuse it, for example to iteratively create them.
      // alignmentGraphs.insert({newNode->id, ag});

      if (returnCode == IS_ISO)
      {
        // iterate over all k's
        // std::map<short, std::map<long, std::shared_ptr<AlignmentNode>>> nodeMap
        for(auto& pair : ag->nodeMap)
        {
          short complexity = pair.first;

          // reached the last alignemnt, which we already tested
          if (complexity >= largestK)
            break;

          // calculate this alignemnt's max_pc
          int max_pc = permissiveness * complexity;

          #if false
          // if this max_pc is larger than our best_max_pc,
          // then don't test this alignment
          if (best_max_pc <= max_pc)
          {
            ++stats_skip_3;
            break;
          }
          #endif

          // ;// std::cerr << "testing: " << " permissiveness=" << permissiveness << " complexity=" << complexity << std::endl;
          int returnCodeRk = newRkJob(newNode->id, ag, complexity);

          if (returnCodeRk == IS_ISO)
          {
            #if true
            std::cerr << "    !!!! isotactics: "
              << "R={" << toString(newNode->id) << "}"
              << "Alignment={" << ag->getSortedAlignment(complexity) << "}"
              << " permissiveness=" << permissiveness
              << " complexity=" << complexity
              << std::endl << std::flush;
            #endif

            // if this max_pc is better than our currently best one, overwrite.
            // since we only test alignments that have a smaller max_pc we
            // don't need this check until multithreading.
            if (max_pc < best_max_pc)
            {
              ;// std::cerr << "new best max_pc = " << max_pc << std::endl;
              best_max_pc = max_pc;
            }

            break;
          }
        }


      }
      // only continue the search with this node's children if
      // we didn't find isotactics.
      return true;
      //return ! (returnCode == IS_ISO);
    }

    int newRkJob(long nodeId, std::shared_ptr<AG::AlignmentGraph>& ag, int k)
    {
      // std::string alignmentJson = "jobs/job_" + std::to_string(nodeId) + "_" + std::to_string(k) + ".json";
      // ag->outputAlignmentsToFile(k, alignmentJson);

      return executeIsoDecision(m1, m2, ag->toDecisionAlignment(k));

      // iso-decision returns 0 for isotactics, >0 otherwise
      // return callIsoDecision("iso-decision " + m1 + " " + m2 + " " + alignmentJson + " >/dev/null 2>&1");
    }

    const int IS_ISO = EXIT_SUCCESS;
    const int IS_NOT_ISO = 256; // return code 1 shifted by 8 (1<<8).
    const int SEG_FAULT = 35584;

    /**
     * create a new process (fork on *nix), create a new shell, execute command.
     * returns IS_ISO, IS_NOT_ISO or SEG_FAULT.
     */
    int callIsoDecision(std::string command)
    {
      ++stats_iso_tests;

      // record start time before we call iso-decision
      auto start = std::chrono::high_resolution_clock::now();

      // ;// std::cerr << "executing " << command << std::endl;

      int returnCode = system(command.c_str());

      if (returnCode == IS_ISO)
        ++stats_iso_yes;
      else if (returnCode == IS_NOT_ISO)
        ++stats_iso_no;
      else
        ++stats_iso_segfault;

      // calculate how long the call to iso-decision took and save it for
      // all calls so that we know how much time we spend in iso-decision.
      auto stop = std::chrono::high_resolution_clock::now();
      AGTime += std::chrono::duration_cast<std::chrono::microseconds>( stop - start ).count();

      return returnCode;
    }

    int executeIsoDecision(std::string m1, std::string m2, AG::alignment alm)
    {
      ++stats_iso_tests;

      // record start time before we call iso-decision
      auto start = std::chrono::high_resolution_clock::now();

      // read and parse the input automatons
      Graph_t g1 = Graph::parse(m1);
      Graph_t g2 = Graph::parse(m2);

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
      DWG_t dwg2 = DWG::createRhs(wg, els2);

      bool leftEqual = Cmp::isEqual(dg1, dwg1, lgm1);
      bool rightEqual = Cmp::isEqual(dg2, dwg2, lgm2);
      bool bothEqual = leftEqual && rightEqual;

      // calculate how long the call to iso-decision took and save it for
      // all calls so that we know how much time we spend in iso-decision.
      auto stop = std::chrono::high_resolution_clock::now();
      AGTime += std::chrono::duration_cast<std::chrono::microseconds>( stop - start ).count();

      if (bothEqual)
        ++stats_iso_yes;
      else
        ++stats_iso_no;

      return bothEqual ? IS_ISO : IS_NOT_ISO;
    }

    short calculatePermissiveness (long nodeId)
    {
      short s1Size = s1.size();

      std::vector<short> partnerCount(s1Size + s2.size());

      for(int i = 0, i_max = R_all_short.size(); i < i_max; ++i)
      {
        // if node contains this pair
        if (nodeId & (1 << i))
        {
          ++partnerCount[ R_all_short[i].first ];
          ++partnerCount[ R_all_short[i].second + s1Size ];
        }
      }

      return *std::max_element(partnerCount.begin(), partnerCount.end());
    }

    bool relationContainsAllSymbols (binary_relation_t& R)
    {
      return relationContainsAllSymbolsLeft(R)
          && relationContainsAllSymbolsRight(R);
    }

    bool relationContainsAllSymbolsLeft (binary_relation_t& R)
    {
      for(auto& l : s1)
      {
        // current symbol not found
        int found = false;

        // look for it
        for(auto& pair : R)
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

    bool relationContainsAllSymbolsRight (binary_relation_t& R)
    {
      for(auto& l : s2)
      {
        // current symbol not found
        int found = false;

        // look for it
        for(auto& pair : R)
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

    void outputDot (std::string filename = "rgraph.dot")
    {
      std::ostringstream outbuf;

      outbuf << "digraph RelationGraph{" << std::endl;

      // outbuffer << "\"\" [shape=point]" << endl;
      outbuf << "\"\" [style=filled fillcolor=black width=0.15 height=0.15]" << std::endl;

      // over the graph write all the arguments: symbol sets, R and k
      outbuf << "label=\"" << getArgumentsForDot() << "\";\nlabelloc=\"t\";\n";

      outbuf << "node [color=grey label=\"\" style=filled]" << std::endl;

      outbuf << "edge [color=grey]" << std::endl;

      for(auto& pair : isotacticRelations)
      {
        // iso-decision returned yes
        if (pair.second == 1)
          outbuf << toLabelString(pair.first) << "[color=forestgreen peripheries=2]" << std::endl;

        // iso-decision returned no
        else if (pair.second == 0)
          outbuf << toLabelString(pair.first) << "[color=red]" << std::endl;

        // iso-decision returned segfault
        else if (pair.second == 2)
          outbuf << toLabelString(pair.first) << "[color=gold]" << std::endl;

        // skipped because R didn't contain every label
        else if (pair.second == 3)
            outbuf << toLabelString(pair.first) << "[fontcolor=grey]" << std::endl;
      }

      // walk through the list of all nodes.
      for (auto& pair : nodes)
      {
        // get a weak_ptr to the current node (nodes has weak_ptrs)
        auto currentWeak = pair.second;

        // convert weak_ptr to shared_ptr
        if (auto current = currentWeak.lock())
        {
          // iterate through all connected nodes
          for (auto& pair : current->nextNodes)
          {
            // get a shared_ptr to the other node (nextNodes has shared_ptrs)
            // nextNodes has format: <long, <node, short>> and we want node
            // std::MAP_TYPE<long, std::pair<std::shared_ptr<RelationsNode>, short>>
            auto other = pair.second.first;

            #if DRAW_EDGES_IN_COLOR
              std::string color;
              if (pair.second.second == 0)
                color = "black";
              else
              {
                int nr = log2(pair.second.second);
                color = colors[ nr % colors.size() ];
              }
            #endif

            // output edge
            outbuf << "  "
              << toLabelString(current->id)
              << " -> "
              << toLabelString(other->id)
              << " ["
              #if DRAW_EDGE_LABELS
                << " label="
                << toLabelString(pair.second.second, false)
              #endif
              #if DRAW_EDGES_IN_COLOR
                << " color=" << color << " fontcolor=" << color
              #endif
              << "]"
              << std::endl;
          }
        }
      }

      outbuf << "}" << std::endl;

      // write stringbuffer to file
      std::ofstream outputf;
      outputf.open(filename);
      outputf << outbuf.str();
      outputf.close();
    }

    std::unordered_map<long, std::string> nodeStringCache;

    std::string toLabelString (long i, bool addPermissiveness = true)
    {
      if (i == 0)
        return "\"\"";

      if (addPermissiveness)
        return "\"" + toString(i) + "\np=" + std::to_string(calculatePermissiveness(i)) + "\"";

      return "\"" + toString(i) + "\"";
    }

    std::string toString (long i)
    {
      if (i == 0)
      {
        return "";
      }

      // cache lookup if this node string has been previously generated and
      // generate it if it hasn't.
      auto it = nodeStringCache.find(i);
      if (it == nodeStringCache.end())
      {
        // buffer to write the node string
        std::ostringstream outbuffer;

        bool firstPairPrinted = false;
        // for all symbols in non-binary representation (we use them to access vector s)
        for(long j = 0, jmax = R_all_short.size(); j < jmax; ++j)
        {
          // if j is set in i, add it to the output
          if (i & (1 << j))
          {
            if (firstPairPrinted)
            #if DRAW_NODE_LABELS_IN_LINE
              outbuffer << ",";
            #else
              outbuffer << "\n";
            #endif

            outbuffer << "(" << R_all[ j ].first << "," << R_all[ j ].second << ")";

            firstPairPrinted = true;
          }
        }

        // write it to cache to prevent it from being generated again
        nodeStringCache[i] = outbuffer.str();
      }

      // output cache contents
      return nodeStringCache[i];
    }

    std::string getArgumentsForDot()
    {
      std::ostringstream outbuf;

      // output symbolsLeft
      outbuf << "S1={";
      bool nfirst = false;
      for(auto& s : s1)
      {
        if (nfirst)
          outbuf << ',';
        nfirst = true;

        outbuf << s;
      }
      outbuf << "}, ";

      // output symbolsRight
      outbuf << "S2={";
      nfirst = false;
      for(auto& s : s2)
      {
        if (nfirst)
          outbuf << ',';
        nfirst = true;

        outbuf << s;
      }
      outbuf << "}";

      return outbuf.str();
    }
  };
}

#endif // __RELATION_GRAPH_HPP__

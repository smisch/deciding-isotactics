#ifndef __ALIGNMENT_GRAPH_HPP__
#define __ALIGNMENT_GRAPH_HPP__

// false iff only the pair of nodes that created this
// node should have edges to it. otherwise draw all
// edges (takes longer).
#define DRAW_ALL_EDGES false

// unordered_map (=hash map) performs better than map (red-black tree),
// but messes up the order in which the nodes are printed into dot.
// the performance boost is minimal.
#define USE_PERFORMANCE_TYPE false

// true iff the relation R should be parsed using two maps for the symbol sets
// this performs better when the symbol sets grow, as otherwise the conversion
// uses a linear-time search on the symbol set vectors.
#define PARSE_R_WITH_MAP true

// true iff combining nodes is only allowed if the new nodes gets exactly
// one new symbol, i.e. ab~x + ac~x = abc~x is allowed, but
// ab~x + cd~x = abcd~x is not allowed since abcd~x gained two new symbols.
// abcd~x will be generated from nodes with 3 symbols on the next level.
#define FORCE_ONE_SYMBOL_ADVANCE true

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

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <iostream>
#include <fstream>

#include <memory>         // std::shared_ptr, std::weak_ptr
#include <queue>          // std::queue
#include <map>            // std::map, std::multimap
#include <unordered_map>  // std::unordered_map
#include <chrono>         // for time measurement

#include "json.hpp"
using json = nlohmann::json;

namespace AG
{
  // forward declaration used by following type aliasing
  struct AlignmentNode;

  // owning: contains a shared_ptr that keeps the object alive
  // non-owning: contains a weak_ptr that does not keep the object alive
  //             if all other references are deleted
  using owning_node_t           = std::shared_ptr<AlignmentNode>;
  using owning_node_list_t      = std::MAP_TYPE<long, owning_node_t>;
  using non_owning_node_t       = std::weak_ptr<AlignmentNode>;
  using non_owning_node_list_t  = std::MAP_TYPE<long, non_owning_node_t>;
  using i_t                     = std::vector<non_owning_node_t>::size_type;

  using non_owning_node_queue_t = std::queue<non_owning_node_t>;
  using non_owning_node_map_t   = std::multimap<long, non_owning_node_t>;

  using symbol_set_t            = std::vector<std::string>;
  using binary_relation_t       = std::vector<std::pair<std::string, std::string>>;
  using binary_relation_short_t = std::vector<std::pair<short, short>>;

  using label             = std::string;
  using alignmentGroup    = std::vector<label>;
  using alignmentPair     = std::pair<alignmentGroup, alignmentGroup>;
  using alignment         = std::vector<alignmentPair>;

  struct AlignmentNode
  {
    long id;

    long left;
    long right;

    short kLeft;
    short kRight;

    owning_node_list_t nextNodes;

    /**
     * Constructor calculates id and k's from left and right.
     */
    AlignmentNode(long left, long right)
      : left(left), right(right)
    {
      id = left | right;

      kLeft = __builtin_popcountll(left);
      kRight = __builtin_popcountll(right);
    }

    /**
     * Calculate the complexity k for this alignment pair.
     */
    short getK()
    {
      return kLeft * kRight;
    }
  };

  struct AlignmentGraph
  {
    bool leafsOnly = false;

    symbol_set_t symbolsLeft;
    symbol_set_t symbolsRight;

    short symbolsLeftCount;
    short symbolsRightCount;
    short count;

    binary_relation_t R;
    binary_relation_short_t RShort;
    short k;

    owning_node_t root;
    non_owning_node_list_t nodes;

    non_owning_node_queue_t nextToProccess;

    non_owning_node_map_t leftMap;
    non_owning_node_map_t rightMap;


    std::unordered_map<long, std::string> nodeStringCache;

    // nodeMap is array of k's and node ids containing a pointer to the node
    // nodeMap[k=1..][i=01001..11111]
    std::map<short, std::map<long, std::shared_ptr<AlignmentNode>>> nodeMap;

    /**
     * Constructor for symbolic binary relation R.
     */
    AlignmentGraph(const symbol_set_t & symbolsLeft, const symbol_set_t & symbolsRight,
      const binary_relation_t & R, short k
    ):
      symbolsLeft(symbolsLeft), symbolsRight(symbolsRight), R(R), k(k)
    {
      init();
    }

    /**
     * Constructor for numeric binary relation RShort, used internally when we generate
     * the binary relations and can skip the detour over to the symbols.
     */
    AlignmentGraph(const symbol_set_t & symbolsLeft, const symbol_set_t & symbolsRight,
      const binary_relation_short_t & RShort, short k
    ):
      symbolsLeft(symbolsLeft), symbolsRight(symbolsRight), RShort(RShort), k(k)
    {
      init();
    }

    void init ()
    {
      symbolsLeftCount = symbolsLeft.size();
      symbolsRightCount = symbolsRight.size();
      count = symbolsLeftCount + symbolsRightCount;

      // create empty root node
      root = std::make_shared<AlignmentNode>(0, 0);
      nodes.insert({root->id, root});
    }

    void populate ()
    {
      populateInitial();
      populateRecursive();
    }

    void populateInitial ()
    {
      #if PARSE_R_WITH_MAP
        std::map<std::string, short> symbolsLeftInverse;
        std::map<std::string, short> symbolsRightInverse;

        for (short i = 0; i < symbolsLeftCount; ++i)
          symbolsLeftInverse.insert({symbolsLeft[i], i});

        for (short i = 0; i < symbolsRightCount; ++i)
          symbolsRightInverse.insert({symbolsRight[i], i});
      #endif

      // for all pairs in R, i.e. (a,x)
      for (auto& pair : R)
      {
        #if PARSE_R_WITH_MAP
          long left  = 1 << symbolsLeftInverse[pair.first];
          long right = 1 << (symbolsRightInverse[pair.second] + symbolsLeftCount);
        #else
          long left  = symbolToId(pair.first);
          long right = symbolToId(pair.second);
        #endif

        // create a new node a ~ x and attach it to root.
        auto node = std::make_shared<AlignmentNode>(left, right);

        // implicitly converts to weak_ptr and thereby not giving up ownership
        nodes.insert({node->id, node});

        // insert into k-group (complexity)
        nodeMap[ node->getK() ].insert({node->id, node});

        // associate to root node. this will pass ownership to root.
        root->nextNodes.insert({node->id, node});

        // put new node into next to process queue
        nextToProccess.push(node);
      }
    }

    void populateRecursive ()
    {
      while (! nextToProccess.empty())
      {
        while (! nextToProccess.empty())
        {
          // pop a node from the queue
          auto nodeWeak = nextToProccess.front();
          nextToProccess.pop();

          // lock the weak pointer to get a shared pointer that
          // won't be released while we use it.
          if (auto node = nodeWeak.lock())
          {
            // put it into both maps
            leftMap.insert({node->left, node});
            rightMap.insert({node->right, node});
          }
        }

        // we popped every element from the queue.
        // this is important because we'll fill up the queue with new nodes now.
        // assert(nextToProccess.empty());

        // walk through leftMap to find equal elements like {ab~x} & {ab~y}
        for(auto it = leftMap.begin(); it != leftMap.end(); ++it)
        {
          // get a shared pointer to the current node
          auto currentNode = (*it).second.lock();
          auto it2 = it;

          // for all following nodes that have the same side
          while (++it2 != leftMap.end() && (*it).first == (*it2).first)
          {
            // get a shared pointer to the other node that has a matching side
            // to current node.
            auto otherNode = (*it2).second.lock();

            #if FORCE_ONE_SYMBOL_ADVANCE
              // skip this new node if it's right k increases by more than one
              if (hamming(currentNode->right | otherNode->right) != currentNode->kRight + 1)
                continue;
            #endif

            createNodeIfNotExists(
              currentNode->left,
              currentNode->right | otherNode->right,
              *currentNode,
              *otherNode
            );
          }
        }

        // walk through rightMap to find equal elements like {ab~x} & {cd~x}
        for(auto it = rightMap.begin(); it != rightMap.end(); ++it)
        {
          // get a shared pointer to the current node
          auto currentNode = (*it).second.lock();
          auto it2 = it;

          // for all following nodes that have the same side
          while (++it2 != rightMap.end() && (*it).first == (*it2).first)
          {
            // get a shared pointer to the other node that has a matching side
            // to current node.
            auto otherNode = (*it2).second.lock();

            #if FORCE_ONE_SYMBOL_ADVANCE
              // skip this new node if it's left k increases by more than one
              if (hamming(currentNode->left | otherNode->left) != currentNode->kLeft + 1)
                continue;
            #endif

            createNodeIfNotExists(
              currentNode->left | otherNode->left,
              currentNode->right,
              *currentNode,
              *otherNode
            );
          }
        }

        // done with the maps. clear them for next iteration.
        leftMap.clear();
        rightMap.clear();
      }
    }

    void createNodeIfNotExists(long left, long right, AlignmentNode & currentNode, AlignmentNode & otherNode)
    {
      long newId = left | right;

      if (newId == currentNode.id || newId == otherNode.id)
        return;

      auto nodesIterator = nodes.find(newId);

      // check if this node already exists
      if (nodesIterator == nodes.end())
      {
        // create a new node as a combination of the two
        auto newNode = std::make_shared<AlignmentNode>(
          left, right
        );

        // add to node list
        nodes.insert({newNode->id, newNode});

        // insert into k-group (complexity)
        nodeMap[ newNode->getK() ].insert({newNode->id, newNode});

        // add to next to process next queue
        nextToProccess.push(newNode);

        // create edges from currentNode and otherNode to newNode
        // both current and other node now own new node.
        currentNode.nextNodes.insert({newNode->id, newNode});
        otherNode.nextNodes.insert({newNode->id, newNode});
      }
      #if DRAW_ALL_EDGES
      else
      {
        // create a shared pointer to the node
        auto oldNode = (*nodesIterator).second.lock();

        // create edges from currentNode and otherNode to oldNode
        // both current and other node now own old node.
        currentNode.nextNodes.insert({oldNode->id, oldNode});
        otherNode.nextNodes.insert({oldNode->id, oldNode});
      }
      #endif
    }

    std::string getSortedAlignment(short maxK = -1, bool outputK = false)
    {
      // initialize maxK to the maximum value of k if none is given.
      if (maxK < 0)
        maxK = symbolsLeftCount * symbolsRightCount;

      std::ostringstream outbuf;

      bool first = true;
      // iterate manually so that we can start at the second element.
      // this saves a comparison.
      //
      // workaround: iterate backwards so that the bigger pairs come first
      //             so that iso-decision doesn't fail on valid alignments
      for(auto it = nodeMap.rbegin(); it != nodeMap.rend(); ++it)
      {
        int k = it->first;

        // check if k is too large
        if (k > maxK)
          // since we are looping largest to smallest, we can't break here.
          continue;

        // output k
        if (outputK)
          outbuf << "k = " << (*it).first << std::endl;

        // for all alignment pairs for this k
        for(auto& nPair : (*it).second)
        {
          auto node = nPair.second;

          if (leafsOnly && node->nextNodes.size() > 0)
            continue;

          if (first)
            first = false;
          else
            outbuf << ", ";

          // output alignment pair
          outbuf << toString(node->id);
        }
      }

      return outbuf.str();
    }

    void outputDot (std::string filename = "output.dot")
    {
      std::ostringstream outbuf;

      outbuf << "digraph AlignmentGraph{" << std::endl;

      // over the graph write all the arguments: symbol sets, R and k
      outbuf << "label=\"" << getArgumentsForDot() << "\";\nlabelloc=\"t\";\n";

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
            auto other = pair.second;

            // output edge
            outbuf << "  "
              << toLabelString(current->id, current->getK())
              << " -> "
              << toLabelString(other->id, other->getK())
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

    std::string getArgumentsForDot()
    {
      std::ostringstream outbuf;

      // output symbolsLeft
      outbuf << "S1={";
      bool nfirst = false;
      for(auto& s : symbolsLeft)
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
      for(auto& s : symbolsRight)
      {
        if (nfirst)
          outbuf << ',';
        nfirst = true;

        outbuf << s;
      }
      outbuf << "}, ";

      // output R
      outbuf << "R={";
      nfirst = false;
      for(auto& pair : R)
      {
        if (nfirst)
          outbuf << ',';
        nfirst = true;

        outbuf << '(' << pair.first << ',' << pair.second << ')';
      }
      outbuf << "}, ";

      // output k
      outbuf << "k=" << k;

      return outbuf.str();
    }

    /**
     * Helper function to parse the binary relation R that is passed to this class.
     *
     * It would be easier to just get ids in the first place, but for convenients
     * and readability we'll parse the input once in the beginning.
     *
     * This method receives a symbol from either symbolsLeft or symbolsRight and
     * converts them into their binary representation.
     *
     * Example. Assume symbolsLeft = {a,b,c}, symbolsRight = {x,y}
     * Here are some binary representations:
     *  a - 00 001
     *  b - 00 010
     *  c - 00 100
     *  x - 01 000
     *  y - 10 000
     * Each bit represents one symbol activated, i.e. yx cba. The space indicates
     * the point where the bits represent different symbol sets and are just
     * there for readability.
     */
    long symbolToId (const std::string & s)
    {
      // search symbol s in symbolsLeft first
      auto it = std::find(symbolsLeft.begin(), symbolsLeft.end(), s);

      // found in symbolsLeft ?
      if (it == symbolsLeft.end())
      {
        // no. must be in symbolsRight. get its index
        auto i = std::distance(symbolsRight.begin(), std::find(symbolsRight.begin(), symbolsRight.end(), s));

        // return 01|000 for x, 10|000 for y (shift for number of left symbols)
        return 1 << (i + symbolsLeftCount);
      }
      else
      {
        // yes. it is in symbolsLeft. get its index
        auto i = std::distance(symbolsLeft.begin(), it);

        // return 001 for a, 010 for b, 100 for c
        return 1 << i;
      }
    }

    /**
     * Return the hamming weight for i, which is the number of
     * non-0 characters in its binary representation.
     *
     * Use a fast builtin function.
     */
    int hamming(long long i)
    {
      return __builtin_popcountll(i);
    }

    std::string toLabelString (long i, short k = -1)
    {
      std::ostringstream buf;

      buf << "\"{" << toString(i) << "}";

      if (k > 0)
        buf << " " << k;

      buf << "\"";

      return buf.str();
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

        // for all symbols in non-binary representation (we use them to access vector s)
        for(long j = 0, jmax = count; j < jmax; ++j)
        {
          if (j == symbolsLeftCount)
            outbuffer << "~";

          // if j is set in i, add it to the output
          if (i & (1 << j))
          {
            if (j < symbolsLeftCount)
              outbuffer << symbolsLeft[j];
            else
              outbuffer << symbolsRight[j - symbolsLeftCount];
          }
        }

        // write it to cache to prevent it from being generated again
        nodeStringCache[i] = outbuffer.str();
      }

      // output cache contents
      return nodeStringCache[i];
    }

    /*
    {
      "alignment" : [
        {
          "lhs" : ["a"],
          "rhs" : ["s"]
        },
        {
          "lhs" : ["b","c"],
          "rhs" : ["t"]
        },
        {
          "lhs" : ["d","e"],
          "rhs" : ["u"]
        },
        {
          "lhs" : ["f"],
          "rhs" : ["v"]
        }
      ]
    }
    */
    void outputAlignmentsToFile(short maxK = -1, std::string filename = "alignment.json")
    {
      if (maxK < 0)
        maxK = symbolsLeftCount * symbolsRightCount;

      std::ostringstream outbuf;

      outbuf << "{\"alignment\":";

      nodeStringCache.clear();

      std::vector<std::unordered_map<std::string, std::vector<std::string>>> alignments;

      // iterate manually so that we can start at the second element.
      // this saves a comparison (k > 0).
      //
      // workaround: iterate backwards so that the bigger pairs come first
      //             so that iso-decision doesn't fail on valid alignments
      for(auto it = nodeMap.rbegin(); it != nodeMap.rend(); ++it)
        // for(auto it = std::next(nodeMap.begin()); it != nodeMap.end(); ++it)
      {
        int k = it->first;

        // check if k is too large
        if (k > maxK)
          // since we are looping largest to smallest, we can't break here.
          continue;

        // for all alignment pairs for this k
        for(auto& nPair : (*it).second)
        {
          auto node = nPair.second;

          if (leafsOnly && node->nextNodes.size() > 0)
            continue;

          // output alignment pair
          convertNodeForJson(alignments, node);
        }
      }

      json j_alignments(alignments);

      outbuf << j_alignments << "}" << std::endl;

      // debug output
      // ;// std::cerr << outbuf.str();

      // write stringbuffer to file
      std::ofstream outputf;
      outputf.open(filename);
      outputf << outbuf.str();
      outputf.close();
    }

    /**
     * Convert AlignGraph into an alignment compatible with iso-lib.
     */
    alignment toDecisionAlignment(short maxK = -1)
    {
      alignment returnAlignment;

      if (maxK < 0)
        maxK = symbolsLeftCount * symbolsRightCount;

      nodeStringCache.clear();

      // iterate manually so that we can start at the second element.
      // this saves a comparison (k > 0).
      //
      // workaround: iterate backwards so that the bigger pairs come first
      //             so that iso-decision doesn't fail on valid alignments
      for(auto it = nodeMap.rbegin(); it != nodeMap.rend(); ++it)
        // for(auto it = std::next(nodeMap.begin()); it != nodeMap.end(); ++it)
      {
        int k = it->first;

        // check if k is too large
        if (k > maxK)
          // since we are looping largest to smallest, we can't break here.
          continue;

        // for all alignment pairs for this k
        for(auto& nPair : it->second)
        {
          auto node = nPair.second;

          if (leafsOnly && node->nextNodes.size() > 0)
            continue;

          alignmentGroup leftGroup;

          // push all left-hand side symbols into a vector
          for(long j = 0, jmax = symbolsLeftCount; j < jmax; ++j)
          {
            // if j is set in i, add it to the output
            if (node->left & (1 << j))
              leftGroup.push_back(symbolsLeft[j]);
          }

          alignmentGroup rightGroup;

          // push all right-hand side symbols into a vector
          for(long j = symbolsLeftCount, jmax = count; j < jmax; ++j)
          {
            // if j is set in i, add it to the output
            if (node->right & (1 << j))
              rightGroup.push_back(symbolsRight[j - symbolsLeftCount]);
          }

          returnAlignment.push_back(std::make_pair(leftGroup, rightGroup));
        }
      }

      return returnAlignment;
    }

    void convertNodeForJson(std::vector<std::unordered_map<std::string, std::vector<std::string>>>& alignments,
      std::shared_ptr<AlignmentNode>& node)
    {
      auto lhs = std::make_pair<std::string, std::vector<std::string>>("lhs", {});
      auto rhs = std::make_pair<std::string, std::vector<std::string>>("rhs", {});

      // push all left-hand side symbols into a vector
      for(long j = 0, jmax = symbolsLeftCount; j < jmax; ++j)
      {
        // if j is set in i, add it to the output
        if (node->left & (1 << j))
          lhs.second.push_back(symbolsLeft[j]);
      }

      // push all right-hand side symbols into a vector
      for(long j = symbolsLeftCount, jmax = count; j < jmax; ++j)
      {
        // if j is set in i, add it to the output
        if (node->right & (1 << j))
          rhs.second.push_back(symbolsRight[j - symbolsLeftCount]);
      }

      // put them into a map for format reasons
      std::unordered_map<std::string, std::vector<std::string>> pair{lhs, rhs};
      alignments.push_back(pair);
    }

    int getLargestK ()
    {
      return nodeMap.rbegin()->first;
    }
  };
}

#endif // __ALIGNMENT_GRAPH_HPP__

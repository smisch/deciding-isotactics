#ifndef SpanningTreeGrowIteratorNode_hpp
#define SpanningTreeGrowIteratorNode_hpp

#define DEBUG_OUTPUT false
#define DOT_OUTPUT true

#include <iostream>
#include <memory> // shared_ptr

#include <boost/multiprecision/cpp_int.hpp>

#include "AlignmentGraph.hpp"
#include "IsoSearch.hpp"
#include "BinaryRelation.hpp"
#include "IsoDecisionAdapter.hpp"
#include "Job.hpp"

struct SpanningTreeGrowIteratorNode
{
  Job job;
  
  BinaryRelation binaryRelation;
  
  IsoSearch& isoSearch;
  
  /**
   * Use a pointer so that we don't have to initialize the alignment graph immediately.
   */
  std::shared_ptr<AG::AlignmentGraph> ag;
  
  /**
   * If this node has been found to be iso, this will contain the smallest complexity for which it is iso.
   */
  short smallestK;
  
  /**
   * Keep a reference to job queue, where new jobs are acquired so
   * that we can push new jobs with create children.
   */
  ThreadSafeQueue<Job>& jobQueue;
  
  SpanningTreeGrowIteratorNode(Job job, IsoSearch& isoSearch, ThreadSafeQueue<Job>& jobQueue) : job(job), binaryRelation(job.binaryRelationCode, job.permissiveness, isoSearch), isoSearch(isoSearch), jobQueue(jobQueue)
  {}
  
  void execute()
  {
    // skip relation with code 0, which is the empty set.
    if (binaryRelation.binaryRelationCode > 0)
    {
      createAlignment();
      runIsoDecision();
    }
    
    // queue children
    createChildren();
    
#if DOT_OUTPUT
    {
      std::ostringstream buffer;
      buffer << job.binaryRelationCode;
      if (job.isoStatus == iso)
      {
        buffer << "[label=\"" << binaryRelation.toString() << "\\np=" << job.permissiveness << ",c=" << smallestK << "\" ";
        buffer << "style=filled fillcolor=limegreen ";
      }
      else if (job.isoStatus == notIso)
      {
        buffer << "[label=\"" << binaryRelation.toString() << "\\np=" << job.permissiveness << "\" ";
        buffer << "style=filled fillcolor=tomato ";
      }
      else
      {
        buffer << "[label=\"" << binaryRelation.toString() << "\\np=" << job.permissiveness << "\" ";
        buffer << "style=filled fillcolor=grey ";
      }
      buffer << "]\n";
      std::cerr << buffer.str();
    }
#endif
  }
  
  
  void createAlignment()
  {
    ag = std::make_shared<AG::AlignmentGraph>(isoSearch.s1, isoSearch.s2, binaryRelation.binaryRelation, isoSearch.kMax);
    ag->populateInitial();
    ag->populateRecursive();
  }
  
  /**
   * If the parent node is not iso, run the iso test with the highest possible k to
   * test whether alignment with this R is iso. If this node is iso, find the smallest
   * k for which it is iso.
   *
   * If the parent node is iso, only test if there is a smaller k for which this node is iso.
   * From the parent node being iso we already know that this node is also iso for some k and
   * we know that this node is iso for all k's that the parent node is iso.
   */
  void runIsoDecision()
  {
    // fetch the current best max_pc
    short best_max_pc = isoSearch.best_max_pc;
    
    // in case this node's permissivness is already as big or bigger than
    // our best alignment's max_pc, don't bother testing it.
    if (binaryRelation.permissiveness >= best_max_pc)
    {
#if DEBUG_OUTPUT
      std::ostringstream buffer;
      buffer << "skipping " << binaryRelation.binaryRelationCode << ": permissiveness " << job.permissiveness << " already as large as max_pc " << best_max_pc << "\n";
      std::cerr << buffer.str();
#endif
      
      // report job to be skipped and return
      job.isoStatus = skipped;
      return;
    }
    
    // skip alignments that are not total (do not contain every symbol from both symbol sets)
    if (! binaryRelation.containsAllSymbols())
    {
#if DEBUG_OUTPUT
      std::ostringstream buffer;
      buffer << "skipping " << binaryRelation.binaryRelationCode << ": alignment is not total \n";
      std::cerr << buffer.str();
#endif
      
      // report job to be skipped and return
      job.isoStatus = skipped;
      return;
    }
    
    // only run the iso check with largest possible complexity if
    // the iso status not known.
    // the iso status can be iso if the parent's status is also iso when iterating by growing R (downwards).
    // the iso status can be notIso if the parent's status is also notIso when iterating by shrinking R (upwards).
    if (job.isoStatus == unknown)
    {
      runIsoDecisionForLargestK();
    }
    
    // if this alignment is iso for largestK, we try to find a smaller complexity for which it is also iso
    if (job.isoStatus == iso)
    {
      // for all k values in ag starting with the smallest, run the iso test
      for(auto& pair : ag->nodeMap)
      {
        short k = pair.first;
        
        // don't test ks we already know are iso
        if (k >= smallestK)
          break;
        
        // calculate this alignment's max_pc, because we only want to test it for
        // iso if it could be better than our current best hit
        short max_pc = std::max(binaryRelation.permissiveness, k);
        
        // in case the current alignment is worse than our best alignment, don't
        // test it for iso. also don't test any other k, because they can only get worse
        if (max_pc >= isoSearch.best_max_pc)
          break;
        
        ++isoSearch.stats_iso_tests;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        {
#if DEBUG_OUTPUT
          std::ostringstream buffer;
          buffer
            << "running iso: " << binaryRelation.binaryRelationCode
            << ", p=" << job.permissiveness
            << ", k=" << k
            << "\n";
          std::cerr << buffer.str();
#endif
        }
        
        // run iso-decision
        bool isIso = isIsotactic(isoSearch.m1, isoSearch.m2, ag->toDecisionAlignment(k));
        
        auto stop = std::chrono::high_resolution_clock::now();
        isoSearch.inDecision += std::chrono::duration_cast<std::chrono::microseconds>( stop - start ).count();
        
        // if we found a smaller k for which this node is iso
        if (isIso)
        {
          smallestK = k;
          
          short max_pc = std::max(binaryRelation.permissiveness, smallestK);
          
          if (isoSearch.set_best_max_pc(max_pc, binaryRelation.binaryRelationCode))
          {
#if DEBUG_OUTPUT
            std::ostringstream buffer;
            buffer << "  " << binaryRelation.binaryRelationCode
            << " is iso: " << isIso
            << ", p=" << binaryRelation.permissiveness
            << ", k=" << smallestK
            << ", a=" << ag->getSortedAlignment(smallestK)
            << "\n";
            std::cout << buffer.str();
#endif
          }
          
          // break out of the loop. this node is iso for ks which are bigger than this,
          // so we don't need to test then.
          break;
        }
      }
    }
  } // runIsoDecision
  
  
  void runIsoDecisionForLargestK()
  {
    int largestK = ag->getLargestK();
    
    ++isoSearch.stats_iso_tests;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    {
#if DEBUG_OUTPUT
      std::ostringstream buffer;
      buffer
      << "running iso: " << binaryRelation.binaryRelationCode
      << ", p=" << job.permissiveness
      << ", k=" << largestK
      << "\n";
      std::cerr << buffer.str();
#endif
    }
    
    // run iso-decision
    bool isIso = isIsotactic(isoSearch.m1, isoSearch.m2, ag->toDecisionAlignment(largestK));
    
    auto stop = std::chrono::high_resolution_clock::now();
    isoSearch.inDecision += std::chrono::duration_cast<std::chrono::microseconds>( stop - start ).count();
    
    if (isIso)
    {
      job.isoStatus = iso;
      
      // for now largestK is the smallest K for which we know that node is iso.
      // we will continue to look for smaller Ks next
      smallestK = largestK;
      
      short max_pc = std::max(binaryRelation.permissiveness, smallestK);
      
      // report our current best max_pc. if this returns true than we truely
      // found a new best alignment and report it to std::cout
      if (isoSearch.set_best_max_pc(max_pc, binaryRelation.binaryRelationCode))
      {
#if DEBUG_OUTPUT
        std::ostringstream buffer;
        buffer << binaryRelation.binaryRelationCode
        << " is iso: " << isIso
        << ", p=" << binaryRelation.permissiveness
        << ", k=" << smallestK
        << ", a=" << ag->getSortedAlignment(smallestK)
        << "\n";
        std::cout << buffer.str();
#endif
      }
    }
    else // if (isIso)
    {
      job.isoStatus = notIso;
      
      {
#if DEBUG_OUTPUT
        std::ostringstream buffer;
        buffer << binaryRelation.binaryRelationCode << " not iso\n";
        std::cerr << buffer.str();
#endif
      }
    }
    
  } // runIsoDecisionForLargestK
  
  
  /**
   * @todo: push all children with one call to ThreadSafeQueue::push
   */
  void createChildren()
  {
    // get the current best max_pc, so that we can skip child nodes with
    // permissivness >= max_pc. such child nodes can't produce alignments with
    // better max_pc due to permissivness.
    short max_pc = isoSearch.best_max_pc;
    
    // pass down whether this node has been found to be iso.
    // if this node is iso, then all children must also be iso.
    // if this node is not iso, we don't know whether the children are iso.
    IsoStatus isoStatus = job.isoStatus == iso ? iso : unknown;
    
    short bestComplexity = job.isoStatus == iso ? smallestK : -1;
    
    for (cpp_int j = 1; j < isoSearch.maxBinaryRelationCode; j = j << 1)
    {
      // skip nodes that will be handled by other sibling nodes.
      // this effectively iterates the graph as a spanning tree.
      if (binaryRelation.binaryRelationCode >= j)
        continue;
      
      // next node is current node plus the new tuple
      cpp_int nextRelationBinary = binaryRelation.binaryRelationCode + j;
      
      // calculate next node's permissiveness
      short nextRelationBinaryPermissiveness = binaryRelation.calculatePermissiveness(nextRelationBinary);
      
      // if next node's permissivness is as big or bigger than our current best, don't bother testing it
      if (nextRelationBinaryPermissiveness >= max_pc)
      {
#if DEBUG_OUTPUT
        std::ostringstream buffer;
        buffer << "skipping " << nextRelationBinary << ": child's permissiveness " << nextRelationBinaryPermissiveness << " already as large as max_pc " << max_pc << "\n";
        std::cerr << buffer.str();
#endif
        
        continue;
      }
      
      // create the next job object
      Job nextJob{nextRelationBinary, nextRelationBinaryPermissiveness, isoStatus, bestComplexity};
      
      {
#if DOT_OUTPUT
        std::ostringstream buffer;
        buffer << binaryRelation.binaryRelationCode << " -> " << nextRelationBinary << "\n";
        std::cout << buffer.str();
#endif
      }
      
      {
        // std::lock_guard<std::mutex> dotFileLock(dotFileMutex);
        // dotFile << buffer.str();
      }
      
      // push next job on the job queue
      jobQueue.push(nextJob);
    }
  } // createChildren
  
}; // struct

#endif /* SpanningTreeGrowIteratorNode_hpp */

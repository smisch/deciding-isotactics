#ifndef WorkerPool_hpp
#define WorkerPool_hpp

#include <iostream>
#include <thread>
#include <vector>

#include "ThreadSafeQueue.hpp"
#include "Worker.hpp"
#include "IsoSearch.hpp"

template<class JobT, class SearchSpaceIterator>
class WorkerPool
{
public:
  /**
    * Initialize an unbounded thread safe deque. 
    */
  ThreadSafeQueue<JobT> jobQueue{0};
  
  int workerCount = 1;
  
  std::vector<Worker<JobT, SearchSpaceIterator>> workers;
  
  std::vector<std::thread> workerThreads;
  
  IsoSearch& isoSearch;
  
  WorkerPool(IsoSearch& isoSearch) : isoSearch(isoSearch)
  {
    // detect how many concurrent threads the hardware supports
    workerCount = std::thread::hardware_concurrency();
    
    // in case the detection fails, n will be 0. set it to at least 1
    if (workerCount <= 0) workerCount = 1;
    
    // workerCount = 1;
  }
  
  void start ()
  {
    // create the worker instances
    for (int i = 0; i < workerCount; ++i)
      workers.push_back(Worker<JobT, SearchSpaceIterator>(jobQueue, isoSearch));
    
    // create a thread for each worker and start them
    for (int i = 0; i < workerCount; ++i)
      workerThreads.push_back(std::thread(&Worker<JobT, SearchSpaceIterator>::run, &workers[i]));
  }
  
  void waitUntilFinished ()
  {
    jobQueue.waitUntilFinished();
  }
  
  void stop ()
  {
    for (auto& worker : workers)
    {
      // tell all workers to come to a stop the next time they can
      worker.stop();
    }
    
    // tell queue to notify all blocked threads to stop
    jobQueue.stop();
  }
  
  void join ()
  {
    for (auto& workerThread : workerThreads)
    {
      workerThread.join();
    }
  }
  
private:
};

#endif /* WorkerPool_hpp */

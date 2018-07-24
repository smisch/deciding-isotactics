#ifndef Worker_hpp
#define Worker_hpp

#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <sstream>
#include <mutex>

#include <boost/multiprecision/cpp_int.hpp>

#include "ThreadSafeQueue.hpp"
#include "IsoSearch.hpp"
#include "Job.hpp"

using boost::multiprecision::cpp_int;

extern std::ostringstream dotFile;
extern std::mutex dotFileMutex;

template<class JobT, class SearchSpaceIterator>
class Worker
{
public:
  IsoSearch& isoSearch;
  
  Worker(ThreadSafeQueue<JobT>& jobQueue, IsoSearch& isoSearch) : jobQueue(jobQueue), isoSearch(isoSearch)
  {
    // give each worker a unique id starting with 0
    static int idCounter = 0;
    
    id = idCounter++;
  }
  
  void run() {
    while (! stopped) {
      // get up to 10 jobs.
      std::vector<JobT> jobs = jobQueue.pop(10);
      
      if (jobs.size() == 0)
      {
        // queue sent a poison pill. exit this worker (jump out of while-loop).
        // std::cout << "worker[" + std::to_string(id) + "] stopped\n";
        return;
      }
      
      for (auto job : jobs)
      {
        SearchSpaceIterator node{job, isoSearch, jobQueue};
        
        node.execute();
      }
      
      // signal the queue that this job has been done and the worker is going idle.
      jobQueue.jobDone();
    }
  }
  
  void stop() {
    stopped = true;
  }
  
private:
  /**
    * Keep a reference to job queue, where new jobs are acquired.
    */
  ThreadSafeQueue<JobT>& jobQueue;
  
  /**
    * Setting stopped to true will terminate this worker.
    * The worker will finish processing any job it already acquired.
    */
  bool stopped = false;
  
  int id;
};

#endif /* Worker_hpp */

#ifndef ThreadSafeQueue_hpp
#define ThreadSafeQueue_hpp

#include <iostream>
#include <thread>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm> // std::min

template<class JobT>
class ThreadSafeQueue
{
public:
  ThreadSafeQueue() {workingCount = 0;}
  ThreadSafeQueue(unsigned int maxElements) : maxElements(maxElements) {workingCount = 0;}
  
  /**
    * Does a thread-safe push_back on the internal deque and
    * blocks the pushing thread until there is space in the queue.
    */
  void push(JobT newJob)
  {
    // lock to gain exclusive access to jobQueue
    std::unique_lock<std::mutex> bufferLock(accessMutex);
    
    // wait until we have exclusive access to jobQueue and until there is space in the queue
    if (maxElements > 0)
      cond.wait(bufferLock, [this](){return jobQueue.size() < maxElements;});
    else
      cond.wait(bufferLock, [](){return true;});
    
    // we have exclusive access, push new job
    jobQueue.push_back(newJob);
    
    // release access to jobQueue
    bufferLock.unlock();
    
    // unblock all other waiting threads
    cond.notify_all();
  }
  
  /**
    * Does a thread-safe front() and pop_front() on the internal deque and blocks until
    * there are jobs in the queue, or the queue is told to terminate via stop().
    *
    * Returns a vector of jobs. The vector will be filled with up to numberOfJobsToPop depending on
    * availability, but block until at least one job is available.
    *
    * In case of stop() pop() will return an empty vector, signalling the worker that they should exit.
    */
  std::vector<JobT> pop(int numberOfJobsToPop)
  {
    // lock to gain exclusive access to jobQueue
    std::unique_lock<std::mutex> bufferLock(accessMutex);
    
    // wait until we have exclusive access to jobQueue and there are elements in the queue
    cond.wait(bufferLock, [this](){return jobQueue.size() > 0 || stopped;});
    
    // empty vector jobs
    std::vector<JobT> poppedJobs;
    
    // when stopping return a poison pill to unblock the worker and make him stop as well
    if (stopped)
      // empty vector = poison pill
      return poppedJobs;
    
    // get up to numberOfJobsToPop from the queue, depending on how many are in the queue.
    // since we unblocked there is at least one.
    for(int i = 0, iMax = std::min((size_t)numberOfJobsToPop, jobQueue.size()); i < iMax; ++i)
    {
      // add job to result vector
      poppedJobs.push_back(jobQueue.front());
      
      // remove job from queue
      jobQueue.pop_front();
    }
    
    ++workingCount;
    
    // release access to jobQueue
    bufferLock.unlock();
    
    // unblock all other waiting threads
    cond.notify_all();
    
    // return the jobs in a vector
    return poppedJobs;
  }
  
  int size ()
  {
    // get exclusive access to job queue for this context. lock will be released automatically
    std::lock_guard<std::mutex> bufferLock(accessMutex);
    
    return jobQueue.size();
  }
  
  /**
    * Unblock all waiting threads and tell them that no more jobs will be done.
    */
  void stop ()
  {
    stopped = true;
    
    cond.notify_all();
  }
  
  /**
   * Suspend the calling thread until all jobs in this queue have been done.
   */
  void waitUntilFinished()
  {
    // lock to gain exclusive access to jobQueue
    std::unique_lock<std::mutex> bufferLock(accessMutex);
    
    // wait until we have exclusive access to jobQueue and there are elements in the queue
    cond.wait(bufferLock, [this](){return jobQueue.size() == 0 && workingCount == 0;});
  }
  
  /**
    * Worker signals that it has finished a job it got via pop.
    * If all workers have finished, signal the variable so that threads waiting for
    * the queue to finish all jobs may unblock.
    */
  void jobDone()
  {
    --workingCount;
    
    // unblock threads waiting via waitUntilFinished
    if (workingCount == 0)
       cond.notify_all();
  }
  
  std::atomic<int> workingCount;

private:
  // job queue holds the jobs until the workers gets them
  std::deque<JobT> jobQueue;
  
  // maximum number of elements the queue can hold.
  // add will block the execution until there is space.
  const unsigned int maxElements = 10;
  
  // mutex used internally to gain exclusive access to the queue
  std::mutex accessMutex;
  
  // used for blocking wait until a certain condition is met, for example space in queue to add new elements
  std::condition_variable cond;
  
  bool stopped = false;
  
};


#endif /* ThreadSafeQueue_hpp */

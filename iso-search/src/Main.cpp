#include <thread>
#include <vector>
#include <sstream>
#include <mutex>
#include <chrono>

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/program_options.hpp> // parameter parsing

#include "BinaryRelation.hpp"
#include "ThreadSafeQueue.hpp"
#include "Worker.hpp"
#include "Job.hpp"
#include "WorkerPool.hpp"
#include "IsoSearch.hpp"
#include "SpanningTreeGrowIteratorNode.hpp"
#include "SpanningTreeShrinkIteratorNode.hpp"

using boost::multiprecision::cpp_int;
namespace po = boost::program_options;

std::ostringstream dotFile;
std::mutex dotFileMutex;
cpp_int MAX_BINARY_RELATION_CODE;

auto startTimer()
{
  // std::chrono::high_resolution_clock::time_point
  return std::chrono::high_resolution_clock::now();
}

void measureTime(std::chrono::high_resolution_clock::time_point start)
{
  // time measurement
  {
    auto stop = std::chrono::high_resolution_clock::now();
    double dif = std::chrono::duration_cast<std::chrono::microseconds>( stop - start ).count();
    fprintf(stdout, "%lf microseconds elapsed.\n", dif);
  }
}

int main(int ac, char** av)
{
  std::string m1;
  std::string m2;
  
  symbol_set_t s1;
  symbol_set_t s2;
  
  po::options_description desc("Allowed options");
  
  desc.add_options()
  ("help", "produce help message")
  ("input-file", po::value< std::vector<std::string> >(), "input file")
  ;
  
  po::positional_options_description p;
  p.add("input-file", -1);
  
  po::variables_map vm;
  po::store(po::command_line_parser(ac, av).
            options(desc).positional(p).run(), vm);
  po::notify(vm);
  
  if (vm.count("input-file"))
  {
    auto files = vm["input-file"].as< std::vector<std::string> >();
    
    if (files.size() != 2)
    {
      std::cout << "Usage: " << "iso-search m1.dot m2.dot\n";
      std::cout << desc;
      return 0;
    }
    
    m1 = files[0];
    m2 = files[1];
    
    Graph_t g1 = Graph::parse(m1);
    Graph_t g2 = Graph::parse(m2);
    
    for (auto edge : boost::make_iterator_range(boost::edges(g1)))
    {
      // std::cout << "edge1: " << g1[edge].label << "\n";
      s1.push_back(g1[edge].label);
    }
    
    for (auto edge : boost::make_iterator_range(boost::edges(g2)))
    {
      // std::cout << "edge2: " << g2[edge].label << "\n";
      s2.push_back(g2[edge].label);
    }
  }
  
  IsoSearch isoSearch{s1, s2};
  isoSearch.m1 = m1;
  isoSearch.m2 = m2;
  
  /** /
  // ex1
  symbol_set_t s1{"a", "b"};
  symbol_set_t s2{"s", "t"};
  
  IsoSearch isoSearch{s1, s2};
  
  isoSearch.m1 = "ex1/m1.dot";
  isoSearch.m2 = "ex1/m2.dot";
  /**/
  
  /** /
  // ex2
  symbol_set_t s1{"a", "b"};
  symbol_set_t s2{"s", "t", "u"};
  
  IsoSearch isoSearch{s1, s2};
  
  isoSearch.m1 = "ex2/m1.dot";
  isoSearch.m2 = "ex2/m2.dot";
  /**/
  
  /** /
  // ex6
  symbol_set_t s1{"a", "b", "c", "d"};
  symbol_set_t s2{"s", "t"};
   
  IsoSearch isoSearch{s1, s2};

  isoSearch.m1 = "ex6/m1.dot";
  isoSearch.m2 = "ex6/m2.dot";
  /**/
  
  /** /
  // ex2
  symbol_set_t s1{"a", "b", "c", "d", "e"};
  symbol_set_t s2{"s", "t", "u", "v", "w", "x"};
  
  IsoSearch isoSearch{s1, s2};
  
  isoSearch.m1 = "ex2/m1.dot";
  isoSearch.m2 = "ex2/m2.dot";
  /**/
  
  /*
  cpp_int best = 285249834;
  Job job{best, isoSearch};
  job.createAlignment();
  std::cout << job.binaryRelation.toString()<<"\n";
  std::cout << job.ag->getSortedAlignment(3)<<"\n";
  
  return 0;
  */
  
  // 2^(|S_1| + |S_2|) - 1 ~ 111111...1 (in binary)
  MAX_BINARY_RELATION_CODE = (1 << (s1.size() + s2.size())) - 1;
  
  std::cout << MAX_BINARY_RELATION_CODE;
  
  Job startingJob{0, 0};

  WorkerPool<Job, SpanningTreeGrowIteratorNode> wp(isoSearch);

  wp.start();
  
  auto startTime = startTimer();
  
  wp.jobQueue.push(startingJob);
  
  bool statsRun = true;
  std::thread statsThread([&isoSearch, &statsRun, &wp](){
    auto startTime = std::chrono::high_resolution_clock::now();
    bool once = true;
    
    while (statsRun)
    {
      int currentTestCount = isoSearch.stats_iso_tests;
      
      if (once && currentTestCount > 0)
      {
        startTime = std::chrono::high_resolution_clock::now();
        once = false;
      }
      
      auto currentTime = std::chrono::high_resolution_clock::now();
      
      double diff = std::chrono::duration_cast<std::chrono::seconds>( currentTime - startTime ).count();
      
      if (diff > 0)
      {
        double testsPerSecond = (currentTestCount) / diff;
      
        std::cout << "Tests: " + std::to_string(isoSearch.stats_iso_tests) + ", Tests/s: " + std::to_string(testsPerSecond) +
        " QueueSize: " + std::to_string(wp.jobQueue.size()) + "\n";
      }
      
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
  });

  std::cout << "waiting until finished \n";
  wp.jobQueue.waitUntilFinished();

  measureTime(startTime);
  auto stopTime = std::chrono::high_resolution_clock::now();
  
  std::cout << "stopping everything \n";
  wp.stop();

  std::cout << "joining threads \n";
  wp.join();
  
  statsRun = false;
  statsThread.join();
  
  // std::cout << dotFile.str();
  
  std::cout << "iso tests: " << isoSearch.stats_iso_tests << "\n";
  
  fprintf(stdout, "%lf microseconds spent in iso-decision ", isoSearch.inDecision / wp.workerCount);
  double diff = std::chrono::duration_cast<std::chrono::microseconds>( stopTime - startTime ).count();
  std::cout << "(" << (100 * (isoSearch.inDecision / wp.workerCount) / diff) << "%)\n";

  return 0;
}

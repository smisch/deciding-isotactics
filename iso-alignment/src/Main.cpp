#include <chrono>         // for time measurement
#include "RelationGraph.hpp"


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

int main ()
{
  /*
  INIT_ISO_LIB_LOGGING();
  LOG(INFO) << "My first info log using default logger";

  */
  std::streambuf *coutBackup = std::cout.rdbuf();
  std::streambuf *cerrBackup = std::cerr.rdbuf();

  /*
  LOG(DEBUG) << "My first debug log using default logger";

  CLOG(FATAL, "iso-lib") << "My first debug log using iso-lib logger";
  */

  std::cout.rdbuf(0);
  //;// std::cerr.rdbuf(0);

  auto startTime = startTimer();

  #define EX 3

  // < 1sec
  #if EX == 1
    RG::symbol_set_t s1{"a", "b", "c"};
    RG::symbol_set_t s2{"s", "t"};

    RG::RelationsGraph g{s1, s2};
    g.m1 = "ex1/m1.dot";
    g.m2 = "ex1/m2.dot";
  #endif

  #if EX == 2
    // paper example
    RG::symbol_set_t s1{"a", "b", "c", "d", "e"};
    RG::symbol_set_t s2{"s", "t", "u", "v", "w", "x"};

    RG::RelationsGraph g{s1, s2};
    g.m1 = "ex2/m1.dot";
    g.m2 = "ex2/m2.dot";
  #endif

  // ~300ms
  #if EX == 3
    RG::symbol_set_t s1{"a", "b", "c", "d", "e", "f"};
    RG::symbol_set_t s2{"s", "t", "u", "v"};

    RG::RelationsGraph g{s1, s2};
    g.m1 = "ex3/m1.dot";
    g.m2 = "ex3/m2.dot";
  #endif

  #if EX == 4
    RG::symbol_set_t s1{"a", "b", "c"};
    RG::symbol_set_t s2{"s"};

    RG::RelationsGraph g{s1, s2};
  #endif

  #if EX == 5
    RG::symbol_set_t s1{"a", "b", "c", "d", "e", "f"};
    RG::symbol_set_t s2{"s", "t", "u", "v"};

    RG::RelationsGraph g{s1, s2};
    g.m1 = "ex5/m1.dot";
    g.m2 = "ex5/m2.dot";
  #endif

  // < 1sec
  #if EX == 6
    RG::symbol_set_t s1{"a", "b", "c", "d"};
    RG::symbol_set_t s2{"s", "t"};

    RG::RelationsGraph g{s1, s2};
    g.m1 = "ex6/m1.dot";
    g.m2 = "ex6/m2.dot";
  #endif

  // takes about a minute
  #if EX == 7
    RG::symbol_set_t s1{"a", "b", "c", "d", "e", "f", "g", "h", "i", "j"};
    RG::symbol_set_t s2{"s", "t", "u", "v", "x"};

    RG::RelationsGraph g{s1, s2};
    g.m1 = "ex7/m1.dot";
    g.m2 = "ex7/m2.dot";
  #endif

  // < 1sec
#if EX == 8
  RG::symbol_set_t s1{"a", "b", "c", "d"};
  RG::symbol_set_t s2{"s"};

  RG::RelationsGraph g{s1, s2};
  g.m1 = "ex6/m8.dot";
  g.m2 = "ex6/m8.dot";
#endif


  /*
    RG::binary_relation_t R;

    for(int i = 0, i_max = g.R_all.size(); i < i_max; ++i)
    {
      // if new node includes this pair
      if (29 & (1 << i))
        // push it into R
        R.push_back( g.R_all[i] );
    }

    ;// std::cerr << "R={" << g.toString(29) << "}" << std::endl;
    auto ag = std::make_shared<AG::AlignmentGraph>(g.s1, g.s2, R, 100000000);
    ag->populateInitial();
    ag->populateRecursive();
    ag->leafsOnly = false;
    ;// std::cerr << "Alignment={" << ag->getSortedAlignment() << "}" << std::endl;
    ag->leafsOnly = true;
    ;// std::cerr << "Alignment={" << ag->getSortedAlignment() << "}" << std::endl;
    ;// std::cerr << ag->nodes.size() << std::endl;
    for(auto& node : ag->nodes){
      auto foo = node.second.lock();
      ;// std::cerr << node.first << ":" << foo->nextNodes.size() << std::endl;
    }
    return 1;
*/









  g.populateInitial();
  g.populateRecursive();

  std::cout.rdbuf(coutBackup);

  measureTime(startTime);
  fprintf(stdout, "%lf microseconds spent in iso-decision.\n", g.AGTime);

  std::cout << "stats_R_nodes = " << (1 << g.R_all.size()) << std::endl;
  std::cout << "stats_iso_tests = " << g.stats_iso_tests << std::endl;
  std::cout << "stats_iso_tests_R = " << g.stats_iso_tests_R << std::endl;
  std::cout << "stats_iso_tests_Rk = " << (g.stats_iso_tests - g.stats_iso_tests_R) << std::endl;

  std::cout << "stats_iso_yes = " << g.stats_iso_yes << std::endl;
  std::cout << "stats_iso_no = " << g.stats_iso_no << std::endl;
  std::cout << "stats_iso_segfault = " << g.stats_iso_segfault << std::endl;

  std::cout << "stats_skip_1 = " << g.stats_skip_1 << std::endl;
  std::cout << "stats_skip_2 = " << g.stats_skip_2 << std::endl;
  std::cout << "stats_skip_3 = " << g.stats_skip_3 << std::endl;
  std::cout << "stats_skip_4 = " << g.stats_skip_4 << std::endl;

  std::cout << "best_max_p = " << g.best_permissiveness << std::endl;
  std::cout << "best_max_pc = " << g.best_max_pc << std::endl;

  if (g.nodes.size() < 5000)
  {
    g.outputDot();
    return EXIT_SUCCESS;
  }
  else
  {
    std::cerr << "RGraph has " << g.nodes.size() << " nodes. Do not output." << std::endl;

    // do not chain further in case this program is followed up by dot
    return EXIT_FAILURE;
  }
}

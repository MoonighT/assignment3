#include <stdlib.h>
#include <stdio.h>
#include <iostream>

#include <math.h>
#include <omp.h>
#include "mic.h"

#include "graph.h"
#include "grade.h"

#include "parse_args.h"


/* Apps */
#include "apps/bfs.h"
#include "apps/page_rank.h"
#include "apps/kBFS.h"
#include "apps/graph_decomposition.h"

/* Reference Solutions */
#include "ref/apps/bfs_ref.h"
#include "ref/apps/page_rank_ref.h"
#include "ref/apps/graph_decomposition_ref.h"
#include "ref/apps/kBFS_ref.h"

/* App constants */
#define DecompBeta 2.f
#define PageRankDampening 0.3f
#define PageRankConvergence 0.01f

#define EPSILON 0.001f

// Number of trials to run benchmarks
#define NUM_TRIALS 3

template <class T>
bool compareArrays(Graph graph, T* ref, T* stu)
{
  for (int i = 0; i < graph->num_nodes; i++) {
    if (ref[i] != stu[i]) {
      std::cerr << "*** Results disagree at " << i << " expected " 
        << ref[i] << " found " << stu[i] << std::endl;
      return false;
    }
  }
  return true;
}

template <class T>
bool compareApprox(Graph graph, T* ref, T* stu)
{
  for (int i = 0; i < graph->num_nodes; i++) {
    if (abs(ref[i] - stu[i]) > EPSILON) {
      std::cerr << "*** Results disagree at " << i << " expected " 
        << ref[i] << " found " << stu[i] << std::endl;
      return false;
    }
  }
  return true;
}

template <class T>
bool compareArraysAndDisplay(Graph graph, T* ref, T*stu) 
{
  printf("Visualization of results\n");
  int grid_dim = (int)sqrt(graph->num_nodes);
  for (int j=0; j<grid_dim; j++) {
    for (int i=0; i<grid_dim; i++) {
      printf("%02d ", ref[j*grid_dim + i]);
    }
    printf("\n");
  }

  for (int i = 0; i < graph->num_nodes; i++) {
    if (ref[i] != stu[i]) {
      std::cerr << "*** Results disagree at " << i << " expected "
        << ref[i] << " found " << stu[i] << std::endl;
      return false;
    }
  }
  return true;
}

template <class T>
bool compareArraysAndRadiiEst(Graph graph, T* ref, T* stu) 
{
  bool isCorrect = true;
  for (int i = 0; i < graph->num_nodes; i++) {
    if (ref[i] != stu[i]) {
      std::cerr << "*** Results disagree at " << i << " expected "
        << ref[i] << " found " << stu[i] << std::endl;
	isCorrect = false;
    }
  }
  int stuMaxVal = -1;
  int refMaxVal = -1;
  #pragma omp parallel for schedule(dynamic, 512) reduction(max: stuMaxVal)
  for (int i = 0; i < graph->num_nodes; i++) {
	if (stu[i] > stuMaxVal)
		stuMaxVal = stu[i];
  }
  #pragma omp parallel for schedule(dynamic, 512) reduction(max: refMaxVal)
  for (int i = 0; i < graph->num_nodes; i++) {
        if (ref[i] > refMaxVal)
                refMaxVal = ref[i];
  }
 
  if (refMaxVal != stuMaxVal) {
	std::cerr << "*** Radius estimates differ. Expected: " << refMaxVal << " Got: " << stuMaxVal << std::endl;
	isCorrect = false;
  }   
  return isCorrect;
}

void pageRankWrapper(Graph g, float* solution)
{
  pageRank(g, solution, PageRankDampening, PageRankConvergence);
}

void graphDecompWrapper(Graph g, int* solution) 
{
  decompose(g, solution, DecompBeta); 
}

void pageRankRefWrapper (Graph g, float* solution)
{
  pageRank_ref(g, solution, PageRankDampening, PageRankConvergence);
}

// returns for every node, the cluster id it belongs to 
void graphDecompRefWrapper(Graph g, int* solution) 
{
  decompose_ref(g, solution, DecompBeta); 
}

void timingApp(std::ostream& timing, const char* appName)
{
  std::cout << std::endl;
  std::cout << "Timing results for " << appName << ":" << std::endl;
  sep(std::cout, '=', 75);

  timing << std::endl;
  timing << "Timing results for " << appName << ":" << std::endl;
  sep(timing, '=', 75);
}


int main(int argc, char** argv)
{
  Arguments arguments = parseArgs(argc, argv);

  sep(std::cout);
  std::cout << "Running on device " << arguments.device << std::endl;
  sep(std::cout);

  int max_threads, thread_count;
  bool incremental = true;

  // Get the number of available threads on the MIC or host device
  #pragma offload target(mic: arguments.device)
  max_threads = omp_get_max_threads();

  if (arguments.threads > 0) {
    thread_count = std::min(arguments.threads, max_threads);
    incremental = false;
  } else {
    thread_count = max_threads;
  }

  // Test correctness only.
  int numTrials = NUM_TRIALS;
  if (arguments.correctness) {
    numTrials = 1;
  }

  std::cout << std::endl;
  sep(std::cout);
  std::cout << "Max system threads = " << max_threads << std::endl;
  std::cout << "Running with " << thread_count <<  " threads" << std::endl;
  sep(std::cout);

  std::cout << std::endl;
  std::cout << "Loading graph..." << std::endl;
  Graph graph = load_graph_binary(arguments.graph);

  std::cout << std::endl;
  std::cout << "Graph stats:" << std::endl;
  std::cout << "  Nodes: " << num_nodes(graph) << std::endl;
  std::cout << "  Edges: " << num_edges(graph) << std::endl;

  std::cout << std::endl;
  std::stringstream timing;
  float points = 0;
  float possiblePoints = 0;
  if (arguments.app == BFS || arguments.app == GRADE) {
    /* BFS */
    timingApp(timing, "BFS");
    possiblePoints += 5;
    points += TIME_MIC(bfs_ref, bfs, int)
      (timing, arguments.device, numTrials, thread_count, incremental,
      compareArrays<int>, graph);
  }
  if (arguments.app == PAGERANK || arguments.app == GRADE) {
    /* PageRank */
    timingApp(timing, "PageRank");
    possiblePoints += 5;
    points += TIME_MIC(pageRankRefWrapper, pageRankWrapper, float)
      (timing, arguments.device, numTrials, thread_count, incremental,
      compareApprox<float>, graph);
  }
  if (arguments.app == KBFS || arguments.app == GRADE) {
    /* kBFS */
    timingApp(timing, "kBFS");
    possiblePoints += 5;
    points += TIME_MIC(kBFS_ref, kBFS, int)
      (timing, arguments.device, numTrials, thread_count, incremental,
      compareArraysAndRadiiEst<int>, graph);
  }
  if (arguments.app == DECOMP) {
    /* GraphDecomposition */
    timingApp(timing, "Graph Decomposition");
    TIME_MIC(graphDecompRefWrapper, graphDecompWrapper, int)
      (timing, arguments.device, numTrials, thread_count, incremental,
      compareArraysAndDisplay<int>, graph);
  }

  std::cout << std::endl << std::endl;
  std::cout << "Grading summary:" << std::endl;
  std::cout << timing.str();
  std::cout << std::endl;
  std::cout << "Total Grade: " << points << "/" << possiblePoints << std::endl;
  std::cout << std::endl;

  free_graph(graph);
}
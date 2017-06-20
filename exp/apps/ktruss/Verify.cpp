/** Verification for maximal k-truss -*- C++ -*-
 * @example Verify.cpp
 * @file
 * @section License
 *
 * Galois, a framework to exploit amorphous data-parallelism in irregular
 * programs.
 *
 * Copyright (C) 2013, The University of Texas at Austin. All rights reserved.
 * UNIVERSITY EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES CONCERNING THIS
 * SOFTWARE AND DOCUMENTATION, INCLUDING ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR ANY PARTICULAR PURPOSE, NON-INFRINGEMENT AND WARRANTIES OF
 * PERFORMANCE, AND ANY WARRANTY THAT MIGHT OTHERWISE ARISE FROM COURSE OF
 * DEALING OR USAGE OF TRADE.  NO WARRANTY IS EITHER EXPRESS OR IMPLIED WITH
 * RESPECT TO THE USE OF THE SOFTWARE OR DOCUMENTATION. Under no circumstances
 * shall University be liable for incidental, special, indirect, direct or
 * consequential damages or loss of profits, interruption of business, or
 * related expenses which may arise from use of Software or Documentation,
 * including but not limited to those resulting from defects in Software and/or
 * Documentation, or loss or inaccuracy of data of any kind.
 *
 * @section Description
 *
 * Verify whether an edgelist from an undirected graph is a maximal k-truss
 *
 * @author Yi-Shan Lu <yishanlu@cs.utexas.edu>
 */
#include "Galois/Galois.h"
#include "Galois/Accumulator.h"
#include "Galois/Bag.h"
#include "Galois/Statistic.h"
#include "Galois/Timer.h"
#include "Galois/Graphs/Graph.h"
#include "Galois/Graphs/TypeTraits.h"
#include "llvm/Support/CommandLine.h"
#include "Lonestar/BoilerPlate.h"

#include <iostream>
#include <unordered_set>
#include <algorithm>
#include <fstream>

namespace cll = llvm::cl;

static const char* name = "verify_ktruss";
static const char* desc = "Verify for maximal k-truss";
static const char* url = nullptr;

static cll::opt<std::string> filename(cll::Positional, cll::desc("<input graph>"), cll::Required);
static cll::opt<std::string> trussFile("trussFile", cll::desc("edgelist for the trusses"), cll::Required);
static cll::opt<unsigned int> trussNum("trussNum", cll::desc("verify for maximal trussNum-trusses"), cll::Required);
static cll::opt<unsigned int> ktrussNodes("trussNodes", cll::desc("truss nodes for verification"), cll::init(0));
static cll::opt<unsigned int> ktrussEdges("trussEdges", cll::desc("truss edges for verification"), cll::init(0)); // must be undirected edge count, i.e. counting (n1, n2) and (n2, n1) as 1 edge

static const uint32_t valid = 0x0;
static const uint32_t removed = 0x1;

// edge weight: (# triangles supported << 1) | removal
//   set LSB of an edge weight to indicate the removal of the edge.
//   << 1 to track # triangles an edge supports, 
//   >> 1 when computing edge supports
typedef Galois::Graph::LC_CSR_Graph<void, uint32_t>
  ::template with_numa_alloc<true>::type 
  ::template with_no_lockable<true>::type Graph;
typedef Graph::GraphNode GNode;

typedef std::pair<GNode, GNode> Edge;
typedef Galois::InsertBag<Edge> EdgeVec;

void initialize(Graph& g) {
  g.sortAllEdgesByDst();

  // initializa all edges to removed
  Galois::do_all_local(
    g, 
    [&g] (typename Graph::GraphNode N) { 
      for (auto e: g.edges(N, Galois::MethodFlag::UNPROTECTED)) {
        g.getEdgeData(e) = removed;
      }
    },
    Galois::do_all_steal<true>()
  );
}

// TODO: can we read in edges in parallel?
void readTruss(Graph& g) {
  std::ifstream edgelist(trussFile);
  if (!edgelist.is_open()) {
    std::string errMsg = "Failed to open " + trussFile;
    GALOIS_DIE(errMsg);
  }

  unsigned int n1, n2;
  unsigned int edges = 0;
  std::unordered_set<unsigned int> nodes;
  while (edgelist >> n1 >> n2) {
    auto e = g.findEdgeSortedByDst(n1, n2);
    if(valid == g.getEdgeData(e)) {
      std::cout << "ignoring duplicate edge" << n1 << ", " << n2 << std::endl;
      continue;
    }
    g.getEdgeData(e) = valid;

    e = g.findEdgeSortedByDst(n2, n1);
    if(valid == g.getEdgeData(e)) {
      std::cout << "duplicate edge (rev) " << n2 << ", " << n1 << std::endl;
      continue;
    }
    g.getEdgeData(e) = valid;
    
    edges++;
    nodes.insert(n1);
    nodes.insert(n2);
  }
  
  std::cout << "read " << edges << " unique edges" << std::endl;
  
  if(ktrussEdges && edges != ktrussEdges) {
    std::cerr << "edges read not equal to -trussEdges=" << ktrussEdges << std::endl;
    GALOIS_DIE("Verification error");
  }

  if(ktrussNodes && nodes.size() != ktrussNodes) {
    std::cerr << "nodes read not equal to -trussNodes=" << ktrussNodes << std::endl;
    GALOIS_DIE("Verification error");
  }
}

void printGraph(Graph& g) {
  for (auto n: g) {
    std::cout << "node " << n << std::endl;
    for (auto e: g.edges(n, Galois::MethodFlag::UNPROTECTED)) {
      auto d = g.getEdgeDst(e);
      if (d >= n) continue;
      std::cout << "  edge to " << d << ((g.getEdgeData(e) & removed) ? " removed" : "") << std::endl;
    }
  }
}

std::pair<size_t, size_t> countValidNodesAndEdges(Graph& g) {
  Galois::GAccumulator<size_t> numNodes, numEdges;

  Galois::do_all_local(g, 
    [&g, &numNodes, &numEdges] (GNode n) {
      size_t numN = 0;
      for (auto e: g.edges(n, Galois::MethodFlag::UNPROTECTED)) {
        if (!(g.getEdgeData(e) & removed)) {
          if (g.getEdgeDst(e) > n) {
            numEdges += 1;
          }
          numN = 1;
        }
      }
      numNodes += numN;
    },
    Galois::do_all_steal<true>()
  );

  return std::make_pair(numNodes.reduce(), numEdges.reduce());
}

bool isSupportNoLessThanJ(Graph& g, GNode src, GNode dst, unsigned int j) {
  size_t numValidEqual = 0;
  auto srcI = g.edge_begin(src, Galois::MethodFlag::UNPROTECTED), 
    srcE = g.edge_end(src, Galois::MethodFlag::UNPROTECTED), 
    dstI = g.edge_begin(dst, Galois::MethodFlag::UNPROTECTED), 
    dstE = g.edge_end(dst, Galois::MethodFlag::UNPROTECTED);

  while (true) {
    // find the first valid edge
    while (srcI != srcE && (g.getEdgeData(srcI) & removed)) {
      ++srcI;
    }
    while (dstI != dstE && (g.getEdgeData(dstI) & removed)) {
      ++dstI;
    }

    if (srcI == srcE || dstI == dstE) {
      return numValidEqual >= j;
    }

    // check for intersection
    auto sN = g.getEdgeDst(srcI), dN = g.getEdgeDst(dstI);
    if (sN < dN) {
      ++srcI;
    } else if (dN < sN) {
      ++dstI;
    } else {
      numValidEqual += 1;
      if (numValidEqual >= j) {
        return true;
      }
      ++srcI;
      ++dstI;
    }
  }
  return numValidEqual >= j;
}

int main(int argc, char **argv) {
//  Galois::StatManager statManager;
  LonestarStart(argc, argv, name, desc, url);

  if (2 > trussNum) {
    std::cerr << "trussNum >= 2" << std::endl;
    return -1;
  }

  std::cout << "Verifying maximal " << trussNum << "-truss" << std::endl;
  std::cout << "Truss is computed for " << filename << " and stored in " << trussFile << std::endl;

  Graph g;
  EdgeVec work, shouldBeInvalid, shouldBeValid;

  Galois::Graph::readGraph(g, filename);
  std::cout << "Read " << g.size() << " nodes" << std::endl;

  initialize(g);
  readTruss(g);
//  printGraph(g);

  auto validNum = countValidNodesAndEdges(g);
  std::cout << validNum.first << " valid nodes" << std::endl;
  std::cout << validNum.second << " valid edges" << std::endl;

  // every valid node should have at least trussNum-1 valid neighbors
  // so # valid edges >= smallest # directed edges among valid nodes
  assert((validNum.first * (trussNum-1)) <= validNum.second * 2);

  // symmetry breaking: 
  // consider only edges (i, j) where i < j
  Galois::do_all_local(g, 
    [&g, &work] (GNode n) {
      for (auto e: g.edges(n, Galois::MethodFlag::UNPROTECTED)) {
        auto dst = g.getEdgeDst(e);
        if (dst > n) {
          work.push_back(std::make_pair(n, dst));
        }
      }
    },
    Galois::do_all_steal<true>()
  );

  // pick out the following:
  // 1. valid edges whose support < trussNum-2
  // 2. removed edges whose support >= trussNum-2
  Galois::do_all_local(work, 
    [&g, &shouldBeInvalid, &shouldBeValid] (Edge e) {
       bool isSupportEnough = isSupportNoLessThanJ(g, e.first, e.second, trussNum-2);
       bool isRemoved = g.getEdgeData(g.findEdgeSortedByDst(e.first, e.second)) & 0x1;
       if (!isRemoved && !isSupportEnough) {
         shouldBeInvalid.push_back(e);
       } else if (isRemoved && isSupportEnough) {
         shouldBeValid.push_back(e);
       }
    },
    Galois::do_all_steal<true>()
  );

  auto numShouldBeInvalid = std::distance(shouldBeInvalid.begin(), shouldBeInvalid.end());
  auto numShouldBeValid = std::distance(shouldBeValid.begin(), shouldBeValid.end());
  if (!numShouldBeInvalid && !numShouldBeValid) {
    std::cout << "Verification succeeded" << std::endl;
  } else {
    for (auto e: shouldBeInvalid) {
      std::cerr << "(" << e.first << ", " << e.second << ") should be invalid" << std::endl;
    }
    for (auto e: shouldBeValid) {
      std::cerr << "(" << e.first << ", " << e.second << ") should be valid" << std::endl;
    }
    std::cerr << "Verification failed!" << std::endl;
    return 1;
  }

  return 0;
}

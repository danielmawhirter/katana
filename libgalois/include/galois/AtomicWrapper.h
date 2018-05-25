/**
 * This file belongs to the Galois project, a C++ library for exploiting parallelism.
 * The code is being released under the terms of XYZ License (a copy is located in
 * LICENSE.txt at the top-level directory).
 *
 * Copyright (C) 2018, The University of Texas at Austin. All rights reserved.
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
 */

#ifndef _ATOMIC_WRAPPER_H_
#define _ATOMIC_WRAPPER_H_

#include <atomic>

namespace galois {
  template<class T>
  class CopyableAtomic : public std::atomic<T> {
  public:
    CopyableAtomic() : std::atomic<T>(T{}) {}

    constexpr CopyableAtomic(T desired) : std::atomic<T>(desired) { }

    constexpr CopyableAtomic(const CopyableAtomic<T>& other) 
      : CopyableAtomic(other.load(std::memory_order_relaxed)) { }

    CopyableAtomic& operator=(const CopyableAtomic<T>& other) {
      this->store(other.load(std::memory_order_relaxed), 
                  std::memory_order_relaxed);
      return *this;
    }
  };
}
#endif
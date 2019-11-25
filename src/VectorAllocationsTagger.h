// Copyright (c) 2019 VMware, Inc. All Rights Reserved.
// SPDX-License-Identifier: GPL-2.0

#pragma once
#include <string.h>
#include "Allocations/Graph.h"
#include "Allocations/TagHolder.h"
#include "Allocations/Tagger.h"
#include "VirtualAddressMap.h"

namespace chap {
template <typename Offset>
class VectorAllocationsTagger : public Allocations::Tagger<Offset> {
 public:
  typedef typename Allocations::Graph<Offset> Graph;
  typedef typename Allocations::Finder<Offset> Finder;
  typedef typename Allocations::Tagger<Offset> Tagger;
  typedef typename Allocations::ContiguousImage<Offset> ContiguousImage;
  typedef typename Tagger::Phase Phase;
  typedef typename Finder::AllocationIndex AllocationIndex;
  typedef typename Finder::Allocation Allocation;
  typedef typename VirtualAddressMap<Offset>::Reader Reader;
  typedef typename Allocations::TagHolder<Offset> TagHolder;
  typedef typename TagHolder::TagIndex TagIndex;
  VectorAllocationsTagger(Graph& graph, TagHolder& tagHolder)
      : _graph(graph),
        _tagHolder(tagHolder),
        _finder(graph.GetAllocationFinder()),
        _numAllocations(_finder.NumAllocations()),
        _addressMap(_finder.GetAddressMap()),
        _tagIndex(_tagHolder.RegisterTag("vector body")) {}

  bool TagFromAllocation(const ContiguousImage& /* contiguousImage */,
                         Reader& /* reader */, AllocationIndex index,
                         Phase phase, const Allocation& allocation,
                         bool /* isUnsigned */) {
    /*
     * Note that we cannot assume anything based on the start of a vector
     * body because we don't know the type of the entries.  For this reason
     * we ignore whether the allocation is signed.
     */

    if (_tagHolder.GetTagIndex(index) != 0) {
      /*
       * This was already tagged as something other than a vector body.
       */
      return true;  // We are finished looking at this allocation for this pass.
    }

    switch (phase) {
      case Tagger::QUICK_INITIAL_CHECK:
        // Fast initial check, match must be solid
        return (allocation.Size() < 2 * sizeof(Offset));
        break;
      case Tagger::MEDIUM_CHECK:
        break;
      case Tagger::SLOW_CHECK:
        break;
      case Tagger::WEAK_CHECK:
        /*
         * Recognition of a vector body is rather weak because
         * we don't know much about the body itself and so depend on finding
         * the corresponding vector as a way of finding each vector body. A
         * challenge here is part of a deque can look like a vector body.
         * Rather than build in knowledge of these other possible matches
         * let those more reliable patterns run first during the non-weak
         * phase on the corresponding allocation.
         */

        if (!CheckVectorBodyAnchorIn(index, allocation,
                                     _graph.GetStaticAnchors(index))) {
          CheckVectorBodyAnchorIn(index, allocation,
                                  _graph.GetStackAnchors(index));
        }
        return true;
        break;
    }
    return false;
  }

  bool TagFromReferenced(const ContiguousImage& contiguousImage,
                         Reader& /* reader */, AllocationIndex /* index */,
                         Phase phase, const Allocation& allocation,
                         const AllocationIndex* unresolvedOutgoing) {
    switch (phase) {
      case Tagger::QUICK_INITIAL_CHECK:
        return allocation.Size() < 3 * sizeof(Offset);
        break;
      case Tagger::MEDIUM_CHECK:
        break;
      case Tagger::SLOW_CHECK:
        break;
      case Tagger::WEAK_CHECK:
        /*
         * Recognition of a vector body is rather weak because
         * we don't know much about the body itself and so depend on finding
         * the corresponding vector as a way of finding each vector body. A
         * challenge here is part of a deque can look like a vector body.
         * Rather than build in knowledge of these other possible matches
         * let those more reliable patterns run first during the non-weak
         * phase on the corresponding allocation.
         */

        CheckEmbeddedVectors(contiguousImage, unresolvedOutgoing);
        break;
    }
    return false;
  }

  TagIndex GetTagIndex() const { return _tagIndex; }

 private:
  Graph& _graph;
  TagHolder& _tagHolder;
  const Finder& _finder;
  AllocationIndex _numAllocations;
  const VirtualAddressMap<Offset>& _addressMap;
  TagIndex _tagIndex;

  bool CheckVectorBodyAnchorIn(AllocationIndex bodyIndex,
                               const Allocation& bodyAllocation,
                               const std::vector<Offset>* anchors) {
    Offset bodyAddress = bodyAllocation.Address();
    Offset bodyLimit = bodyAddress + bodyAllocation.Size();
    if (anchors != nullptr) {
      typename VirtualAddressMap<Offset>::Reader dequeReader(_addressMap);
      for (Offset anchor : *anchors) {
        const char* image;
        Offset numBytesFound =
            _addressMap.FindMappedMemoryImage(anchor, &image);

        if (numBytesFound < 3 * sizeof(Offset)) {
          continue;
        }
        Offset* check = (Offset*)(image);
        if (check[0] != bodyAddress) {
          continue;
        }
        Offset useLimit = check[1];
        if (useLimit < bodyAddress) {
          continue;
        }

        Offset capacityLimit = check[2];
        if (capacityLimit < useLimit || capacityLimit > bodyLimit ||
            capacityLimit == bodyAddress) {
          continue;
        }

        // ??? fix here and below: don't allow capacity to be too small.
        // ??? More particularly, we need new logic for weak recognizers
        // ??? because otherwise if a DequeBlock has a lower address
        // ??? than the corresponding DequeMap and both are anchorpoints
        // ??? when we reach the DequeBlock it can get interpreted as
        // ??? being a vector body, based on either the start or end
        // ??? field of the vector.  This could be fixed if we scanned all
        // ??? anchors in order of anchor address rather than in order
        // ??? of target or by doing a sort of double check as done with
        // ??? buckets/first-node for unordered map/set.  A third possibility
        // ??? would be to keep the handling for BBL that is currently in
        // ??? the VectorBodyRecognizer.

        _tagHolder.TagAllocation(bodyIndex, _tagIndex);

        return true;
      }
    }
    return false;
  }

  void CheckEmbeddedVectors(const ContiguousImage& contiguousImage,
                            const AllocationIndex* unresolvedOutgoing) {
    Reader mapReader(_addressMap);

    const Offset* offsetLimit = contiguousImage.OffsetLimit() - 2;
    const Offset* firstOffset = contiguousImage.FirstOffset();

    for (const Offset* check = firstOffset; check < offsetLimit; check++) {
      AllocationIndex bodyIndex = unresolvedOutgoing[check - firstOffset];
      if (bodyIndex == _numAllocations) {
        continue;
      }
      if (_tagHolder.GetTagIndex(bodyIndex) != 0) {
        continue;
      }
      const Allocation* allocation = _finder.AllocationAt(bodyIndex);

      Offset address = allocation->Address();
      Offset bodyLimit = address + allocation->Size();
      if (check[0] != address) {
        continue;
      }

      Offset useLimit = check[1];
      if (useLimit < address) {
        continue;
      }

      Offset capacityLimit = check[2];
      if (capacityLimit < useLimit || capacityLimit > bodyLimit ||
          capacityLimit == address) {
        continue;
      }

      /*
       * Warning: If the variant of malloc has nothing like a size/status
       * word between the allocations we will have trouble parsing
       * BLLl where L is the limit of one allocation and l is the limit of
       * the next, because this could be a full vector body starting at B
       * or an empty vector body starting at L.  Fortunately, with libc
       * malloc we do not yet have this problem.
       */
      _tagHolder.TagAllocation(bodyIndex, _tagIndex);
      check += 2;
    }
  }
};
}  // namespace chap

// Copyright (c) 2017-2019 VMware, Inc. All Rights Reserved.
// SPDX-License-Identifier: GPL-2.0

#pragma once
#include "Allocations/AnchorDirectory.h"
#include "Allocations/Finder.h"
#include "Allocations/Graph.h"
#include "Allocations/SignatureDirectory.h"
#include "Allocations/TagHolder.h"
#include "COWStringAllocationsTagger.h"
#include "DequeAllocationsTagger.h"
#include "ListAllocationsTagger.h"
#include "LongStringAllocationsTagger.h"
#include "MapOrSetAllocationsTagger.h"
#include "ModuleDirectory.h"
#include "OpenSSLAllocationsTagger.h"
#include "PythonAllocationsTagger.h"
#include "ThreadMap.h"
#include "UnfilledImages.h"
#include "UnorderedMapOrSetAllocationsTagger.h"
#include "VectorAllocationsTagger.h"
#include "VirtualAddressMap.h"
#include "VirtualMemoryPartition.h"
namespace chap {
template <typename OffsetType>
class ProcessImage {
 public:
  typedef OffsetType Offset;
  typedef VirtualAddressMap<Offset> AddressMap;
  typedef typename AddressMap::Reader Reader;
  typedef typename AddressMap::NotMapped NotMapped;
  typedef typename VirtualAddressMap<Offset>::RangeAttributes RangeAttributes;
  ProcessImage(const AddressMap &virtualAddressMap,
               const ThreadMap<Offset> &threadMap)
      : STACK("stack"),
        STACK_OVERFLOW_GUARD("stack overflow guard"),
        _virtualAddressMap(virtualAddressMap),
        _threadMap(threadMap),
        _virtualMemoryPartition(virtualAddressMap),
        _moduleDirectory(_virtualMemoryPartition),
        _unfilledImages(virtualAddressMap),
        _allocationFinder(nullptr),
        _allocationTagHolder(nullptr),
        _allocationGraph(nullptr) {
    for (typename ThreadMap<Offset>::const_iterator it = _threadMap.begin();
         it != _threadMap.end(); ++it) {
      if (!_virtualMemoryPartition.ClaimRange(
              it->_stackBase, it->_stackLimit - it->_stackBase, STACK, false)) {
        std::cerr << "Warning: overlap found for stack range "
                  << "for thread " << std::dec << it->_threadNum << ".\n";
      }
    }
  }

  virtual ~ProcessImage() {
    if (_allocationGraph != nullptr) {
      delete _allocationGraph;
    }
    if (_allocationFinder != nullptr) {
      delete _allocationFinder;
    }
    if (_allocationTagHolder != nullptr) {
      delete _allocationTagHolder;
    }
  }

  const AddressMap &GetVirtualAddressMap() const { return _virtualAddressMap; }

  const VirtualMemoryPartition<Offset> &GetVirtualMemoryPartition() const {
    return _virtualMemoryPartition;
  }

  const ThreadMap<Offset> &GetThreadMap() const { return _threadMap; }

  const ModuleDirectory<Offset> &GetModuleDirectory() const {
    return _moduleDirectory;
  }

  const Allocations::SignatureDirectory<Offset> &GetSignatureDirectory() const {
    return _signatureDirectory;
  }

  Allocations::SignatureDirectory<Offset> &GetSignatureDirectory() {
    return _signatureDirectory;
  }

  const Allocations::AnchorDirectory<Offset> &GetAnchorDirectory() const {
    return _anchorDirectory;
  }

  Allocations::AnchorDirectory<Offset> &GetAnchorDirectory() {
    return _anchorDirectory;
  }

  const Allocations::Finder<Offset> *GetAllocationFinder() const {
    return _allocationFinder;
  }

  const Allocations::TagHolder<Offset> *GetAllocationTagHolder() const {
    return _allocationTagHolder;
  }

  Allocations::TagHolder<Offset> *GetAllocationTagHolder() {
    return _allocationTagHolder;
  }

  const Allocations::Graph<Offset> *GetAllocationGraph() const {
    return _allocationGraph;
  }

  const char *STACK;
  const char *STACK_OVERFLOW_GUARD;

 protected:
  const AddressMap &_virtualAddressMap;
  const ThreadMap<OffsetType> &_threadMap;
  VirtualMemoryPartition<Offset> _virtualMemoryPartition;
  ModuleDirectory<Offset> _moduleDirectory;
  UnfilledImages<Offset> _unfilledImages;
  Allocations::Finder<Offset> *_allocationFinder;
  Allocations::TagHolder<Offset> *_allocationTagHolder;
  Allocations::Graph<Offset> *_allocationGraph;
  Allocations::SignatureDirectory<Offset> _signatureDirectory;
  Allocations::AnchorDirectory<Offset> _anchorDirectory;

  /*
   * Pre-tag all allocations.  This should be done just once, at the end
   * of the constructor for the derived class.
   */
  void TagAllocations() {
    _allocationTagHolder =
        new Allocations::TagHolder<Offset>(_allocationFinder->NumAllocations());

    Allocations::TaggerRunner<Offset> runner(
        *_allocationGraph, *_allocationTagHolder, _signatureDirectory);

    runner.RegisterTagger(new UnorderedMapOrSetAllocationsTagger<Offset>(
        *(_allocationGraph), *(_allocationTagHolder)));

    runner.RegisterTagger(new MapOrSetAllocationsTagger<Offset>(
        *(_allocationGraph), *(_allocationTagHolder)));

    runner.RegisterTagger(new DequeAllocationsTagger<Offset>(
        *(_allocationGraph), *(_allocationTagHolder)));

    runner.RegisterTagger(new ListAllocationsTagger<Offset>(
        *(_allocationGraph), *(_allocationTagHolder)));

    runner.RegisterTagger(new LongStringAllocationsTagger<Offset>(
        *(_allocationGraph), *(_allocationTagHolder), _moduleDirectory));

    runner.RegisterTagger(new VectorAllocationsTagger<Offset>(
        *(_allocationGraph), *(_allocationTagHolder)));

    runner.RegisterTagger(new COWStringAllocationsTagger<Offset>(
        *(_allocationGraph), *(_allocationTagHolder), _moduleDirectory));

    runner.RegisterTagger(new OpenSSLAllocationsTagger<Offset>(
        *(_allocationTagHolder), _moduleDirectory, _virtualAddressMap));

    runner.RegisterTagger(new PythonAllocationsTagger<Offset>(
        *(_allocationTagHolder), _moduleDirectory));

    runner.ResolveAllAllocationTags();
  }
};
}  // namespace chap

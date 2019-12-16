// Copyright (c) 2019 VMware, Inc. All Rights Reserved.
// SPDX-License-Identifier: GPL-2.0

#pragma once
#include <string.h>
#include "Allocations/PatternDescriber.h"
#include "ProcessImage.h"

namespace chap {
template <typename Offset>
class LongStringDescriber : public Allocations::PatternDescriber<Offset> {
 public:
  typedef typename Allocations::Finder<Offset>::AllocationIndex AllocationIndex;
  typedef typename Allocations::PatternDescriber<Offset> Base;
  typedef typename Allocations::Finder<Offset>::Allocation Allocation;
  LongStringDescriber(const ProcessImage<Offset>& processImage)
      : Allocations::PatternDescriber<Offset>(processImage, "LongString") {}

  /*
   * Describe the specified allocation, which has already been pre-tagged
   * as matching the pattern.
   */
  virtual void Describe(Commands::Context& context, AllocationIndex /* index */,
                        const Allocation& allocation, bool explain) const {
    Commands::Output& output = context.GetOutput();
    const char* allocationImage;
    Offset numBytesFound = Base::_addressMap.FindMappedMemoryImage(
        allocation.Address(), &allocationImage);
    if (numBytesFound >= allocation.Size()) {
      Offset stringLength = (Offset)(strlen(allocationImage));
      output << "This allocation matches pattern LongString.\n";
      output << "The string has 0x" << std::hex << stringLength << " bytes, ";
      if (explain || stringLength < 77) {
        output << "containing\n\"" << allocationImage << "\".\n";
      } else {
        output << "starting with\n\"" << std::string(allocationImage, 77)
               << "\".\n";
      }
    }
    if (explain) {
      /* TODO: Identify the owner of the string. */
    }
  }
};
}  // namespace chap

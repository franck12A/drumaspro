// Global allocation counter used by the "audio path does not allocate" test.
//
// AllocationCounter.cpp replaces the global operator new/delete of the test
// executable and counts every call. It is linked into the test binary (not into
// a library) so the replacement cannot be dropped by the linker.
#pragma once

#include <cstddef>

namespace drumas::test {

/// Number of global allocation calls since the process started (or since the
/// last resetAllocationCount()).
[[nodiscard]] std::size_t allocationCount() noexcept;

/// Number of matching deallocation calls.
[[nodiscard]] std::size_t deallocationCount() noexcept;

void resetAllocationCount() noexcept;

}  // namespace drumas::test

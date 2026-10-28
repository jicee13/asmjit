// This file is part of AsmJit project <https://asmjit.com>
//
// See <asmjit/core.h> or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <asmjit/core/api_build_test_p.h>
#if defined(ASMJIT_TEST) && !defined(ASMJIT_NO_JIT)

#include <asmjit/axl/support_p.h>
#include <asmjit/core/virt_mem.h>

#if defined(__linux__)
  #include <sys/mman.h>
#endif

ASMJIT_BEGIN_NAMESPACE

TEST_CASE(core_virt_mem) {
  VirtMem::Info vm_info = VirtMem::info();

  TEST_LOG("VirtMem::info():");
  TEST_LOG("  page_size: %zu", size_t(vm_info.page_size));
  TEST_LOG("  page_granularity: %zu", size_t(vm_info.page_granularity));

  TEST_LOG("VirtMem::large_page_size():");
  TEST_LOG("  large_page_size: %zu", size_t(VirtMem::large_page_size()));

  VirtMem::HardenedRuntimeInfo hardened_rt_info = VirtMem::hardened_runtime_info();
  VirtMem::HardenedRuntimeFlags hardened_rt_flags = hardened_rt_info.flags;

  TEST_LOG("VirtMem::hardened_runtime_info():");
  TEST_LOG("  flags:");
  TEST_LOG("    kEnabled: %s"    , axl::test(hardened_rt_flags, VirtMem::HardenedRuntimeFlags::kEnabled    ) ? "true" : "false");
  TEST_LOG("    kMapJit: %s"     , axl::test(hardened_rt_flags, VirtMem::HardenedRuntimeFlags::kMapJit     ) ? "true" : "false");
  TEST_LOG("    kDualMapping: %s", axl::test(hardened_rt_flags, VirtMem::HardenedRuntimeFlags::kDualMapping) ? "true" : "false");
}

#if defined(__linux__) && defined(MADV_HUGEPAGE)
TEST_CASE(core_virt_mem_aligned) {
  const size_t page_size = VirtMem::info().page_size;
  void* probe;
  EXPECT_OK(VirtMem::alloc(&probe, page_size, VirtMem::MemoryFlags::kAccessRW));
  // Kernels without THP can reject advice even when the build headers support it.
  bool advice_supported = madvise(probe, page_size, MADV_HUGEPAGE) == 0;
  EXPECT_OK(VirtMem::release(probe, page_size));

  for (size_t alignment : {page_size, page_size * 4, size_t(2u * 1024u * 1024u)}) {
    for (VirtMem::MemoryFlags advice : {VirtMem::MemoryFlags::kNone, VirtMem::MemoryFlags::kMAdviseLargePages}) {
      for (size_t size : {alignment, alignment * 2}) {
        void* p = nullptr;
        Error err = VirtMem::alloc_aligned(&p, size, alignment, VirtMem::MemoryFlags::kAccessRW | advice);
        if (advice != VirtMem::MemoryFlags::kNone && !advice_supported) {
          EXPECT_NE(err, Error::kOk);
          EXPECT_NULL(p);
          continue;
        }
        EXPECT_OK(err);
        EXPECT_TRUE(axl::is_aligned(uintptr_t(p), alignment));
        memset(p, 0xA5, size);
        EXPECT_EQ(static_cast<uint8_t*>(p)[size - 1], uint8_t(0xA5));
        EXPECT_OK(VirtMem::release(p, size));
      }
    }
  }

  void* p = nullptr;
  EXPECT_EQ(VirtMem::alloc_aligned(&p, 0, page_size, VirtMem::MemoryFlags::kAccessRW), Error::kInvalidArgument);
  EXPECT_NULL(p);
  EXPECT_EQ(VirtMem::alloc_aligned(&p, page_size - 1, page_size, VirtMem::MemoryFlags::kAccessRW), Error::kInvalidArgument);
  EXPECT_NULL(p);
  EXPECT_EQ(VirtMem::alloc_aligned(&p, page_size, page_size * 3, VirtMem::MemoryFlags::kAccessRW), Error::kInvalidArgument);
  EXPECT_NULL(p);
  EXPECT_EQ(VirtMem::alloc_aligned(&p, page_size, 1, VirtMem::MemoryFlags::kAccessRW), Error::kInvalidArgument);
  EXPECT_NULL(p);
  EXPECT_EQ(VirtMem::alloc_aligned(&p, page_size, page_size,
                                 VirtMem::MemoryFlags::kAccessRW | VirtMem::MemoryFlags::kMMapLargePages),
            Error::kInvalidArgument);
  EXPECT_NULL(p);
  EXPECT_EQ(VirtMem::alloc_aligned(&p, page_size, page_size,
                                 VirtMem::MemoryFlags::kAccessRW | VirtMem::MemoryFlags::kMapShared),
            Error::kInvalidArgument);
  EXPECT_NULL(p);
  EXPECT_EQ(VirtMem::alloc_aligned(&p, page_size, page_size * 2, VirtMem::MemoryFlags::kAccessRW),
            Error::kInvalidArgument);
  EXPECT_NULL(p);
}
#endif

ASMJIT_END_NAMESPACE

#endif // ASMJIT_TEST && !ASMJIT_NO_JIT

#pragma once

#include "fixed_types.h"
#include <vector>
#include "rangelb.h"

namespace ParametricDramDirectoryMSI
{
    class PhysicalMemoryAllocator
    {
        public:
            PhysicalMemoryAllocator();
            
            virtual void init();
            virtual UInt64 allocate(UInt64) = 0;
            virtual void deallocate(UInt64) = 0;
            virtual std::vector<Range> allocate_eager_paging(UInt64) = 0;

            virtual void print_allocator() = 0;

        protected:
            UInt64 m_memory_size; //in mbytes
    };
}
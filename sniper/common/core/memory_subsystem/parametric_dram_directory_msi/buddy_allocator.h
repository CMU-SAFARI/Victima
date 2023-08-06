#pragma once
#include "physical_memory_allocator.h"
#include "rangelb.h"
#include <vector>
#include <map>

namespace ParametricDramDirectoryMSI
{
    class BuddyAllocator: public PhysicalMemoryAllocator
    {
        public:
            BuddyAllocator(int max_order);
            void init();
            UInt64 allocate(UInt64);
            std::vector<Range> allocate_eager_paging(UInt64);
            void deallocate(UInt64);

            void print_allocator();

            UInt64 getFreePages() const;
            UInt64 getTotalPages() const;

        class BuddyInitializer
        {
            public:
                BuddyInitializer(BuddyAllocator *allocator);
                void perform_init_file(String input_file_name);
                void perform_init_random(double target_fragmentation, double target_memory_percent);
            private:
                BuddyAllocator *m_allocator;

        };

        protected:
            int m_max_order;
            UInt64 m_total_pages;
            UInt64 m_free_pages;
            std::vector<std::vector<std::pair<UInt64,UInt64>>> free_list;
            std::map<UInt64, UInt64> allocated_map;

            double getFragmentationPercentage() const; 


    };
}
#ifndef PTWC_H
#define PTWC_H

#include "fixed_types.h"
#include "./cuckoo/elastic_cuckoo_table.h"
#include "cache.h"
#include "pagetable_walker.h"
#include <unordered_map>
#include "cache_cntlr.h"
#include <random>
#include "pwc.h"
#include "memory_manager.h"
#include "utopia_cache_template.h"

namespace ParametricDramDirectoryMSI
{
   class PageTableWalkerCuckoo: public PageTableWalker
   {
        private:
            elasticCuckooTable_t elasticCuckooHT_4KB;
            elasticCuckooTable_t elasticCuckooHT_2MB;
            IntPtr currentTableAddr = 0x1000000;
            IntPtr migrateTableAddr = 0x900000000;
            int allocation_percentage_2MB;
            UInt64 cuckoo_faults;
            UInt64 cuckoo_hits;
            SubsecondTime cuckoo_latency;
            void accessTable(IntPtr address);

        public:

            PageTableWalkerCuckoo(int core_id, int _page_size, ShmemPerfModel* _m_shmem_perf_model, PWC* _pwc, 
                            bool _page_walk_cache_enabled, int d, char* hash_func, int size,
                            float rehash_threshold, uint8_t scale,
                                      uint8_t swaps, uint8_t priority);
            int init_walk_functional(IntPtr address);
            bool isPageFault(IntPtr address);
            SubsecondTime init_walk(IntPtr eip, IntPtr address, UtopiaCache* shadow_cache, CacheCntlr *cache, Core::lock_signal_t lock_signal, Byte* _data_buf, UInt32 _data_length, bool modeled, bool count) ;

   };
}
   
#endif


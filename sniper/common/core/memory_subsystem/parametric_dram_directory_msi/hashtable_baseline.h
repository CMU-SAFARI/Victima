#include "page_table_walker_types.h"
#include "cache_cntlr.h"
#include "subsecond_time.h"
#include "fixed_types.h"
#include "city.h"
#include "utopia_cache_template.h"



#include <stdint.h>
namespace ParametricDramDirectoryMSI{
    class HashTablePTW:public PageTableWalker{
        struct entry{
            bool empty;
            long vpn;
            int *data;
            entry * next_entry;
        };
        entry* small_page_table;
        entry* large_page_table;
        Core* core;
        CacheCntlr *cache;
        ShmemPerfModel* m_shmem_perf_model;
        int table_size_in_bits;
        int small_page_size_in_bits;
        int large_page_size_in_bits;
        int small_page_percentage;
        

        //stats for page table walk
        UInt64 page_table_walks;
        UInt64 pagefaults_2MB;
        UInt64 pagefaults_4KB;
        UInt64 chain_per_entry_4KB;
        UInt64 chain_per_entry_2MB;
        UInt64 num_accesses_4KB;
        UInt64 num_accesses_2MB;
        
        SubsecondTime total_latency_4KB;
        SubsecondTime total_latency_2MB;
        SubsecondTime total_latency_page_fault;

        public:

            int hash_function(IntPtr address);
            HashTablePTW(int table_size_in_bits,int small_page_size_in_bits,int big_page_size_in_bits,int _small_page_percentage,Core* _core,ShmemPerfModel* _m_shmem_perf_model,PWC* pwc, bool pwc_enabled);
            SubsecondTime init_walk(IntPtr eip, IntPtr address, UtopiaCache* shadow_cache, CacheCntlr *cache, Core::lock_signal_t lock_signal, Byte* _data_buf, UInt32 _data_length, bool modeled, bool count);
            int init_walk_functional(IntPtr address);
            int handle_page_fault(IntPtr address, int hash_function_result_4KB, int hash_function_result_2MB);
            bool isPageFault(IntPtr address);
            SubsecondTime access_cache(IntPtr eip, IntPtr address, UtopiaCache* shadow_cache,
            CacheCntlr *cache, Core::lock_signal_t lock_signal, Byte* _data_buf, UInt32 _data_length, bool modeled, bool count);
    };
}
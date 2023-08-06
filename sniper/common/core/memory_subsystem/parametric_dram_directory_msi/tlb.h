#ifndef TLB_H
#define TLB_H

#include "fixed_types.h"
#include "cache.h"
#include <util.h>
#include <unordered_map>
#include "pagetable_walker.h"
#include "lock.h"
#include "utopia.h"
#include "hash_map_set.h"
#include <vector>

namespace ParametricDramDirectoryMSI
{
  class TLB
  {
  private:
    static  UInt32 SIM_PAGE_SHIFT; // 4KB
    static  IntPtr SIM_PAGE_SIZE;
    static  IntPtr SIM_PAGE_MASK;
    static ParametricDramDirectoryMSI::MemoryManager* m_manager;

//    static bool tlb_caching;
    bool tlb_caching_l1_cache;
    bool tlb_caching_l2_cache;
    bool victima_enabled;
    bool victimize_on_ptw;
    bool potm_enabled;
    
    PageTableWalker *ptw;
    bool ptw_enabled;


    UInt32 m_size;
    UInt32 m_associativity;
    UInt32 m_num_entries;
    
    Cache m_cache;

    UInt64 m_access, m_miss, m_eviction;
    UInt64 l1_tlb_cache_hit, l2_tlb_cache_hit, nuca_tlb_cache_hit, victima_alloc_on_eviction, victima_alloc_on_ptw;
    SubsecondTime total_potm_latency;

    ShmemPerfModel* m_shmem_perf_model;
    bool m_translation_enabled;
    const UInt64 util_update_thr = 10000;
    UInt64 m_last_util_update = 0;
    core_id_t m_core_id;
    Utopia *m_utopia;
    UTR *m_utr_4KB;
    UTR *m_utr_2MB;


    bool track_L2TLBmiss;
    bool track_Accesses;


    char* software_tlb; //POTM-related parameter
    SubsecondTime final_potm_latency; //POTM-related parameter


    TLB *m_next_level;
    int level;
    bool is_dtlb;
    bool is_itlb;
    bool is_stlb;
    bool is_nested;
    bool is_potm;

    UInt64 *tlb_address_access;
    int m_tlb_address_saved_cnt;

    struct features{

        bool page_size;
        int dram_accesses;

        int page_table_walks;
        int ll_pwc_hit;

        int l2_tlb_hit;
        int l2_tlb_miss;

        int accesses;
        int l1_tlb_hit;
        SubsecondTime ptw_latency;
    };

    typedef std::unordered_map<IntPtr,struct features> tlb_features;

    tlb_features m_page_features;

    struct pair_hash
    {
      template <class T1, class T2>
      std::size_t operator () (const std::pair<T1,T2> &p) const
      {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);
        return h1 ^ h2;
      }
    };



    /* -----  @kanellok UTOPIA-related parameters ------ */

    bool utopia_enabled;



    std::unordered_map<UInt32, UInt32> tlb_pressure_cnt;
    std::unordered_map<UInt64, UInt64> pte_pressure_cnt;




  public:
    enum where_t
      {
        L1 = 0,
        L2,
        POTM,
        L1_CACHE,
        L2_CACHE,
        NUCA_CACHE,
        UTR_HIT,
        MISS
      };

    
    TLB(String name, String cfgname, core_id_t core_id, ShmemPerfModel* m_shmem_perf_model, UInt32 num_entries,UInt32 pagesize, UInt32 associativity, TLB *next_level, bool _utopia_enableds,bool track_misses, bool track_accesses, int* page_size_list, int page_sizes, PageTableWalker* ptw);
    TLB::where_t lookup(IntPtr address, SubsecondTime now, bool allocate_on_miss, int level , bool model_count, Core::lock_signal_t lock);
    void allocate(IntPtr address, SubsecondTime now,int level, Core::lock_signal_t locksss);
    void setMemManager(ParametricDramDirectoryMSI::MemoryManager* _m_manager){ TLB::m_manager = _m_manager;}
    std::unordered_map<IntPtr,UInt64> access_per_page;
    std::unordered_map<IntPtr,UInt64> L2TLB_miss_per_page;
    std::unordered_map<IntPtr,UInt64> getVPNaccesses(){ return access_per_page; };
    std::unordered_map<IntPtr,UInt64> getL2TLBmisses(){ return L2TLB_miss_per_page; };
		ShmemPerfModel* getShmemPerfModel() { return m_shmem_perf_model; }

    void setPOTMDataStructure(int num_entries){ 
      std::cout << "[VM] POTM: Setting up POTM data structure as L3 TLB with " << num_entries << " entries" << std::endl;
      software_tlb = (char*) malloc(num_entries*(8+8)); // 8 bytes for VPN and 8 bytes for PPN
    }
    SubsecondTime getPOTMlookupTime(){ return final_potm_latency; }

   
    static const UInt64 ADDRESS_REQUEST_VEC_MAX = 10000000;
 
  };
}

#endif // TLB_H

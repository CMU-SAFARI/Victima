#pragma once

#include "memory_manager_base.h"
#include "cache_base.h"
#include "cache_cntlr.h"
#include "../pr_l1_pr_l2_dram_directory_msi/dram_directory_cntlr.h"
#include "../pr_l1_pr_l2_dram_directory_msi/dram_cntlr.h"
#include "address_home_lookup.h"
#include "../pr_l1_pr_l2_dram_directory_msi/shmem_msg.h"
#include "mem_component.h"
#include "semaphore.h"
#include "fixed_types.h"
#include "shmem_perf_model.h"
#include "shared_cache_block_info.h"
#include "subsecond_time.h"
#include "pagetable_walker.h"
#include "pagetable_walker_radix.h"
#include "tlb.h"
#include "rangelb.h"
#include "pwc.h"
#include "ulb.h"
#include "utopia_cache_template.h"
#include "modrian_memory.h"
#include "pagetable_walker_xmem.h"
#include "contention_model.h"
#include "va_area_reader.h"


#include <map>

class DramCache;
class ShmemPerf;

namespace ParametricDramDirectoryMSI
{
   class TLB;

   typedef std::pair<core_id_t, MemComponent::component_t> CoreComponentType;
   typedef std::map<CoreComponentType, CacheCntlr*> CacheCntlrMap;

   enum TranslationHitWhere{
            UTR_HIT = 0,
            UTR_MISS,
            ULB_HIT,
            TLB_HIT_L1,
            TLB_HIT_L2,
            TLB_POTM_HIT,
            TLB_HIT_CACHE_L1,
            TLB_HIT_CACHE_L2,
            TLB_HIT_CACHE_NUCA,
            TLB_MISS
         };

   struct TranslationResultStruct{
            TranslationHitWhere hitwhere;
            SubsecondTime latency;
         }; 
   
   typedef struct TranslationResultStruct TranslationResult;


   class MemoryManager : public MemoryManagerBase
   {
      private:
         CacheCntlr* m_cache_cntlrs[MemComponent::LAST_LEVEL_CACHE + 1];
         NucaCache* m_nuca_cache;
         DramCache* m_dram_cache;
         PrL1PrL2DramDirectoryMSI::DramDirectoryCntlr* m_dram_directory_cntlr;
         PrL1PrL2DramDirectoryMSI::DramCntlr* m_dram_cntlr;
         AddressHomeLookup* m_tag_directory_home_lookup;
         AddressHomeLookup* m_dram_controller_home_lookup;
         
         
         TLB *m_itlb, *m_dtlb, *m_stlb, *m_potm_tlb;

         RLB *m_rlb;


	      UInt64 m_system_page_size;

         PageTableWalker *ptw;
         ModrianMemoryManager * mmm;
         XMemManager * xmem_manager;
         ComponentLatency m_tlb_miss_penalty;
         bool m_tlb_miss_parallel;
         
         ComponentLatency m_tlb_l1_access_penalty;
         ComponentLatency m_tlb_l2_access_penalty;

         ComponentLatency m_tlb_l1_miss_penalty;
         ComponentLatency m_tlb_l2_miss_penalty;
         ComponentLatency potm_latency;
         ComponentLatency migration_latency;

         bool m_utopia_enabled;
         bool m_ulb_enabled;
         bool m_utopia_permission_enabled;
         bool m_utopia_tag_enabled;
         bool m_potm_enabled;
         bool m_virtualized;

         bool m_parallel_walk;

         
         UtopiaCache *utopia_perm_cache;
         UtopiaCache *utopia_tag_cache;
         ULB* ulb;
         std::unordered_map<IntPtr, SubsecondTime> migration_map; // Track migrated pages
         UtopiaCache* shadow_cache;
         bool shadow_cache_enabled;

         UInt32 shadow_cache_size;
         UInt32 shadow_cache_associativity;
         ComponentLatency shadow_cache_hit_latency;
         ComponentLatency shadow_cache_miss_latency;

         int current_nuca_stamp;
         

         struct {
            
           UInt64 tlb_hit;
           UInt64 utr_hit;
           UInt64 tlb_hit_l1;
           UInt64 tlb_hit_l2;
           UInt64 tlb_and_utr_miss;
         
           SubsecondTime tlb_hit_latency;
           SubsecondTime tlb_hit_l1_latency;
           SubsecondTime tlb_hit_l2_latency;

           SubsecondTime utr_hit_latency;
           SubsecondTime tlb_and_utr_miss_latency;
           SubsecondTime total_latency;

           UInt64 llc_miss_l1tlb_hit;
           UInt64 llc_miss_l2tlb_hit;
           UInt64 llc_miss_l2tlb_miss;

           UInt64 llc_hit_l1tlb_hit;
           UInt64 llc_hit_l2tlb_hit;
           UInt64 llc_hit_l2tlb_miss;


           SubsecondTime victima_latency;
           SubsecondTime l1c_hit_tlb_latency;
           SubsecondTime l2c_hit_tlb_latency;
           SubsecondTime nucac_hit_tlb_latency;
           
           UInt64 l1c_hit_tlb;
           UInt64 l2c_hit_tlb;
           UInt64 nucac_hit_tlb;

           SubsecondTime ptw_contention;

           SubsecondTime migrations_affected_latency;
           UInt64 migrations_affected_request;


         } translation_stats;
         
         struct {
            UInt64 metadata_hit[HitWhere::NUM_HITWHERES];
         } metadata_stats;


         PWC *pwc;
         bool m_pwc_enabled;
         UInt32 pwc_L4_assoc,pwc_L4_size;
         UInt32 pwc_L3_assoc,pwc_L3_size;
         UInt32 pwc_L2_assoc,pwc_L2_size;
         bool tlb_caching;
         ComponentLatency pwc_access_latency;
         ComponentLatency pwc_miss_latency;

         int parallel_ptw;
         ContentionModel *ptw_srs; // Enabling parallel PTWs

         core_id_t m_core_id_master;

         bool m_tag_directory_present;
         bool m_dram_cntlr_present;

         bool hash_dont_cache_enabled;
         bool hash_baseline_enabled;
         bool radix_enabled;
         bool xmem_enabled;
         bool modrian_memory_enabled;
         bool ptw_cuckoo_enabled;
	      bool oracle_translation_enabled;
         bool oracle_protection_enabled;
         bool oracle_expressive_enabled;
         bool m_rlb_enabled;

         //Address scaling 
         int scaling_factor;

         ComponentLatency m_tlb_l1_cache_access;
         ComponentLatency m_tlb_l2_cache_access; 
         ComponentLatency m_tlb_nuca_cache_access;

         UInt64 nuca_time_series; 

	      Semaphore* m_user_thread_sem;
         Semaphore* m_network_thread_sem;

         UInt32 m_cache_block_size;
         MemComponent::component_t m_last_level_cache;
         bool m_enabled;

         ShmemPerf m_dummy_shmem_perf;

         // Performance Models
         CachePerfModel* m_cache_perf_models[MemComponent::LAST_LEVEL_CACHE + 1];

         // Global map of all caches on all cores (within this process!)
         static CacheCntlrMap m_all_cache_cntlrs;


      public:
        

         
         MemoryManager(Core* core, Network* network, ShmemPerfModel* shmem_perf_model);
         ~MemoryManager();

         UInt64 getCacheBlockSize() const { return m_cache_block_size; }

         Cache* getCache(MemComponent::component_t mem_component) {
              return m_cache_cntlrs[mem_component == MemComponent::LAST_LEVEL_CACHE ? MemComponent::component_t(m_last_level_cache) : mem_component]->getCache();
         }
         Cache* getL1ICache() { return getCache(MemComponent::L1_ICACHE); }
         Cache* getL1DCache() { return getCache(MemComponent::L1_DCACHE); }
         PageTableWalker* getPTW(){return ptw;}
         Cache* getLastLevelCache() { return getCache(MemComponent::LAST_LEVEL_CACHE); }
         TLB* getTLB() {return m_dtlb;}
         TLB* getPOTM() {return m_potm_tlb;}
         PrL1PrL2DramDirectoryMSI::DramDirectoryCache* getDramDirectoryCache() { return m_dram_directory_cntlr->getDramDirectoryCache(); }
         PrL1PrL2DramDirectoryMSI::DramCntlr* getDramCntlr() { return m_dram_cntlr; }
         AddressHomeLookup* getTagDirectoryHomeLookup() { return m_tag_directory_home_lookup; }
         AddressHomeLookup* getDramControllerHomeLookup() { return m_dram_controller_home_lookup; }
         void updateTranslationCounters(TranslationResult trResult, HitWhere::where_t dataResult);
         CacheCntlr* getCacheCntlrAt(core_id_t core_id, MemComponent::component_t mem_component) { return m_all_cache_cntlrs[CoreComponentType(core_id, mem_component)]; }
         void setCacheCntlrAt(core_id_t core_id, MemComponent::component_t mem_component, CacheCntlr* cache_cntlr) { m_all_cache_cntlrs[CoreComponentType(core_id, mem_component)] = cache_cntlr; }
         NucaCache* getNucaCache(){ return m_nuca_cache; }
         void measureNucaStats();

         HitWhere::where_t coreInitiateMemoryAccess(
               IntPtr eip,
               MemComponent::component_t mem_component,
               Core::lock_signal_t lock_signal,
               Core::mem_op_t mem_op_type,
               IntPtr address, UInt32 offset,
               Byte* data_buf, UInt32 data_length,
               Core::MemModeled modeled);

         TranslationResult performAddressTranslation(
            IntPtr eip,
            MemComponent::component_t mem_component,
            Core::lock_signal_t lock_signal,
            Core::mem_op_t mem_op_type,
            IntPtr address,
            Byte *data_buf, UInt32 data_length,
            bool modeled,
            bool count);
         
         TranslationResult accessUtopiaSubsystem(
            IntPtr eip,
            Core::lock_signal_t lock_signal,
            IntPtr address,
            Byte *data_buf, UInt32 data_length,
            bool modeled, 
            bool count);

         TranslationResult accessTLBSubsystem(
            IntPtr eip,
            TLB * tlb, 
            IntPtr address, 
            bool isIfetch, 
            bool modeled,
            bool count, 
            Core::lock_signal_t lock_signal, 
            Byte* data_buf, 
            UInt32 data_length);


         
         void handleMsgFromNetwork(NetPacket& packet);

         void sendMsg(PrL1PrL2DramDirectoryMSI::ShmemMsg::msg_t msg_type, MemComponent::component_t sender_mem_component, MemComponent::component_t receiver_mem_component, core_id_t requester, core_id_t receiver, IntPtr address, Byte* data_buf = NULL, UInt32 data_length = 0, HitWhere::where_t where = HitWhere::UNKNOWN, ShmemPerf *perf = NULL, ShmemPerfModel::Thread_t thread_num = ShmemPerfModel::NUM_CORE_THREADS, CacheBlockInfo::block_type_t block_type=CacheBlockInfo::block_type_t::NON_PAGE_TABLE);

         void broadcastMsg(PrL1PrL2DramDirectoryMSI::ShmemMsg::msg_t msg_type, MemComponent::component_t sender_mem_component, MemComponent::component_t receiver_mem_component, core_id_t requester, IntPtr address, Byte* data_buf = NULL, UInt32 data_length = 0, ShmemPerf *perf = NULL, ShmemPerfModel::Thread_t thread_num = ShmemPerfModel::NUM_CORE_THREADS);

         SubsecondTime getL1HitLatency(void) { return m_cache_perf_models[MemComponent::L1_ICACHE]->getLatency(CachePerfModel::ACCESS_CACHE_DATA_AND_TAGS); }
         void addL1Hits(bool icache, Core::mem_op_t mem_op_type, UInt64 hits) {
            (icache ? m_cache_cntlrs[MemComponent::L1_ICACHE] : m_cache_cntlrs[MemComponent::L1_DCACHE])->updateHits(mem_op_type, hits);
         }

         void enableModels();
         void disableModels();
	
	      IntPtr bitmap;

         core_id_t getShmemRequester(const void* pkt_data)
         { return ((PrL1PrL2DramDirectoryMSI::ShmemMsg*) pkt_data)->getRequester(); }

         UInt32 getModeledLength(const void* pkt_data)
         { return ((PrL1PrL2DramDirectoryMSI::ShmemMsg*) pkt_data)->getModeledLength(); }

         void tagCachesBlockType(IntPtr address, CacheBlockInfo::block_type_t btype){
                    if(m_nuca_cache) m_nuca_cache->markTranslationMetadata(address,btype);
                    this->getCache(MemComponent::component_t::L1_DCACHE)->markMetadata(address,btype);
                    this->getCache(MemComponent::component_t::L2_CACHE)->markMetadata(address,btype);
         }

         SubsecondTime getCost(MemComponent::component_t mem_component, CachePerfModel::CacheAccess_t access_type);
         void incrElapsedTime(SubsecondTime latency, ShmemPerfModel::Thread_t thread_num = ShmemPerfModel::NUM_CORE_THREADS);
         void incrElapsedTime(MemComponent::component_t mem_component, CachePerfModel::CacheAccess_t access_type, ShmemPerfModel::Thread_t thread_num = ShmemPerfModel::NUM_CORE_THREADS);
         

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
   };
}

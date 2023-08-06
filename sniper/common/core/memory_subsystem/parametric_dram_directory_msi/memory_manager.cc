#include "core_manager.h"
#include "memory_manager.h"
#include "cache_base.h"
#include "nuca_cache.h"
#include "dram_cache.h"
#include "tlb.h"
#include "simulator.h"
#include "log.h"
#include "dvfs_manager.h"
#include "itostr.h"
#include "instruction.h"
#include "config.hpp"
#include "distribution.h"
#include "topology_info.h"
#include "pagetable_walker.h"
#include "allocation_manager.h"
#include "pagetable_walker_cuckoo.h"
#include "pagetable_walker_radix.h"
#include "pagetable_walker_hdc.h"
#include "page_table_virtualized.h"
#include "contention_model.h"
#include "utopia.h"
#include "pwc.h"
#include "hashtable_baseline.h"
#include "thread.h"

#include <algorithm>

#if 0
   extern Lock iolock;
#  include "core_manager.h"
#  include "simulator.h"
#  define MYLOG(...) { ScopedLock l(iolock); fflush(stderr); fprintf(stderr, "[%s] %d%cmm %-25s@%03u: ", itostr(getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD)).c_str(), getCore()->getId(), Sim()->getCoreManager()->amiUserThread() ? '^' : '_', __FUNCTION__, __LINE__); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); fflush(stderr); }
#else
#  define MYLOG(...) {}
#endif


namespace ParametricDramDirectoryMSI
{

std::map<CoreComponentType, CacheCntlr*> MemoryManager::m_all_cache_cntlrs;

MemoryManager::MemoryManager(Core* core,
      Network* network, ShmemPerfModel* shmem_perf_model):
   MemoryManagerBase(core, network, shmem_perf_model),
   m_nuca_cache(NULL),
   m_dram_cache(NULL),
   m_dram_directory_cntlr(NULL),
   m_dram_cntlr(NULL),
   m_itlb(NULL), m_dtlb(NULL), m_stlb(NULL),
   m_tlb_miss_penalty(NULL,0),
   m_tlb_miss_parallel(false),
   m_tag_directory_present(false),
   m_dram_cntlr_present(false),
   m_enabled(false),
   m_utopia_enabled(false),
   m_tlb_l1_access_penalty(NULL, 0),
   m_tlb_l2_access_penalty(NULL, 0),
   m_tlb_l1_miss_penalty(NULL, 0),
   m_tlb_l2_miss_penalty(NULL, 0),
   m_tlb_l1_cache_access(NULL,0),
   m_tlb_l2_cache_access(NULL,0),
   m_tlb_nuca_cache_access(NULL,0),
   scaling_factor(0),
   pwc_access_latency(NULL, 0),
   pwc_miss_latency(NULL, 0),
   shadow_cache_hit_latency(NULL,0),
   shadow_cache_miss_latency(NULL,0),
   tlb_caching(false),
   potm_latency(NULL,0),
   migration_latency(NULL,0)

{
   // Read Parameters from the Config file
   std::map<MemComponent::component_t, CacheParameters> cache_parameters;
   std::map<MemComponent::component_t, String> cache_names;
   
   bool nuca_enable = false;
   CacheParameters nuca_parameters;
   nuca_time_series = 0 ;
   const ComponentPeriod *global_domain = Sim()->getDvfsManager()->getGlobalDomain();

   UInt32 smt_cores;
   bool dram_direct_access = false;
   UInt32 dram_directory_total_entries = 0;
   UInt32 dram_directory_associativity = 0;
   UInt32 dram_directory_max_num_sharers = 0;
   UInt32 dram_directory_max_hw_sharers = 0;
   String dram_directory_type_str;
   UInt32 dram_directory_home_lookup_param = 0;
   ComponentLatency dram_directory_cache_access_time(global_domain, 0);

   current_nuca_stamp = 0;

   try
   {
      m_cache_block_size = Sim()->getCfg()->getInt("perf_model/l1_icache/cache_block_size");

      m_last_level_cache = (MemComponent::component_t)(Sim()->getCfg()->getInt("perf_model/cache/levels") - 2 + MemComponent::L2_CACHE);
      m_system_page_size = Sim()->getCfg()->getInt("perf_model/tlb/pagesize");
   

      int number_of_page_sizes = Sim()->getCfg()->getInt("perf_model/tlb/page_sizes");
      int* page_size_list;
      
      page_size_list = (int *) malloc (sizeof(int)*(number_of_page_sizes));
      std::cout << "Supporting " << number_of_page_sizes << "page sizes: ";

      for (int i = 0; i < number_of_page_sizes; i++){

         page_size_list[i] = Sim()->getCfg()->getIntArray("perf_model/tlb/page_size_list", i);
         std::cout << page_size_list[i] << ", ";

      }


      // Utopia-related params

       m_utopia_enabled = Sim()->getCfg()->getBool("perf_model/utopia/enabled");
       m_ulb_enabled =  Sim()->getCfg()->getBool("perf_model/utopia/ulb/enabled");

      if(m_ulb_enabled){

         int ulb_size  = Sim()->getCfg()->getInt("perf_model/utopia/ulb/size");
         int ulb_assoc  = Sim()->getCfg()->getInt("perf_model/utopia/ulb/assoc");

         ComponentLatency ulb_access_latency  = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/utopia/ulb/access_penalty"));
         ComponentLatency ulb_miss_latency  = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/utopia/ulb/miss_penalty"));

         ulb = new ULB("ulb", "perf_model/utopia/ulb", getCore()->getId(),m_system_page_size, ulb_size, ulb_assoc, ulb_access_latency, ulb_miss_latency, page_size_list,  number_of_page_sizes);

      }
      m_utopia_permission_enabled = Sim()->getCfg()->getBool("perf_model/utopia/pcache/enabled");

      if(m_utopia_permission_enabled){

         int permission_cache_size = Sim()->getCfg()->getInt("perf_model/utopia/pcache/size");
         int permission_cache_assoc = Sim()->getCfg()->getInt("perf_model/utopia/pcache/assoc");

         ComponentLatency permission_cache_access_latency  = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/utopia/pcache/access_penalty"));
         ComponentLatency permission_cache_miss_latency  = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/utopia/pcache/miss_penalty"));

         utopia_perm_cache  = new UtopiaCache ("uperm", "perf_model/utopia/pcache", 
                                                getCore()->getId(),
                                                64, 
                                                permission_cache_size, 
                                                permission_cache_assoc, 
                                                permission_cache_access_latency, 
                                            permission_cache_miss_latency);
                                             

      }

      m_utopia_tag_enabled = Sim()->getCfg()->getBool("perf_model/utopia/tagcache/enabled");

      if(m_utopia_tag_enabled){

         int tag_cache_size = Sim()->getCfg()->getInt("perf_model/utopia/tagcache/size");;
         int tag_cache_assoc = Sim()->getCfg()->getInt("perf_model/utopia/tagcache/assoc");;

         ComponentLatency tag_cache_access_latency  = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/utopia/tagcache/access_penalty"));
         ComponentLatency tag_cache_miss_latency  = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/utopia/tagcache/miss_penalty"));
         
         utopia_tag_cache  = new UtopiaCache ("utag", 
                                             "perf_model/utopia/tagcache", 
                                             getCore()->getId(),
                                             64, 
                                             tag_cache_size, 
                                             tag_cache_assoc, 
                                             tag_cache_access_latency, 
                                             tag_cache_miss_latency);
      }



      bzero(&translation_stats, sizeof(translation_stats));

      registerStatsMetric("mmu", core->getId(), "tlb_hit", &translation_stats.tlb_hit);
   	registerStatsMetric("mmu", core->getId(), "utr_hit", &translation_stats.utr_hit);
   	registerStatsMetric("mmu", core->getId(), "tlb_and_utr_miss", &translation_stats.tlb_and_utr_miss);
      registerStatsMetric("mmu", core->getId(), "tlb_hit_latency", &translation_stats.tlb_hit_latency);
   	registerStatsMetric("mmu", core->getId(), "utr_hit_latency", &translation_stats.utr_hit_latency);
		registerStatsMetric("mmu", core->getId(), "tlb_and_utr_miss_latency", &translation_stats.tlb_and_utr_miss_latency);
		registerStatsMetric("mmu", core->getId(), "total_latency",&translation_stats.total_latency);
          
      //Victima parameters
      registerStatsMetric("mmu", core->getId(), "l1_tlb_hit",  &translation_stats.tlb_hit_l1_latency);
      registerStatsMetric("mmu", core->getId(), "l2_tlb_hit",  &translation_stats.tlb_hit_l2_latency);
      registerStatsMetric("mmu", core->getId(), "l1c_hit_tlb_latency",  &translation_stats.l1c_hit_tlb_latency);
      registerStatsMetric("mmu", core->getId(), "l2c_hit_tlb_latency",  &translation_stats.l2c_hit_tlb_latency);
      registerStatsMetric("mmu", core->getId(), "nucac_hit_tlb_latency",  &translation_stats.nucac_hit_tlb_latency);
      
      registerStatsMetric("mmu", core->getId(), "l1c_hit_tlb",  &translation_stats.l1c_hit_tlb);
      registerStatsMetric("mmu", core->getId(), "l2c_hit_tlb",  &translation_stats.l2c_hit_tlb);
      registerStatsMetric("mmu", core->getId(), "nucac_hit_tlb",  &translation_stats.nucac_hit_tlb);

      //Correlate llc accesses with TLB accesses
      registerStatsMetric("mmu", core->getId(), "llc_miss_and_l1tlb_hit", &translation_stats.llc_miss_l1tlb_hit);
      registerStatsMetric("mmu", core->getId(), "llc_miss_and_l2tlb_hit", &translation_stats.llc_miss_l2tlb_hit);
      registerStatsMetric("mmu", core->getId(), "llc_miss_and_l2tlb_miss", &translation_stats.llc_miss_l2tlb_miss);
      registerStatsMetric("mmu", core->getId(), "llc_hit_and_l1tlb_hit", &translation_stats.llc_hit_l1tlb_hit);
      registerStatsMetric("mmu", core->getId(), "llc_hit_and_l2tlb_hit", &translation_stats.llc_hit_l2tlb_hit);
      registerStatsMetric("mmu", core->getId(), "llc_hit_and_l2tlb_miss", &translation_stats.llc_hit_l2tlb_miss);
      registerStatsMetric("mmu", core->getId(), "ptw_contention",  &translation_stats.ptw_contention);

      registerStatsMetric("mmu", core->getId(), "migrations_requests",  &translation_stats.migrations_affected_request);
      registerStatsMetric("mmu", core->getId(), "migrations_delay",  &translation_stats.migrations_affected_latency);


      for (int i = HitWhere::WHERE_FIRST; i < HitWhere::NUM_HITWHERES; i++)
      {
         if (HitWhereIsValid((HitWhere::where_t)i))
         {
            registerStatsMetric("mmu-metadata", core->getId(), HitWhereString((HitWhere::where_t)i), &metadata_stats.metadata_hit[i]);
         }
      }

      // Baseline Translation with PTW and Page Walk Caches
      oracle_translation_enabled = Sim()->getCfg()->getBool("perf_model/ptw/oracle_translation_enabled");
      m_pwc_enabled = Sim()->getCfg()->getBool("perf_model/ptw/pwc/enabled");
      radix_enabled = Sim()->getCfg()->getBool("perf_model/ptw/enabled");
      ptw_cuckoo_enabled = Sim()->getCfg()->getBool("perf_model/ptw_cuckoo/enabled");
      oracle_protection_enabled= Sim()->getCfg()->getBool("perf_model/modrian_memory/oracle_protection_enabled");
      oracle_expressive_enabled= Sim()->getCfg()->getBool("perf_model/xmem/oracle_expressive_enabled");
      xmem_enabled= Sim()->getCfg()->getBool("perf_model/xmem/enabled");
      modrian_memory_enabled= Sim()->getCfg()->getBool("perf_model/modrian_memory/enabled");
      hash_dont_cache_enabled= Sim()->getCfg()->getBool("perf_model/hash_dont_cache/enabled");
      hash_baseline_enabled= Sim()->getCfg()->getBool("perf_model/hash_baseline/enabled");
      parallel_ptw = Sim()->getCfg()->getInt("perf_model/ptw/parallel");
      m_potm_enabled = Sim()->getCfg()->getBool("perf_model/tlb/potm_enabled");
      m_virtualized = Sim()->getCfg()->getBool("perf_model/ptw/virtualized");
      m_parallel_walk = Sim()->getCfg()->getBool("perf_model/ptw/parallel_walk");
      ptw_srs = new ContentionModel("ptw.mshr", getCore()->getId(), parallel_ptw);
      

     m_tlb_l1_cache_access =  ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/l1_dcache/data_access_time"));;
     m_tlb_l2_cache_access = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/l2_cache/data_access_time"));
     m_tlb_nuca_cache_access = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/nuca/data_access_time"));
     potm_latency = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/potm_tlb/latency"));
     migration_latency = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/utopia/migration_latency"));

      std::cout << "POTM Latency: " << Sim()->getCfg()->getInt("perf_model/potm_tlb/latency") << std::endl;

      if(m_pwc_enabled){

         pwc_L4_size  = Sim()->getCfg()->getInt("perf_model/ptw/pwc/l4_size");
         pwc_L4_assoc  = Sim()->getCfg()->getInt("perf_model/ptw/pwc/l4_assoc");
    
         pwc_L3_size  = Sim()->getCfg()->getInt("perf_model/ptw/pwc/l3_size");
         pwc_L3_assoc  = Sim()->getCfg()->getInt("perf_model/ptw/pwc/l3_assoc");

         pwc_L2_size  = Sim()->getCfg()->getInt("perf_model/ptw/pwc/l2_size");
         pwc_L2_assoc  = Sim()->getCfg()->getInt("perf_model/ptw/pwc/l2_assoc");

         bool pwc_perfect =  Sim()->getCfg()->getBool("perf_model/ptw/pwc/perfect");

         pwc_access_latency  = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/ptw/pwc/access_penalty"));
         pwc_miss_latency  = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/ptw/pwc/miss_penalty"));

         pwc  = new PWC("pwc", "perf_model/ptw/pwc", getCore()->getId(),pwc_L4_assoc,pwc_L4_size,pwc_L3_assoc,pwc_L3_size,pwc_L2_assoc,pwc_L2_size, pwc_access_latency, pwc_miss_latency, pwc_perfect);
      }
      

      if(radix_enabled){

               int levels_ptw=Sim()->getCfg()->getInt("perf_model/ptw_radix/levels");
               int indices_ptw[levels_ptw+1];
               int percentages_ptw[levels_ptw];
               for (int i = 0; i < levels_ptw+1; i++)
               {
                  
                  if(i<levels_ptw){
                     
                     percentages_ptw[i]=Sim()->getCfg()->getIntArray("perf_model/ptw_radix/percentages",i); //@kanellok 2MB/4KB breakdown
                     std::cout<<percentages_ptw[i]<<"\n";
                  }
                  indices_ptw[i]=Sim()->getCfg()->getIntArray("perf_model/ptw_radix/indices",i); //@kanellok reconfigurable PTW
               }
               ptw = new PageTableWalkerRadix(levels_ptw,getCore(),shmem_perf_model,indices_ptw,percentages_ptw, pwc, m_pwc_enabled,shadow_cache); //@kanellok new PTW with pointers
               ptw->setMemoryManager(this);
            

      }
      else if (ptw_cuckoo_enabled){

            int d = Sim()->getCfg()->getInt("perf_model/ptw_cuckoo/d");
            int size = Sim()->getCfg()->getInt("perf_model/ptw_cuckoo/size");
            String hash_func = Sim()->getCfg()->getString("perf_model/ptw_cuckoo/hash_func");
            float rehash_threshold = Sim()->getCfg()->getFloat("perf_model/ptw_cuckoo/rehash_threshold");
            int scale = Sim()->getCfg()->getInt("perf_model/ptw_cuckoo/scale");
            int swaps = Sim()->getCfg()->getInt("perf_model/ptw_cuckoo/swaps");
            int priority = Sim()->getCfg()->getInt("perf_model/ptw_cuckoo/priority");
            m_virtualized = Sim()->getCfg()->getBool("perf_model/ptw_cuckoo/virtual");

            ptw = new PageTableWalkerCuckoo(getCore()->getId(),m_system_page_size,getShmemPerfModel(),pwc,m_pwc_enabled,d, strdup(hash_func.c_str()),size,rehash_threshold,scale,swaps,priority);
            ptw->setMemoryManager(this);
            

      }
      else if (m_virtualized){

               int levels_ptw=Sim()->getCfg()->getInt("perf_model/ptw_radix/levels");
               int indices_ptw[levels_ptw+1];
               int percentages_ptw[levels_ptw];
               for (int i = 0; i < levels_ptw+1; i++)
               {
                  
                  if(i<levels_ptw){
                     
                     percentages_ptw[i]=Sim()->getCfg()->getIntArray("perf_model/ptw_radix/percentages",i); //@kanellok 2MB/4KB breakdown
                     std::cout<<percentages_ptw[i]<<"\n";
                  }
                  indices_ptw[i]=Sim()->getCfg()->getIntArray("perf_model/ptw_radix/indices",i); //@kanellok reconfigurable PTW
               }
               std::cout<<"PWC" << m_pwc_enabled << "\n";
               ptw = new PageTableVirtualized(levels_ptw,getCore(),shmem_perf_model,indices_ptw,percentages_ptw, pwc, m_pwc_enabled,shadow_cache); //@kanellok new PTW with pointers
               ptw->setMemoryManager(this);
            

      }
            
      if(xmem_enabled){
         
         int size=Sim()->getCfg()->getInt("perf_model/xmem/size");
         int granularity=Sim()->getCfg()->getInt("perf_model/xmem/granularity");
         int cache_size=Sim()->getCfg()->getInt("perf_model/xmem/cache_size");
         int cache_associativity=Sim()->getCfg()->getInt("perf_model/xmem/cache_associativity");
         int cache_hit_latency=Sim()->getCfg()->getInt("perf_model/xmem/cache_hit_latency");
         int cache_miss_latency=Sim()->getCfg()->getInt("perf_model/xmem/cache_miss_latency");
         
         xmem_manager = new XMemManager(size,granularity,cache_size,cache_associativity,cache_hit_latency,cache_miss_latency,core,shmem_perf_model,pwc,m_pwc_enabled); 
         xmem_manager->setMemoryManager(this);

      }
      
     
      if(modrian_memory_enabled){
         
         int levels=Sim()->getCfg()->getInt("perf_model/modrian_memory/levels");
         
         int indices[levels+1];
         int percentages[levels];
         int segments=Sim()->getCfg()->getInt("perf_model/modrian_memory/segments");
         
         for (int i = 0; i < levels+1; i++)
         {
            if(i<levels){   
               percentages[i]=Sim()->getCfg()->getIntArray("perf_model/modrian_memory/percentages",i);
            }
            indices[i]=Sim()->getCfg()->getIntArray("perf_model/modrian_memory/indices",i);
         }
         
         mmm = new ModrianMemoryManager(levels,core,shmem_perf_model,percentages,indices,segments,pwc,m_pwc_enabled);
         mmm->setMemoryManager(this);
      }


      if(hash_dont_cache_enabled){

         int page_table_bits=Sim()->getCfg()->getInt("perf_model/hash_dont_cache/page_table_size_in_bits");
         int large_page_size=Sim()->getCfg()->getInt("perf_model/hash_dont_cache/large_page_size");
         int small_page_size=Sim()->getCfg()->getInt("perf_model/hash_dont_cache/small_page_size");
         int small_page_percentage=Sim()->getCfg()->getInt("perf_model/hash_dont_cache/small_page_percentage");
         ptw = new PageTableHashDontCache(page_table_bits,small_page_size,large_page_size,small_page_percentage,core,shmem_perf_model,pwc,false);
         ptw->setMemoryManager(this);

      }


      if(hash_baseline_enabled){

         int page_table_bits=Sim()->getCfg()->getInt("perf_model/hash_baseline/page_table_size_in_bits");
         int large_page_size=Sim()->getCfg()->getInt("perf_model/hash_baseline/large_page_size");
         int small_page_size=Sim()->getCfg()->getInt("perf_model/hash_baseline/small_page_size");
         int small_page_percentage=Sim()->getCfg()->getInt("perf_model/hash_baseline/small_page_percentage");
         ptw = new HashTablePTW(page_table_bits,small_page_size,large_page_size,small_page_percentage,core,shmem_perf_model,pwc,false);
         ptw->setMemoryManager(this);
      }

      /* L1-Metadata Cache to avoid caching in the L1 D-Cache*/
      shadow_cache_enabled  = Sim()->getCfg()->getBool("perf_model/metadata/shadow_cache_enabled");

      if(shadow_cache_enabled){

         shadow_cache_size=Sim()->getCfg()->getInt("perf_model/metadata/shadow_cache_size");
         shadow_cache_associativity=Sim()->getCfg()->getInt("perf_model/metadata/shadow_cache_associativity");
         shadow_cache_hit_latency=ComponentLatency(core->getDvfsDomain(),Sim()->getCfg()->getInt("perf_model/metadata/shadow_cache_hit_latency"));
         shadow_cache_miss_latency=ComponentLatency(core->getDvfsDomain(),Sim()->getCfg()->getInt("perf_model/metadata/shadow_cache_miss_latency"));

         shadow_cache  = new UtopiaCache ("shadow_metadata", 
                                              "perf_model/metadata/", 
                                              getCore()->getId(),
                                              64, 
                                              shadow_cache_size, 
                                              shadow_cache_associativity, 
                                              shadow_cache_hit_latency, 
                                              shadow_cache_miss_latency);
       }



      m_tlb_l1_access_penalty = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/tlb/l1_access_penalty"));
      m_tlb_l2_access_penalty = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/tlb/l2_access_penalty"));

      m_tlb_l1_miss_penalty = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/tlb/l1_miss_penalty"));
      m_tlb_l2_miss_penalty = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/tlb/l2_miss_penalty"));

      bool track_misses =  Sim()->getCfg()->getBool("perf_model/tlb/track_misses");
      bool track_accesses = Sim()->getCfg()->getBool("perf_model/tlb/track_accesses");




      UInt32 potm_size = Sim()->getCfg()->getInt("perf_model/potm_tlb/size");
      if(m_potm_enabled){
         std::cout << "[VM] POTM is enabled " << std::endl;
         m_potm_tlb = new TLB("potm_tlb", "perf_model/potm_tlb", getCore()->getId(),shmem_perf_model, potm_size,m_system_page_size, Sim()->getCfg()->getInt("perf_model/potm_tlb/associativity"), NULL, m_utopia_enabled, track_misses, track_accesses, page_size_list,  number_of_page_sizes, ptw);
         m_potm_tlb->setPOTMDataStructure(potm_size);

      }
      UInt32 stlb_size = Sim()->getCfg()->getInt("perf_model/stlb/size");

      if(stlb_size)
            m_stlb = new TLB("stlb", "perf_model/stlb", getCore()->getId(),shmem_perf_model, stlb_size,m_system_page_size, Sim()->getCfg()->getInt("perf_model/stlb/associativity"), NULL, m_utopia_enabled,  track_misses, track_accesses,page_size_list,  number_of_page_sizes, ptw);
      
      UInt32 itlb_size = Sim()->getCfg()->getInt("perf_model/itlb/size");
      if (itlb_size)
         m_itlb = new TLB("itlb", "perf_model/itlb", getCore()->getId(),shmem_perf_model, itlb_size,m_system_page_size, Sim()->getCfg()->getInt("perf_model/itlb/associativity"), m_stlb, m_utopia_enabled, track_misses, track_accesses,page_size_list,  number_of_page_sizes, ptw);
      
      UInt32 dtlb_size = Sim()->getCfg()->getInt("perf_model/dtlb/size");
      if (dtlb_size){
         m_dtlb = new TLB("dtlb", "perf_model/dtlb", getCore()->getId(),shmem_perf_model, dtlb_size,m_system_page_size, Sim()->getCfg()->getInt("perf_model/dtlb/associativity"), m_stlb, m_utopia_enabled, track_misses, track_accesses, page_size_list,  number_of_page_sizes, ptw);
         m_dtlb->setMemManager(this);
      }



      m_tlb_miss_penalty = ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/tlb/penalty"));
      m_tlb_miss_parallel = Sim()->getCfg()->getBool("perf_model/tlb/penalty_parallel");

      
      // RMM ISCA 2015 - Utopia Comparison point

      m_rlb_enabled = Sim()->getCfg()->getBool("perf_model/rlb/enabled");
      String rlb_policy = Sim()->getCfg()->getString("perf_model/rlb/policy");
      //m_va_reader = new vaAreaReader();
      //std::cout << "Executing application with PID = " << getCore()->getThread()->getId() << std::endl;


      if(m_rlb_enabled){

         m_rlb = new RLB(core,
                         "rlb",
                         ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/rlb/latency")),
                         ComponentLatency(core->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/rlb/miss_latency")),
                         Sim()->getCfg()->getInt("perf_model/rlb/entries"),
                         rlb_policy
                         );

      }



      smt_cores = Sim()->getCfg()->getInt("perf_model/core/logical_cpus");

      for(UInt32 i = MemComponent::FIRST_LEVEL_CACHE; i <= (UInt32)m_last_level_cache; ++i)
      {
         String configName, objectName;
         switch((MemComponent::component_t)i) {
            case MemComponent::L1_ICACHE:
               configName = "l1_icache";
               objectName = "L1-I";
               break;
            case MemComponent::L1_DCACHE:
               configName = "l1_dcache";
               objectName = "L1-D";
               break;
            default:
               String level = itostr(i - MemComponent::L2_CACHE + 2);
               configName = "l" + level + "_cache";
               objectName = "L" + level;
               break;
         }

         const ComponentPeriod *clock_domain = NULL;
         String domain_name = Sim()->getCfg()->getStringArray("perf_model/" + configName + "/dvfs_domain", core->getId());
         if (domain_name == "core")
            clock_domain = core->getDvfsDomain();
         else if (domain_name == "global")
            clock_domain = global_domain;
         else
            LOG_PRINT_ERROR("dvfs_domain %s is invalid", domain_name.c_str());

         LOG_ASSERT_ERROR(Sim()->getCfg()->getInt("perf_model/" + configName + "/cache_block_size") == m_cache_block_size,
                          "The cache block size of the %s is not the same as the l1_icache (%d)", configName.c_str(), m_cache_block_size);

         cache_parameters[(MemComponent::component_t)i] = CacheParameters(
            configName,
            Sim()->getCfg()->getIntArray(   "perf_model/" + configName + "/cache_size", core->getId()),
            Sim()->getCfg()->getIntArray(   "perf_model/" + configName + "/associativity", core->getId()),
            getCacheBlockSize(),
            Sim()->getCfg()->getStringArray("perf_model/" + configName + "/address_hash", core->getId()),
            Sim()->getCfg()->getStringArray("perf_model/" + configName + "/replacement_policy", core->getId()),
            Sim()->getCfg()->getBoolArray(  "perf_model/" + configName + "/perfect", core->getId()),
            i == MemComponent::L1_ICACHE
               ? Sim()->getCfg()->getBoolArray(  "perf_model/" + configName + "/coherent", core->getId())
               : true,
            ComponentLatency(clock_domain, Sim()->getCfg()->getIntArray("perf_model/" + configName + "/data_access_time", core->getId())),
            ComponentLatency(clock_domain, Sim()->getCfg()->getIntArray("perf_model/" + configName + "/tags_access_time", core->getId())),
            ComponentLatency(clock_domain, Sim()->getCfg()->getIntArray("perf_model/" + configName + "/writeback_time", core->getId())),
            ComponentBandwidthPerCycle(clock_domain,
               i < (UInt32)m_last_level_cache
                  ? Sim()->getCfg()->getIntArray("perf_model/" + configName + "/next_level_read_bandwidth", core->getId())
                  : 0),
            Sim()->getCfg()->getStringArray("perf_model/" + configName + "/perf_model_type", core->getId()),
            Sim()->getCfg()->getBoolArray(  "perf_model/" + configName + "/writethrough", core->getId()),
            Sim()->getCfg()->getIntArray(   "perf_model/" + configName + "/shared_cores", core->getId()) * smt_cores,
            Sim()->getCfg()->getStringArray("perf_model/" + configName + "/prefetcher", core->getId()),
            i == MemComponent::L1_DCACHE
               ? Sim()->getCfg()->getIntArray(   "perf_model/" + configName + "/outstanding_misses", core->getId())
               : 0
         );
         cache_names[(MemComponent::component_t)i] = objectName;

         /* Non-application threads will be distributed at 1 per process, probably not as shared_cores per process.
            Still, they need caches (for inter-process data communication, not for modeling target timing).
            Make them non-shared so we don't create process-spanning shared caches. */
         if (getCore()->getId() >= (core_id_t) Sim()->getConfig()->getApplicationCores())
            cache_parameters[(MemComponent::component_t)i].shared_cores = 1;
      }
     

        /* Non-application threads will be distributed at 1 per process, probably not as shared_cores per process.
          Still, they need caches (for inter-process data communication, not for modeling target timing).
          Make them non-shared so we don't create process-spanning shared caches. */


      nuca_enable = Sim()->getCfg()->getBoolArray(  "perf_model/nuca/enabled", core->getId());

      if(nuca_enable){
         std::cout << "Registering cache time series" << std::endl;
         registerStatsMetric("mmu", core->getId(), "cache_timeseries", &nuca_time_series,true);
      }

      if (nuca_enable)
      {
         nuca_parameters = CacheParameters(
            "nuca",
            Sim()->getCfg()->getIntArray(   "perf_model/nuca/cache_size", core->getId()),
            Sim()->getCfg()->getIntArray(   "perf_model/nuca/associativity", core->getId()),
            getCacheBlockSize(),
            Sim()->getCfg()->getStringArray("perf_model/nuca/address_hash", core->getId()),
            Sim()->getCfg()->getStringArray("perf_model/nuca/replacement_policy", core->getId()),
            false, true,
            ComponentLatency(global_domain, Sim()->getCfg()->getIntArray("perf_model/nuca/data_access_time", core->getId())),
            ComponentLatency(global_domain, Sim()->getCfg()->getIntArray("perf_model/nuca/tags_access_time", core->getId())),
            ComponentLatency(global_domain, 0), ComponentBandwidthPerCycle(global_domain, 0), "", false, 0, "", 0 // unused
         );
      }

      // Dram Directory Cache
      dram_directory_total_entries = Sim()->getCfg()->getInt("perf_model/dram_directory/total_entries");
      dram_directory_associativity = Sim()->getCfg()->getInt("perf_model/dram_directory/associativity");
      dram_directory_max_num_sharers = Sim()->getConfig()->getTotalCores();
      dram_directory_max_hw_sharers = Sim()->getCfg()->getInt("perf_model/dram_directory/max_hw_sharers");
      dram_directory_type_str = Sim()->getCfg()->getString("perf_model/dram_directory/directory_type");
      dram_directory_home_lookup_param = Sim()->getCfg()->getInt("perf_model/dram_directory/home_lookup_param");
      dram_directory_cache_access_time = ComponentLatency(global_domain, Sim()->getCfg()->getInt("perf_model/dram_directory/directory_cache_access_time"));

      // Dram Cntlr
      dram_direct_access = Sim()->getCfg()->getBool("perf_model/dram/direct_access");
   }
   catch(...)
   {
      LOG_PRINT_ERROR("Error reading memory system parameters from the config file");
   }

   m_user_thread_sem = new Semaphore(0);
   m_network_thread_sem = new Semaphore(0);

   std::vector<core_id_t> core_list_with_dram_controllers = getCoreListWithMemoryControllers();
   std::vector<core_id_t> core_list_with_tag_directories;
   String tag_directory_locations = Sim()->getCfg()->getString("perf_model/dram_directory/locations");

   if (tag_directory_locations == "dram")
   {
      // Place tag directories only at DRAM controllers
      core_list_with_tag_directories = core_list_with_dram_controllers;
   }
   else
   {
      SInt32 tag_directory_interleaving;

      // Place tag directores at each (master) cache
      if (tag_directory_locations == "llc")
      {
         tag_directory_interleaving = cache_parameters[m_last_level_cache].shared_cores;
      }
      else if (tag_directory_locations == "interleaved")
      {
         tag_directory_interleaving = Sim()->getCfg()->getInt("perf_model/dram_directory/interleaving") * smt_cores;
      }
      else
      {
         LOG_PRINT_ERROR("Invalid perf_model/dram_directory/locations value %s", tag_directory_locations.c_str());
      }

      for(core_id_t core_id = 0; core_id < (core_id_t)Sim()->getConfig()->getApplicationCores(); core_id += tag_directory_interleaving)
      {
         core_list_with_tag_directories.push_back(core_id);
      }
   }

   m_tag_directory_home_lookup = new AddressHomeLookup(dram_directory_home_lookup_param, core_list_with_tag_directories, getCacheBlockSize());
   m_dram_controller_home_lookup = new AddressHomeLookup(dram_directory_home_lookup_param, core_list_with_dram_controllers, getCacheBlockSize());

   // if (m_core->getId() == 0)
   //   printCoreListWithMemoryControllers(core_list_with_dram_controllers);

   if (find(core_list_with_dram_controllers.begin(), core_list_with_dram_controllers.end(), getCore()->getId()) != core_list_with_dram_controllers.end())
   {
      m_dram_cntlr_present = true;

      m_dram_cntlr = new PrL1PrL2DramDirectoryMSI::DramCntlr(this,
            getShmemPerfModel(),
            getCacheBlockSize(),m_dram_controller_home_lookup);
      Sim()->getStatsManager()->logTopology("dram-cntlr", core->getId(), core->getId());

      if (Sim()->getCfg()->getBoolArray("perf_model/dram/cache/enabled", core->getId()))
      {
         m_dram_cache = new DramCache(this, getShmemPerfModel(), m_dram_controller_home_lookup, getCacheBlockSize(), m_dram_cntlr);
         Sim()->getStatsManager()->logTopology("dram-cache", core->getId(), core->getId());
      }
   }

   if (find(core_list_with_tag_directories.begin(), core_list_with_tag_directories.end(), getCore()->getId()) != core_list_with_tag_directories.end())
   {
      m_tag_directory_present = true;

      if (!dram_direct_access)
      {
         if (nuca_enable)
         {
            m_nuca_cache = new NucaCache(
               this,
               getShmemPerfModel(),
               m_tag_directory_home_lookup,
               getCacheBlockSize(),
               nuca_parameters);
            Sim()->getStatsManager()->logTopology("nuca-cache", core->getId(), core->getId());
         
               ptw->setNucaCache(m_nuca_cache);
               if(modrian_memory_enabled){
                  mmm->setNucaCache(m_nuca_cache);
               }
               if(xmem_enabled){
                  xmem_manager->setNucaCache(m_nuca_cache);
               }
         }

         m_dram_directory_cntlr = new PrL1PrL2DramDirectoryMSI::DramDirectoryCntlr(getCore()->getId(),
               this,
               m_dram_controller_home_lookup,
               m_nuca_cache,
               dram_directory_total_entries,
               dram_directory_associativity,
               getCacheBlockSize(),
               dram_directory_max_num_sharers,
               dram_directory_max_hw_sharers,
               dram_directory_type_str,
               dram_directory_cache_access_time,
               getShmemPerfModel());
         Sim()->getStatsManager()->logTopology("tag-dir", core->getId(), core->getId());
      }
   }
   
   for(UInt32 i = MemComponent::FIRST_LEVEL_CACHE; i <= (UInt32)m_last_level_cache; ++i) {

      std::cout << "Initializing cache L " << (MemComponent::component_t)i << std::endl;
      CacheCntlr* cache_cntlr = new CacheCntlr(
         (MemComponent::component_t)i,
         cache_names[(MemComponent::component_t)i],
         getCore()->getId(),
         this,
         m_tag_directory_home_lookup,
         m_user_thread_sem,
         m_network_thread_sem,
         getCacheBlockSize(),
         cache_parameters[(MemComponent::component_t)i],
         getShmemPerfModel(),
         i == (UInt32)m_last_level_cache
      );
      m_cache_cntlrs[(MemComponent::component_t)i] = cache_cntlr;
      setCacheCntlrAt(getCore()->getId(), (MemComponent::component_t)i, cache_cntlr);
   }
   




   m_cache_cntlrs[MemComponent::L1_ICACHE]->setNextCacheCntlr(m_cache_cntlrs[MemComponent::L2_CACHE]);
   m_cache_cntlrs[MemComponent::L1_DCACHE]->setNextCacheCntlr(m_cache_cntlrs[MemComponent::L2_CACHE]);
   for(UInt32 i = MemComponent::L2_CACHE; i <= (UInt32)m_last_level_cache - 1; ++i) 
   {
      m_cache_cntlrs[(MemComponent::component_t)i]->setNextCacheCntlr(m_cache_cntlrs[(MemComponent::component_t)(i + 1)]);
   }

   CacheCntlrList prev_cache_cntlrs;
   prev_cache_cntlrs.push_back(m_cache_cntlrs[MemComponent::L1_ICACHE]);
   prev_cache_cntlrs.push_back(m_cache_cntlrs[MemComponent::L1_DCACHE]);
   m_cache_cntlrs[MemComponent::L2_CACHE]->setPrevCacheCntlrs(prev_cache_cntlrs);

   for(UInt32 i = MemComponent::L2_CACHE; i <= (UInt32)m_last_level_cache - 1; ++i) {
      CacheCntlrList prev_cache_cntlrs;
      prev_cache_cntlrs.push_back(m_cache_cntlrs[(MemComponent::component_t)i]);
      m_cache_cntlrs[(MemComponent::component_t)(i + 1)]->setPrevCacheCntlrs(prev_cache_cntlrs);
   }

   // Create Performance Modes
   for(UInt32 i = MemComponent::FIRST_LEVEL_CACHE; i <= (UInt32)m_last_level_cache; ++i)
      m_cache_perf_models[(MemComponent::component_t)i] = CachePerfModel::create(
       cache_parameters[(MemComponent::component_t)i].perf_model_type,
       cache_parameters[(MemComponent::component_t)i].data_access_time,
       cache_parameters[(MemComponent::component_t)i].tags_access_time
      );


   if (m_dram_cntlr_present)
      LOG_ASSERT_ERROR(m_cache_cntlrs[m_last_level_cache]->isMasterCache() == true,
                       "DRAM controllers may only be at 'master' node of shared caches\n"
                       "\n"
                       "Make sure perf_model/dram/controllers_interleaving is a multiple of perf_model/l%d_cache/shared_cores\n",
                       Sim()->getCfg()->getInt("perf_model/cache/levels")
                      );
   if (m_tag_directory_present)
      LOG_ASSERT_ERROR(m_cache_cntlrs[m_last_level_cache]->isMasterCache() == true,
                       "Tag directories may only be at 'master' node of shared caches\n"
                       "\n"
                       "Make sure perf_model/dram_directory/interleaving is a multiple of perf_model/l%d_cache/shared_cores\n",
                       Sim()->getCfg()->getInt("perf_model/cache/levels")
                      );

   
   // The core id to use when sending messages to the directory (master node of the last-level cache)
   m_core_id_master = getCore()->getId() - getCore()->getId() % cache_parameters[m_last_level_cache].shared_cores;

   if (m_core_id_master == getCore()->getId())
   {
      UInt32 num_sets = cache_parameters[MemComponent::L1_DCACHE].num_sets;
      // With heterogeneous caches, or fancy hash functions, we can no longer be certain that operations
      // only have effect within a set as we see it. Turn of optimization...
      if (num_sets != (1UL << floorLog2(num_sets)))
         num_sets = 1;
      for(core_id_t core_id = 0; core_id < (core_id_t)Sim()->getConfig()->getApplicationCores(); ++core_id)
      {
         if (Sim()->getCfg()->getIntArray("perf_model/l1_dcache/cache_size", core_id) != cache_parameters[MemComponent::L1_DCACHE].size)
            num_sets = 1;
         if (Sim()->getCfg()->getStringArray("perf_model/l1_dcache/address_hash", core_id) != "mask")
            num_sets = 1;
         // FIXME: We really should check all cache levels
      }

      m_cache_cntlrs[(UInt32)m_last_level_cache]->createSetLocks(
         getCacheBlockSize(),
         num_sets,
         m_core_id_master,
         cache_parameters[m_last_level_cache].shared_cores
      );
      if (dram_direct_access && getCore()->getId() < (core_id_t)Sim()->getConfig()->getApplicationCores())
      {
         LOG_ASSERT_ERROR(Sim()->getConfig()->getApplicationCores() <= cache_parameters[m_last_level_cache].shared_cores, "DRAM direct access is only possible when there is just a single last-level cache (LLC level %d shared by %d, num cores %d)", m_last_level_cache, cache_parameters[m_last_level_cache].shared_cores, Sim()->getConfig()->getApplicationCores());
         LOG_ASSERT_ERROR(m_dram_cntlr != NULL, "I'm supposed to have direct access to a DRAM controller, but there isn't one at this node");
         m_cache_cntlrs[(UInt32)m_last_level_cache]->setDRAMDirectAccess(
            m_dram_cache ? (DramCntlrInterface*)m_dram_cache : (DramCntlrInterface*)m_dram_cntlr,
            Sim()->getCfg()->getInt("perf_model/llc/evict_buffers"));
      }
   }

   // Register Call-backs
   getNetwork()->registerCallback(SHARED_MEM_1, MemoryManagerNetworkCallback, this);

   // Set up core topology information
   getCore()->getTopologyInfo()->setup(smt_cores, cache_parameters[m_last_level_cache].shared_cores);

}

MemoryManager::~MemoryManager()
{
   
   UInt32 i;

   getNetwork()->unregisterCallback(SHARED_MEM_1);

   // Delete the Models

   if (m_itlb) delete m_itlb;
   if (m_dtlb) delete m_dtlb;
   if (m_stlb) delete m_stlb;

   for(i = MemComponent::FIRST_LEVEL_CACHE; i <= (UInt32)m_last_level_cache; ++i)
   {
      delete m_cache_perf_models[(MemComponent::component_t)i];
      m_cache_perf_models[(MemComponent::component_t)i] = NULL;
   }

   delete m_user_thread_sem;
   delete m_network_thread_sem;
   delete m_tag_directory_home_lookup;
   delete m_dram_controller_home_lookup;

   for(i = MemComponent::FIRST_LEVEL_CACHE; i <= (UInt32)m_last_level_cache; ++i)
   {
      delete m_cache_cntlrs[(MemComponent::component_t)i];
      m_cache_cntlrs[(MemComponent::component_t)i] = NULL;
   }
   
   if (m_nuca_cache)
      delete m_nuca_cache;
   if (m_dram_cache)
      delete m_dram_cache;
   if (m_dram_cntlr)
      delete m_dram_cntlr;
   if (m_dram_directory_cntlr)
      delete m_dram_directory_cntlr;
   
}

HitWhere::where_t
MemoryManager::coreInitiateMemoryAccess(
      IntPtr eip,
      MemComponent::component_t mem_component, 
      Core::lock_signal_t lock_signal,
      Core::mem_op_t mem_op_type,
      IntPtr address, UInt32 offset,
      Byte* data_buf, UInt32 data_length,
      Core::MemModeled modeled) 
{
   LOG_ASSERT_ERROR(mem_component <= m_last_level_cache,
      "Error: invalid mem_component (%d) for coreInitiateMemoryAccess", mem_component);

   
   

      if( getCore()->getInstructionCount() / 1000000  > current_nuca_stamp){

         current_nuca_stamp =  getCore()->getInstructionCount() / 1000000;
         measureNucaStats();
         getCache(MemComponent::component_t::L2_CACHE)->measureStats();
         getCache(MemComponent::component_t::L1_DCACHE)->measureStats();
         if(m_utopia_enabled){
            UTR *m_utr_4KB;
            Utopia *m_utopia;
            m_utopia = Sim()->getUtopia();
            m_utr_4KB = m_utopia->getUtr(0);
            m_utr_4KB->track_utilization();
         }
      }
   

   TranslationResult tr_result;

   if(!oracle_translation_enabled)
   {  
      
      tr_result = performAddressTranslation(eip, mem_component, 
                                 lock_signal, 
                                 mem_op_type, 
                                 address, 
                                 data_buf, 
                                 data_length, 
                                 modeled == Core::MEM_MODELED_NONE || modeled == Core::MEM_MODELED_COUNT ? false : true,
                                 modeled == Core::MEM_MODELED_NONE ? false : true);
   }
   
   if(!oracle_protection_enabled){
      if(modrian_memory_enabled){
         mmm->init_walk(eip, address, m_cache_cntlrs[MemComponent::L1_DCACHE] , lock_signal, data_buf, data_length, modeled == Core::MEM_MODELED_NONE || modeled == Core::MEM_MODELED_COUNT ? false : true,modeled == Core::MEM_MODELED_NONE ? false : true);
      }


   }

   if(!oracle_expressive_enabled){

      if(xmem_enabled){
         xmem_manager->init_walk(eip, address, m_cache_cntlrs[MemComponent::L1_DCACHE] , lock_signal, data_buf, data_length, modeled == Core::MEM_MODELED_NONE || modeled == Core::MEM_MODELED_COUNT ? false : true,modeled == Core::MEM_MODELED_NONE ? false : true);
      }
      
   }

   if(m_utopia_enabled){
      Utopia* utopia = Sim()->getUtopia();    
      std::unordered_map<IntPtr,SubsecondTime> *map;
      map = (utopia->getMigrationMap());         
      if(map->find(address >> 12) != map->end()){
         if(map->at(address>>12) > getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD) ){
            SubsecondTime delay = map->at(address>>12) - getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
            tr_result.latency+=delay;
            translation_stats.migrations_affected_request++;
            translation_stats.migrations_affected_latency+=delay;
            map->erase(address>>12);
         }
         else{
            map->erase(address>>12);
         }
      }
   }

   HitWhere::where_t result =m_cache_cntlrs[mem_component]->processMemOpFromCore(
         eip,
         lock_signal,
         mem_op_type,
         address, offset,
         data_buf, data_length,
         modeled == Core::MEM_MODELED_NONE || modeled == Core::MEM_MODELED_COUNT ? false : true,
         modeled == Core::MEM_MODELED_NONE ? false : true, CacheBlockInfo::block_type_t::NON_PAGE_TABLE, tr_result.latency, shadow_cache);

  // metadata_stats.metadata_hit[result]++;

   updateTranslationCounters(tr_result,result);
   return result;
}

 void MemoryManager::updateTranslationCounters(TranslationResult trResult, HitWhere::where_t dataResult){



   //printf("dataResult= %s and TranslationResult= %d \n",HitWhereString(dataResult),trResult.hitwhere);

   if      (dataResult == HitWhere::where_t::DRAM_LOCAL && trResult.hitwhere == TranslationHitWhere::TLB_HIT_L2) translation_stats.llc_miss_l2tlb_hit++;
   else if (dataResult == HitWhere::where_t::DRAM_LOCAL && trResult.hitwhere == TranslationHitWhere::TLB_HIT_L1) translation_stats.llc_miss_l1tlb_hit++;
   else if (dataResult == HitWhere::where_t::DRAM_LOCAL && trResult.hitwhere == TranslationHitWhere::TLB_MISS) translation_stats.llc_miss_l2tlb_miss++;
   else if (dataResult != HitWhere::where_t::DRAM_LOCAL && trResult.hitwhere == TranslationHitWhere::TLB_HIT_L1) translation_stats.llc_hit_l1tlb_hit++;
   else if (dataResult != HitWhere::where_t::DRAM_LOCAL && trResult.hitwhere == TranslationHitWhere::TLB_HIT_L2) translation_stats.llc_hit_l2tlb_hit++;
   else if (dataResult != HitWhere::where_t::DRAM_LOCAL && trResult.hitwhere == TranslationHitWhere::TLB_MISS) translation_stats.llc_hit_l2tlb_miss++;

}

TranslationResult MemoryManager::performAddressTranslation(
            IntPtr eip,
            MemComponent::component_t mem_component,
            Core::lock_signal_t lock_signal,
            Core::mem_op_t mem_op_type,
            IntPtr address,
            Byte *data_buf, UInt32 data_length,
            bool modeled, bool count)
{
   TranslationResult tlb_result;
   TranslationResult utopia_result;

   SubsecondTime total_translation_latency = SubsecondTime::Zero();
   

   if(!m_rlb_enabled){
            
            SubsecondTime t_begin =  getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);

            if (mem_component == MemComponent::L1_ICACHE && m_itlb){
               
                  tlb_result = accessTLBSubsystem(eip, m_itlb, address, true,  modeled, count, lock_signal, data_buf, data_length);
                  total_translation_latency = tlb_result.latency; 
            }
            else if (mem_component == MemComponent::L1_DCACHE && m_dtlb)
                  tlb_result = accessTLBSubsystem(eip, m_dtlb, address, false, modeled, count, lock_signal, data_buf, data_length); // TLBSubsystem latency in tlb_hit_location


            if(m_utopia_enabled && (!m_virtualized) && mem_component == MemComponent::L1_DCACHE)
            {

                  if(tlb_result.hitwhere == TranslationHitWhere::UTR_HIT){ // Take into account only Utopia-latency
                     utopia_result = accessUtopiaSubsystem(eip, lock_signal, address, data_buf, data_length, modeled, count);
                    // getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_USER_THREAD, t_begin + utopia_result.latency); 
                     translation_stats.utr_hit++;
                     translation_stats.utr_hit_latency += utopia_result.latency;  
                     total_translation_latency = utopia_result.latency;
                     if(Sim()->getUtopia()->getSerialL2TLB() && utopia_result.hitwhere!=TranslationHitWhere::ULB_HIT){
                        translation_stats.utr_hit_latency +=m_tlb_l2_access_penalty.getLatency();
                        total_translation_latency+=m_tlb_l2_access_penalty.getLatency();
                     }
                  }  
                  else if(tlb_result.hitwhere == TranslationHitWhere::TLB_HIT_L1){ //Take into account only TLB latency

                   //  getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_USER_THREAD, t_begin + tlb_result.latency); 
                     translation_stats.tlb_hit_l1++;
                     translation_stats.tlb_hit++;
                     translation_stats.tlb_hit_latency += tlb_result.latency;
                     total_translation_latency = tlb_result.latency; 

                  }  
                  else if(tlb_result.hitwhere == TranslationHitWhere::TLB_HIT_L2){ //Take into account only TLB latency

                   //  getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_USER_THREAD, t_begin + tlb_result.latency); 
                     translation_stats.tlb_hit_l2++;
                     translation_stats.tlb_hit_l2_latency += tlb_result.latency;
                     translation_stats.tlb_hit++;
                     translation_stats.tlb_hit_latency += tlb_result.latency;
                     total_translation_latency = tlb_result.latency; 

                  }           
                  else if(tlb_result.hitwhere == TranslationHitWhere::TLB_MISS && tlb_result.hitwhere != TranslationHitWhere::UTR_HIT){ // TLB Miss needs to wait for UTR miss 
                     utopia_result = accessUtopiaSubsystem(eip, lock_signal, address, data_buf, data_length, modeled, count);
                     if(utopia_result.latency > tlb_result.latency) tlb_result.latency += utopia_result.latency - tlb_result.latency;
                     
                     translation_stats.tlb_and_utr_miss++;
                     translation_stats.tlb_and_utr_miss_latency += tlb_result.latency;
                     total_translation_latency = tlb_result.latency; 

                  }

            }
            else if (mem_component == MemComponent::L1_DCACHE) // Utopia is disabled, take into account only tlb latency
            {
                     total_translation_latency = tlb_result.latency; 
            	      assert(!m_utopia_enabled);
	    
	         }
           // std::cout<<"Utopia translation latency : "<<utopia_result.latency<<"\n"<<"Tlb translation latency : "<<tlb_result.latency<<"\n"<<"Total latency : "<<total_translation_latency<<"\n";
            translation_stats.total_latency += total_translation_latency;

   }

   TranslationResult result;
   result.hitwhere = tlb_result.hitwhere;
   result.latency = total_translation_latency;
   return result;

}

TranslationResult MemoryManager::accessTLBSubsystem(IntPtr eip, TLB * tlb, IntPtr address, bool isIfetch,bool modeled, bool count, Core::lock_signal_t lock_signal, Byte* data_buf, UInt32 data_length)
{
   
   TLB::where_t hit = tlb->lookup(address, getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD), true, 1, count, lock_signal);
   
   TranslationResult result;
   result.hitwhere = TranslationHitWhere::TLB_MISS;
   result.latency = SubsecondTime::Zero();
   
   if (hit == TLB::where_t::MISS && modeled ) //TLB Miss: Perform Page Table walk
   {
          if(radix_enabled || ptw_cuckoo_enabled || hash_dont_cache_enabled || hash_baseline_enabled || m_virtualized){
              
            SubsecondTime ptw_contention_latency = SubsecondTime::Zero();
            SubsecondTime t_ptw_begin = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
            SubsecondTime t_ptw_avail = t_ptw_begin;
            t_ptw_avail = ptw_srs->getStartTime(t_ptw_begin);
            ptw_contention_latency  = t_ptw_avail - t_ptw_begin;
                 
               
            SubsecondTime ptw_latency=  ptw->init_walk(eip, address, (shadow_cache_enabled ? shadow_cache : NULL), m_cache_cntlrs[MemComponent::L1_DCACHE] , lock_signal, data_buf, data_length, modeled,count);


            ptw_srs->getCompletionTime(t_ptw_begin, ptw_contention_latency+ptw_latency, address);
              
              translation_stats.ptw_contention += ptw_contention_latency;

              #ifdef PTW_DEBUG
               //if (ptw_contention_latency != SubsecondTime::Zero() )
               //   std::cout<<"Works with latency ptw_latency : " << ptw_contention_latency << "\n";
              #endif  

              result.hitwhere = TranslationHitWhere::TLB_MISS;

             if(m_utopia_enabled) //If Utopia is not serialized - then ptw is also parallel with L2 TLB 
               result.latency = ptw_latency + m_tlb_l1_access_penalty.getLatency(); 
              else if(m_potm_enabled && (Sim()->getCfg()->getInt("perf_model/potm_tlb/latency")) > 0) //if POM-TLB Latency is static: acts like L3 TLB
               result.latency = ptw_latency + m_tlb_l1_access_penalty.getLatency() + m_tlb_l2_access_penalty.getLatency();
              else if(m_potm_enabled) //if POM-TLB latency is not static: act as software TLB
               result.latency = ptw_latency + m_tlb_l1_access_penalty.getLatency() + m_tlb_l2_access_penalty.getLatency()+m_stlb->getPOTMlookupTime();
              else if (m_parallel_walk)
               result.latency = ptw_latency + m_tlb_l1_access_penalty.getLatency();
              else if(!(m_parallel_walk)){
               result.latency = ptw_latency + m_tlb_l1_access_penalty.getLatency() +m_tlb_l2_access_penalty.getLatency();
              }
              return result;
          }
          else{
            
            result.hitwhere = TranslationHitWhere::TLB_MISS;
            result.latency = m_tlb_miss_penalty.getLatency()+m_tlb_l1_access_penalty.getLatency()+m_tlb_l2_access_penalty.getLatency();
            return result;

          }
   }
   else if (modeled && hit != TLB::where_t::UTR_HIT){ // We need to be out of warmup to account for latency

      if(hit == TLB::where_t::L1){
         result.latency = m_tlb_l1_access_penalty.getLatency();
         translation_stats.tlb_hit_l1_latency += result.latency;
         result.hitwhere = TranslationHitWhere::TLB_HIT_L1;
      }
      if(hit == TLB::where_t::L2){
         result.latency = m_tlb_l1_access_penalty.getLatency() + m_tlb_l2_access_penalty.getLatency();
         translation_stats.tlb_hit_l2_latency += result.latency;
         result.hitwhere = TranslationHitWhere::TLB_HIT_L2;

      }
      if(hit == TLB::where_t::POTM){
         if( Sim()->getCfg()->getInt("perf_model/potm_tlb/latency") > 0 ){
            result.latency = m_tlb_l1_access_penalty.getLatency() + m_tlb_l2_access_penalty.getLatency()+potm_latency.getLatency();
         }
         else 
            result.latency = m_tlb_l1_access_penalty.getLatency() + m_tlb_l2_access_penalty.getLatency()+m_stlb->getPOTMlookupTime() ;

         result.hitwhere = TranslationHitWhere::TLB_POTM_HIT;
      }
      if(hit == TLB::where_t::L1_CACHE){
         result.latency = m_tlb_l1_access_penalty.getLatency()+ m_tlb_l1_cache_access.getLatency(); //parallel access to L2 and victima 
         result.hitwhere = TranslationHitWhere::TLB_HIT_CACHE_L1;
         translation_stats.l1c_hit_tlb_latency += m_tlb_l1_cache_access.getLatency();
         translation_stats.l1c_hit_tlb++;
      }
      if(hit == TLB::where_t::L2_CACHE){
         result.latency = m_tlb_l1_access_penalty.getLatency() + m_tlb_l2_cache_access.getLatency();
         result.hitwhere = TranslationHitWhere::TLB_HIT_CACHE_L2;
         translation_stats.l2c_hit_tlb_latency += m_tlb_l2_cache_access.getLatency();
         translation_stats.l2c_hit_tlb++;

      }
      if(hit == TLB::where_t::NUCA_CACHE){
         result.latency = m_tlb_l1_access_penalty.getLatency() + m_tlb_nuca_cache_access.getLatency();
         result.hitwhere = TranslationHitWhere::TLB_HIT_CACHE_NUCA;
         translation_stats.nucac_hit_tlb_latency += m_tlb_nuca_cache_access.getLatency();
         translation_stats.nucac_hit_tlb++;

      }

      return result; // TLB Hit: Return Access Latency

   }
   else if (modeled && hit == TLB::where_t::UTR_HIT){
      result.hitwhere = TranslationHitWhere::UTR_HIT;
   }

   return result;
}

TranslationResult MemoryManager::accessUtopiaSubsystem(
            IntPtr eip,
            Core::lock_signal_t lock_signal,
            IntPtr address,
            Byte *data_buf, UInt32 data_length,
            bool modeled,
            bool count)
{

   TranslationResult utopia_result;
   TranslationHitWhere utr_hit_result=TranslationHitWhere::UTR_MISS;
   
   utopia_result.hitwhere = TranslationHitWhere::UTR_MISS;

   Utopia *m_utopia;
   UTR* m_utr;
   bool in_utr;

   m_utopia = Sim()->getUtopia();

   SubsecondTime now = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);

   SubsecondTime max = SubsecondTime::Zero() ;
   SubsecondTime max_miss = SubsecondTime::Zero() ;
   //std::cout << "Start Utopia request with address:" << address <<   std::endl;

      ULB::where_t ulb_hitwhere;
      

      ulb_hitwhere = ulb->lookup(address, max,true,count); // Allocate on miss only if the page resides in a UTR

      bool utopia_walk_skip = false;
      bool tag_match_skip = true;

      if(ulb_hitwhere == ULB::where_t::HIT){

         utopia_walk_skip = true; // Access ULB and the Utopia Walk is skipped
         utopia_result.latency = ulb->access_latency.getLatency();
         utopia_result.hitwhere = TranslationHitWhere::ULB_HIT;
         return utopia_result;
      }
      else{
       //  std::cout << "ULB miss for address" << address << "with page size" << m_utr->getPageSize() << std::endl;
      }


   for (int i=0; i < m_utopia->utrs; i++ ){ //@kanellok iterate on top of all UTRs



      m_utr = m_utopia->getUtr(i);
      utopia_result.latency = SubsecondTime::Zero();

      SubsecondTime t_start= getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);

      
      int vpn  = address >> m_utr->getPageSize();


      
      if(m_utr->inUTRnostats(address,count,getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD),getCore()->getId())){
               in_utr = true;
               utopia_result.hitwhere = TranslationHitWhere::UTR_HIT;
      }
      else{
         
            in_utr = false;
            utopia_result.hitwhere = TranslationHitWhere::UTR_MISS;

      }



      if(m_utr->permission_filter(address,getCore()->getId())) tag_match_skip = true;
      else tag_match_skip = false;



      if(utopia_walk_skip == false){

               UInt64 permission_filter = m_utr->calculate_permission_address(address,getCore()->getId()); 
               UInt64* aligned_perm_address = (UInt64*)(reinterpret_cast<UInt64>(permission_filter) & (~((UInt64)getCacheBlockSize() - 1))); 

               // Access Permissions

      
               UtopiaCache::where_t permcache_hitwhere;

               permcache_hitwhere = utopia_perm_cache->lookup((IntPtr) permission_filter  , t_start, true, count); // Lookup the Utopia Permission Cache

               if(permcache_hitwhere == UtopiaCache::where_t::MISS){
                     
                     SubsecondTime t_start_2 = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);

                     HitWhere::where_t res = m_cache_cntlrs[MemComponent::L1_DCACHE]->processMemOpFromCore(
                     eip,
                     lock_signal,
                     Core::mem_op_t::READ,
                     (IntPtr) aligned_perm_address, 0,
                     data_buf, data_length,
                     modeled,count, CacheBlockInfo::block_type_t::UTOPIA, SubsecondTime::Zero());

                     if(m_nuca_cache){
                           m_nuca_cache->markTranslationMetadata(permission_filter,CacheBlockInfo::block_type_t::UTOPIA);
                     }


                     SubsecondTime t_end = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
                     SubsecondTime total_latency = std::max(t_end - t_start_2 +utopia_perm_cache->miss_latency.getLatency(), ulb->miss_latency.getLatency());
                     getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_USER_THREAD,now);
                     utopia_result.latency += total_latency;
               }
               else{
                     utopia_result.latency += std::max(utopia_perm_cache->access_latency.getLatency(), ulb->miss_latency.getLatency());
               }

               if(tag_match_skip == false){

                  UInt64 tag = m_utr->calculate_tag_address(address,getCore()->getId());
                  UInt64* aligned_tag_address =  (UInt64*)(reinterpret_cast<UInt64>(tag) & (~((UInt64)getCacheBlockSize() - 1)));
                  
                  UtopiaCache::where_t tagcache_hitwhere;

                  tagcache_hitwhere = utopia_tag_cache->lookup((IntPtr) tag , t_start, true, count); // Lookup the Utopia Permission Cache

                  if(tagcache_hitwhere == UtopiaCache::where_t::MISS){
                        
                        //std::cout << "Tag miss, let's access the cache" << std::endl;
                        SubsecondTime t_start_2 = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);

                        HitWhere::where_t res = m_cache_cntlrs[MemComponent::L1_DCACHE]->processMemOpFromCore(
                        eip,
                        lock_signal,
                        Core::mem_op_t::READ,
                        (IntPtr) aligned_tag_address, 0,
                        data_buf, data_length,
                        modeled,
                        count, CacheBlockInfo::block_type_t::UTOPIA, SubsecondTime::Zero());

                        if(m_nuca_cache){
                           m_nuca_cache->markTranslationMetadata(tag,CacheBlockInfo::block_type_t::UTOPIA);
                        }

                        SubsecondTime t_end = getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD);
                        SubsecondTime total_latency = t_end - t_start_2 +utopia_tag_cache->miss_latency.getLatency();
                        getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_USER_THREAD,now);
                        utopia_result.latency += total_latency;
                  }
                  else{
                        utopia_result.latency += utopia_tag_cache->access_latency.getLatency();
                  }

               }
               
               if(utopia_result.hitwhere == TranslationHitWhere::UTR_HIT){
                  
                 // std::cout<<" Utopia translation latency result after ulb miss: " << utopia_result.latency << "\n";
                  utr_hit_result = TranslationHitWhere::UTR_HIT;
                  max = utopia_result.latency;
               }

               if(max_miss < utopia_result.latency ){
                  //std::cout<<" Utopia translation latency result after ulb miss: " << utopia_result.latency << "\n";
                  max_miss = utopia_result.latency;
               }




      }

      
      if(!modeled)
            utopia_result.latency = SubsecondTime::Zero();
   
   }

   if( utr_hit_result ==  TranslationHitWhere::UTR_HIT)
      utopia_result.latency = max;
   else
      utopia_result.latency = max_miss;

   utopia_result.hitwhere = utr_hit_result;

   //std::cout<<"Utopia translation latency : "<<utopia_result.latency << "\n";

   return utopia_result;
}

void MemoryManager::handleMsgFromNetwork(NetPacket& packet)
{  
   MYLOG("begin");
   core_id_t sender = packet.sender;
   PrL1PrL2DramDirectoryMSI::ShmemMsg* shmem_msg = PrL1PrL2DramDirectoryMSI::ShmemMsg::getShmemMsg((Byte*) packet.data, &m_dummy_shmem_perf);
   SubsecondTime msg_time = packet.time;

   getShmemPerfModel()->setElapsedTime(ShmemPerfModel::_SIM_THREAD, msg_time);
   shmem_msg->getPerf()->updatePacket(packet);  
   
   
   MemComponent::component_t receiver_mem_component = shmem_msg->getReceiverMemComponent();
   MemComponent::component_t sender_mem_component = shmem_msg->getSenderMemComponent();

   if (m_enabled)
   {
      LOG_PRINT("Got Shmem Msg: type(%i), address(0x%x), sender_mem_component(%u), receiver_mem_component(%u), sender(%i), receiver(%i)",
            shmem_msg->getMsgType(), shmem_msg->getAddress(), sender_mem_component, receiver_mem_component, sender, packet.receiver);
   }

   switch (receiver_mem_component)
   {
      case MemComponent::L2_CACHE: /* PrL1PrL2DramDirectoryMSI::DramCntlr sends to L2 and doesn't know about our other levels */
      case MemComponent::LAST_LEVEL_CACHE:
         switch(sender_mem_component)
         {
            case MemComponent::TAG_DIR:
               m_cache_cntlrs[m_last_level_cache]->handleMsgFromDramDirectory(sender, shmem_msg);
               break;

            default:
               LOG_PRINT_ERROR("Unrecognized sender component(%u)",
                     sender_mem_component);
               break;
         }
         break;

      case MemComponent::TAG_DIR:
         LOG_ASSERT_ERROR(m_tag_directory_present, "Tag directory NOT present");

         switch(sender_mem_component)
         {
            case MemComponent::LAST_LEVEL_CACHE:
               m_dram_directory_cntlr->handleMsgFromL2Cache(sender, shmem_msg);
               break;

            case MemComponent::DRAM:
               m_dram_directory_cntlr->handleMsgFromDRAM(sender, shmem_msg);
               break;

            default:
               LOG_PRINT_ERROR("Unrecognized sender component(%u)",
                     sender_mem_component);
               break;
         }
         break;

      case MemComponent::DRAM:
         LOG_ASSERT_ERROR(m_dram_cntlr_present, "Dram Cntlr NOT present");
         
         switch(sender_mem_component)
         {
            case MemComponent::TAG_DIR:
            {
               DramCntlrInterface* dram_interface = m_dram_cache ? (DramCntlrInterface*)m_dram_cache : (DramCntlrInterface*)m_dram_cntlr;
               dram_interface->handleMsgFromTagDirectory(sender, shmem_msg);
               break;
            }

            default:
               LOG_PRINT_ERROR("Unrecognized sender component(%u)",
                     sender_mem_component);
               break;
         }
         break;

      default:
         LOG_PRINT_ERROR("Unrecognized receiver component(%u)",
               receiver_mem_component);
         break;
   }

   // Delete the allocated Shared Memory Message
   // First delete 'data_buf' if it is present
   // LOG_PRINT("Finished handling Shmem Msg");

   if (shmem_msg->getDataLength() > 0)
   {
      assert(shmem_msg->getDataBuf());
      delete [] shmem_msg->getDataBuf();
   }
   delete shmem_msg;
MYLOG("end");
}

void
MemoryManager::sendMsg(PrL1PrL2DramDirectoryMSI::ShmemMsg::msg_t msg_type, MemComponent::component_t sender_mem_component, MemComponent::component_t receiver_mem_component, core_id_t requester, core_id_t receiver, IntPtr address, Byte* data_buf, UInt32 data_length, HitWhere::where_t where, ShmemPerf *perf, ShmemPerfModel::Thread_t thread_num, CacheBlockInfo::block_type_t block_type)
{
   //std::cout<<"Address: "<<address<<"\n";
MYLOG("send msg %u %ul%u > %ul%u", msg_type, requester, sender_mem_component, receiver, receiver_mem_component);
   assert((data_buf == NULL) == (data_length == 0));
   PrL1PrL2DramDirectoryMSI::ShmemMsg shmem_msg(msg_type, sender_mem_component, receiver_mem_component, requester, address, data_buf, data_length, perf, block_type);
   shmem_msg.setWhere(where);

   Byte* msg_buf = shmem_msg.makeMsgBuf();
   SubsecondTime msg_time = getShmemPerfModel()->getElapsedTime(thread_num);
   perf->updateTime(msg_time);

   if (m_enabled)
   {
      LOG_PRINT("Sending Msg: type(%u), address(0x%x), sender_mem_component(%u), receiver_mem_component(%u), requester(%i), sender(%i), receiver(%i)", msg_type, address, sender_mem_component, receiver_mem_component, requester, getCore()->getId(), receiver);
   }

   NetPacket packet(msg_time, SHARED_MEM_1,
         m_core_id_master, receiver,
         shmem_msg.getMsgLen(), (const void*) msg_buf);
   getNetwork()->netSend(packet);

   // Delete the Msg Buf
   delete [] msg_buf;
}

void
MemoryManager::broadcastMsg(PrL1PrL2DramDirectoryMSI::ShmemMsg::msg_t msg_type, MemComponent::component_t sender_mem_component, MemComponent::component_t receiver_mem_component, core_id_t requester, IntPtr address, Byte* data_buf, UInt32 data_length, ShmemPerf *perf, ShmemPerfModel::Thread_t thread_num)
{
MYLOG("bcast msg");
   assert((data_buf == NULL) == (data_length == 0));
   PrL1PrL2DramDirectoryMSI::ShmemMsg shmem_msg(msg_type, sender_mem_component, receiver_mem_component, requester, address, data_buf, data_length, perf,CacheBlockInfo::block_type_t::NON_PAGE_TABLE);

   Byte* msg_buf = shmem_msg.makeMsgBuf();
   SubsecondTime msg_time = getShmemPerfModel()->getElapsedTime(thread_num);
   perf->updateTime(msg_time);

   if (m_enabled)
   {
      LOG_PRINT("Sending Msg: type(%u), address(0x%x), sender_mem_component(%u), receiver_mem_component(%u), requester(%i), sender(%i), receiver(%i)", msg_type, address, sender_mem_component, receiver_mem_component, requester, getCore()->getId(), NetPacket::BROADCAST);
   }

   NetPacket packet(msg_time, SHARED_MEM_1,
         m_core_id_master, NetPacket::BROADCAST,
         shmem_msg.getMsgLen(), (const void*) msg_buf);
   getNetwork()->netSend(packet);

   // Delete the Msg Buf
   delete [] msg_buf;
}



SubsecondTime
MemoryManager::getCost(MemComponent::component_t mem_component, CachePerfModel::CacheAccess_t access_type)
{
   if (mem_component == MemComponent::INVALID_MEM_COMPONENT)
      return SubsecondTime::Zero();

   return m_cache_perf_models[mem_component]->getLatency(access_type);
}

void
MemoryManager::incrElapsedTime(SubsecondTime latency, ShmemPerfModel::Thread_t thread_num)
{
   MYLOG("cycles += %s", itostr(latency).c_str());
   getShmemPerfModel()->incrElapsedTime(latency, thread_num);
}

void
MemoryManager::incrElapsedTime(MemComponent::component_t mem_component, CachePerfModel::CacheAccess_t access_type, ShmemPerfModel::Thread_t thread_num)
{
   incrElapsedTime(getCost(mem_component, access_type), thread_num);
}

void
MemoryManager::enableModels()
{
   m_enabled = true;

   for(UInt32 i = MemComponent::FIRST_LEVEL_CACHE; i <= (UInt32)m_last_level_cache; ++i)
   {
      m_cache_cntlrs[(MemComponent::component_t)i]->enable();
      m_cache_perf_models[(MemComponent::component_t)i]->enable();
   }

   if (m_dram_cntlr_present)
      m_dram_cntlr->getDramPerfModel()->enable();
}

void
MemoryManager::disableModels()
{
   m_enabled = false;

   for(UInt32 i = MemComponent::FIRST_LEVEL_CACHE; i <= (UInt32)m_last_level_cache; ++i)
   {
      m_cache_cntlrs[(MemComponent::component_t)i]->disable();
      m_cache_perf_models[(MemComponent::component_t)i]->disable();
   }

   if (m_dram_cntlr_present)
      m_dram_cntlr->getDramPerfModel()->disable();
}

void
MemoryManager::measureNucaStats()
{
   m_nuca_cache->measureStats();
}

}
  
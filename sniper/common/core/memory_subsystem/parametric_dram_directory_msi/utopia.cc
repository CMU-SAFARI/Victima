#include "pwc.h"
#include "stats.h"
#include "config.hpp"
#include "simulator.h"
#include <cmath>
#include <iostream>
#include <utility>
#include "core_manager.h"
#include "cache_set.h"
#include "utopia.h"
#include "utils.h"
#include "hash_map_set.h"


namespace ParametricDramDirectoryMSI{
         
  UTR::UTR(int _id, int _size, int _page_size, int _assoc, String _repl, String _hash, Utopia *_utopia):
       id(_id),
       size(_size),
       page_size(_page_size),
       assoc(_assoc),
       hash(_hash),
       repl(_repl),
       m_utr_conflicts(0),
       m_utr_accesses(0),
       m_utr_hits(0),
       utopia(_utopia)
  {

    

    utr_lock = new Lock();

 

    num_sets = k_MEGA * size / (assoc * (1 << page_size));

    std::cout << "[MMU] Creating UTR with sets : " << num_sets << " - page_size: " << page_size << " - assoc: " << assoc << std::endl ;

    //LOG_ASSERT_ERROR( k_MEGA * size / (assoc * (1 << page_size)), "Invalid utopia  configuration: size(%d MB) != sets(%d) * associativity(%d) * block_size(%d)", size, num_sets, assoc, (1 << page_size);
    m_utr_cache = new Cache(("utr_cache_"+std::to_string(id)).c_str(), "perf_model/utopia/utr", 0, num_sets, assoc, (1L << page_size), repl, CacheBase::PR_L1_CACHE,CacheBase::parseAddressHash(hash));

    int permission_size = (num_sets*log2(assoc))/8;
    int tag_size = (48-page_size-ceil(log2(num_sets))+3);

    int core_num;
	  core_num = Config::getSingleton()->getTotalCores();

    for ( int i =0; i < core_num; i++){
      
      // Get an address from the mem allocator
  
      

      char *utr_per_base = (char*) malloc ((num_sets*log2(assoc))/8); 

      IntPtr permission_bounds  = (IntPtr)utr_per_base + permission_size;

      std::cout << "[MMU:UTR:CoreID-" << i << "]" << "Permission base: " << static_cast<void*>(utr_per_base) << " Size(KB): " << permission_size/1024 <<  std::endl ;

        
    
      char* utr_tags_base = (char*) malloc(num_sets*(tag_size/8));
      
      std::cout << "[MMU:UTR:CoreID-" << i << "]" << " Tag Base: " << static_cast<void*>(utr_tags_base) <<std::endl ;

      std::cout << "[MMU:UTR:CoreID-" << i << "]" << " Metadata Tag Size (KB): " <<  num_sets*(tag_size/8)/1024 << std::endl ;

      int core_num = Config::getSingleton()->getTotalCores();


        

      permissions.push_back((IntPtr) utr_per_base);
      tags.push_back((IntPtr) utr_tags_base);

        
    }



    registerStatsMetric(("utr_"+std::to_string(id)).c_str(), 0, "allocation_conflicts", &m_utr_conflicts);
    registerStatsMetric(("utr_"+std::to_string(id)).c_str(), 0, "hits", &m_utr_hits);
    registerStatsMetric(("utr_"+std::to_string(id)).c_str(), 0, "accesses", &m_utr_accesses);
    registerStatsMetric(("utr_"+std::to_string(id)).c_str(), 0, "allocations", &m_allocations);
    registerStatsMetric(("utr_"+std::to_string(id)).c_str(), 0, "utr_utilization", &m_utr_conflicts,true);



  
  }
    

  bool UTR::inUTR(IntPtr address, bool count, SubsecondTime now,int core_id){

    utr_lock->acquire();

    m_utr_accesses++;
    bool hit = m_utr_cache->accessSingleLine(address, Cache::LOAD, NULL, 0, now, true);

    IntPtr tag;
    UInt32 set_index;
    m_utr_cache->splitAddress(address, tag, set_index);

    bool owner_hit = false;
    for (int i=0; i < m_utr_cache->getCacheSet(set_index)->getAssociativity(); i++){
            
                if( (m_utr_cache->peekBlock(set_index,i)->getTag() == tag) && (m_utr_cache->peekBlock(set_index,i)->getOwner() == (core_id+1))){
                   m_utr_cache->peekBlock(set_index,i)->increaseReuse();
                   owner_hit = true;
                   break;
                }
                 
          }
    
    if (count && hit && owner_hit) m_utr_hits++;

    utr_lock->release();

    return  (owner_hit);


  }

  bool UTR::inUTRnostats(IntPtr address, bool count, SubsecondTime now, int core_id){

    utr_lock->acquire();
    bool hit = m_utr_cache->accessSingleLine(address, Cache::LOAD, NULL, 0, now, true);

    IntPtr tag;
    UInt32 set_index;
    m_utr_cache->splitAddress(address, tag, set_index);

    //if(hit == true) std::cout <<"Hit in tag: " << tag << " for core id" << core_id << std::endl;
    bool owner_hit = false;
    //std::cout << "Starting search for owner" << std::endl;
    for (int i=0; i < m_utr_cache->getCacheSet(set_index)->getAssociativity(); i++){
                //std::cout << "Tag of set: " << set_index << " is " << m_utr_cache->peekBlock(set_index,i)->getTag() << std::endl;
                //if(m_utr_cache->peekBlock(set_index,i)->getTag() == tag)
                  //std::cout <<"Hit in tag: " << tag <<  " The owner of block with tag : " << m_utr_cache->peekBlock(set_index,i)->getTag() << " is " <<  m_utr_cache->peekBlock(set_index,i)->getOwner() << std::endl;
                if((m_utr_cache->peekBlock(set_index,i)->getTag() == tag) && (m_utr_cache->peekBlock(set_index,i)->getOwner() == (core_id+1))){
                 // std::cout <<"Hit in tag" << tag <<  " The owner of block with tag : " << m_utr_cache->peekBlock(set_index,i)->getTag() << " is " <<  m_utr_cache->peekBlock(set_index,i)->getOwner() << std::endl;
                   owner_hit = true;
                   break;
                }
                 
    }
    

    utr_lock->release();


    return ( owner_hit);

  }

  IntPtr UTR::calculate_permission_address(IntPtr address, int core_id){

          IntPtr tag;
          UInt32 set_index;
          m_utr_cache->splitAddress(address, tag, set_index);
          return (IntPtr)(permissions[core_id] + (set_index * (int)(log2(assoc)))/8);

  }

  IntPtr UTR::calculate_tag_address(IntPtr address, int core_id){

          IntPtr tag;
          UInt32 set_index;
          m_utr_cache->splitAddress(address, tag, set_index);
          //std::cout << "Accessing tag " <<  tags[core_id]+set_index*assoc*(metadata_size/8) << std::endl;
          return (IntPtr)(tags[core_id]+set_index*assoc*(tag_size/8)); // We assume 4byte tags including read/write permissions + PCID
  
  }

  bool UTR::permission_filter(IntPtr address, int core_id ){

     utr_lock->acquire();

          IntPtr tag;
          UInt32 set_index;
          bool set_is_empty = true;

          m_utr_cache->splitAddress(address, tag, set_index);

          for (int i=0; i < m_utr_cache->getCacheSet(set_index)->getAssociativity(); i++){

              // Check if set is empty from blocks 
              if(m_utr_cache->getCacheSet(set_index)->peekBlock(i)->isValid() && m_utr_cache->getCacheSet(set_index)->peekBlock(i)->getOwner() == (core_id +1) ){ 
                  set_is_empty = false;
              }
          }

      utr_lock->release();

      return set_is_empty;
    
    


  }

  void UTR::allocate(IntPtr address,  SubsecondTime now, int core_id){
         
          utr_lock->acquire();

          IntPtr tag;
          UInt32 set_index;
          bool eviction;
          IntPtr evict_addr;
          CacheBlockInfo evict_block_info;


          m_allocations++;
          
          m_utr_cache->splitAddress(address, tag, set_index);
          m_utr_cache->insertSingleLine(address, NULL, &eviction, &evict_addr, &evict_block_info, NULL, now);
          
         // std::cout << "Adding address with set_index: " << set_index << " and tag: " << tag << std::endl;

          for (int i=0; i < m_utr_cache->getCacheSet(set_index)->getAssociativity(); i++){
            
                if(m_utr_cache->peekBlock(set_index,i)->getTag() == tag && m_utr_cache->peekBlock(set_index,i)->getOwner() == 0 ){
                  //std::cout << "Found where we inserted the block in UTR" << std::endl;
                  m_utr_cache->peekBlock(set_index,i)->setOwner(core_id+1); // Set core_id as owner of the page to track the permission_filter correctly
                //  std::cout << "Set the owner to: " <<  m_utr_cache->peekBlock(set_index,i)->getOwner() << std::endl;

                  break;
                }
                 
          }

          if(eviction){
             IntPtr evict_addr_vpn = evict_addr >> page_size;
             ComponentLatency latency_migration(Sim()->getCoreManager()->getCoreFromID(core_id)->getDvfsDomain(), Sim()->getCfg()->getInt("perf_model/utopia/migration_latency"));

             std::unordered_map<IntPtr,SubsecondTime> *map;
             map = (utopia->getMigrationMap());
             if(map->find(evict_addr_vpn) == map->end())
                map->insert({evict_addr_vpn,Sim()->getCoreManager()->getCoreFromID(core_id)->getShmemPerfModel()->getElapsedTime(ShmemPerfModel::_USER_THREAD)  +latency_migration.getLatency()});
             m_utr_conflicts++;
          }

          utr_lock->release();

          return;

  }

  void UTR::track_utilization(){
    
    utr_lock->acquire();

    int accum = 0;

    for(uint32_t set_index = 0; set_index < m_utr_cache->getNumSets(); ++set_index)
    {
      for (int i=0; i < m_utr_cache->getCacheSet(set_index)->getAssociativity(); i++){
       
       if(m_utr_cache->getCacheSet(set_index)->peekBlock(i)->isValid()) accum+=1;
          
      }
    }

    utilization.push_back(accum);

    printf("UTR utilization = %d, %d, %d \n", accum, m_utr_cache->getNumSets(),m_utr_cache->getAssociativity());

    utr_lock->release();

  }


Utopia::Utopia(){

      page_faults = 0;
      shadow_mode_enabled = Sim()->getCfg()->getBool("perf_model/utopia/shadow_mode_enabled");
      serial_l2_tlb = Sim()->getCfg()->getBool("perf_model/utopia/serial_l2ltb");
      migration_map = new std::unordered_map<IntPtr, SubsecondTime>();

      if(shadow_mode_enabled){

        int utr_size = Sim()->getCfg()->getInt("perf_model/utopia/shadow_utr/size");
        int utr_page_size = Sim()->getCfg()->getInt("perf_model/utopia/shadow_utr/page_size");
        int utr_assoc= Sim()->getCfg()->getInt("perf_model/utopia/shadow_utr/assoc");
        String utr_repl= Sim()->getCfg()->getString("perf_model/utopia/shadow_utr/repl");
        String utr_hash= Sim()->getCfg()->getString("perf_model/utopia/shadow_utr/hash");
        shadow_utr = new UTR(-1, utr_size, utr_page_size, utr_assoc, utr_repl, utr_hash, this);
        registerStatsMetric("utopia", 0, "page_faults", &page_faults);

        return;
      }



      utrs = Sim()->getCfg()->getInt("perf_model/utopia/utrs");
      


      heur_type_primary = (Utopia::utopia_heuristic) Sim()->getCfg()->getInt("perf_model/utopia/heuristic_primary");
      heur_type_secondary = (Utopia::utopia_heuristic) Sim()->getCfg()->getInt("perf_model/utopia/heuristic_secondary");

      std::cout << "[MMU:UTR] Heuristic primary  " << heur_type_primary << std::endl ;
      std::cout << "[MMU:UTR] Heuristic secondary  " << heur_type_secondary << std::endl ;


      tlb_eviction_thr = (UInt32)(Sim()->getCfg()->getInt("perf_model/utopia/tlb_eviction_thr"));
      pte_eviction_thr = (UInt32)(Sim()->getCfg()->getInt("perf_model/utopia/pte_eviction_thr"));

      std::cout << "[MMU:UTR] TLB threshold:  " << tlb_eviction_thr << std::endl ;
      std::cout << "[MMU:UTR] PTE threshold:  " << pte_eviction_thr << std::endl ;



      


      for (int i = 0; i < utrs; i++)
      {
          int utr_size = Sim()->getCfg()->getIntArray("perf_model/utopia/utr/size", i);
          int utr_page_size = Sim()->getCfg()->getIntArray("perf_model/utopia/utr/page_size", i);
          int utr_assoc= Sim()->getCfg()->getIntArray("perf_model/utopia/utr/assoc", i);
          String utr_repl= Sim()->getCfg()->getStringArray("perf_model/utopia/utr/repl", i);
          String utr_hash= Sim()->getCfg()->getStringArray("perf_model/utopia/utr/hash", i);


          UTR *utr_object;

          utr_object = new UTR(i+1, utr_size, utr_page_size, utr_assoc, utr_repl, utr_hash,this);
          utr_vector.push_back(utr_object);
      }



  }

}

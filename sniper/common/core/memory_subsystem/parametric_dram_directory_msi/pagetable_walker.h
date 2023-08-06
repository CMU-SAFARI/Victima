#ifndef PTW_H
#define PTW_H

#include "fixed_types.h"
#include "cache.h"
#include <unordered_map>
#include "cache_cntlr.h"
#include <random>
#include "pwc.h"
#include "nuca_cache.h"
#include "utopia_cache_template.h"

namespace ParametricDramDirectoryMSI
{
   class MemoryManager;
   class PageTableWalker{
   
	   protected:
			struct hash_pair {
				
					template <class T1, class T2> 
					size_t operator()(const std::pair<T1, T2>& p) const
					{ 
						auto hash1 = std::hash<T1>{}(p.first); 
						auto hash2 = std::hash<T2>{}(p.second); 
						return hash1 ^ hash2; 
					} 
				}; 

				struct hash_tuple3 {
				
					template <class T1, class T2, class T3> 
					size_t operator()(const std::tuple<T1, T2, T3>& t) const
					{ 
						auto hash1 = std::hash<T1>{}(std::get<0>(t)); 
						auto hash2 = std::hash<T2>{}(std::get<1>(t)); 
						auto hash3 = std::hash<T3>{}(std::get<2>(t)); 

						return hash1 ^ hash2 ^ hash3; 
					} 
				}; 

				struct hash_tuple4 {

					template <class T1, class T2, class T3, class T4> 
					size_t operator()(const std::tuple<T1, T2, T3, T4>& t) const
					{ 
						auto hash1 = std::hash<T1>{}(std::get<0>(t)); 
						auto hash2 = std::hash<T2>{}(std::get<1>(t)); 
						auto hash3 = std::hash<T3>{}(std::get<2>(t)); 
						auto hash4 = std::hash<T4>{}(std::get<3>(t)); 


						return hash1 ^ hash2 ^ hash3 ^ hash4; 
					} 
				}; 


			std::unordered_map <int, IntPtr> PML4_table;
			std::unordered_map <std::pair<int,IntPtr>, IntPtr, hash_pair> PDPE_table;
			std::unordered_map <std::tuple<int,IntPtr,IntPtr>, IntPtr,hash_tuple3> PD_table;
			std::unordered_map <std::tuple<int,IntPtr,IntPtr,IntPtr>, IntPtr,hash_tuple4> PT_table;

	   
	   		static int counter;
			IntPtr CR3; 
	   	    int data_size;
			CacheCntlr *cache; 
			int page_size;
			PWC* pwc;
			bool page_walk_cache_enabled;
			Byte* data_buf; 
			UInt32 data_length;
			bool modeled;
			bool count;
			IntPtr vpn;
			NucaCache* nuca;
			SubsecondTime current_lat;
			int core_id;
			Core::lock_signal_t lock_signal;
       		ShmemPerfModel* m_shmem_perf_model;
	   		SubsecondTime latency;
	   		std::uniform_int_distribution<long long int> uni;	
	   		std::unordered_map<IntPtr, long int>  per_page_ptw_latency;
	   		int *ptw_latency_heatmap;
	   		IntPtr eip; 
			MemoryManager* mem_manager;
			
	   public:

		struct {
				UInt64 page_walks;
				UInt64 number_of_2MB_pages;
         	} stats;
	   	   
		
		std::unordered_map<IntPtr, long int> getPtwHeatmap(){return per_page_ptw_latency;}
		ShmemPerfModel* getShmemPerfModel() { return m_shmem_perf_model; }
		virtual SubsecondTime init_walk(IntPtr eip, IntPtr address, UtopiaCache* shadow_cache, CacheCntlr *cache, Core::lock_signal_t lock_signal, Byte* _data_buf, UInt32 _data_length, bool modeled, bool count);
		virtual int init_walk_functional(IntPtr address) = 0;
		virtual bool isPageFault(IntPtr address) = 0;
		PageTableWalker(int core_id, int pagesize, ShmemPerfModel* m_shmem_perf_model,PWC* pwc, bool pwc_enabled);
		void setNucaCache(NucaCache* nuca2){ nuca = nuca2; printf("Attaching Nuca to PTW \n");}
		virtual void setMemoryManager(MemoryManager* _mem_manager){ mem_manager = _mem_manager;}
   		void track_per_page_ptw_latency(int vpn, SubsecondTime current_lat);
		int allocate_page_size();
   };

}

#endif
 

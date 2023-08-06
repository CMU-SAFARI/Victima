#include "buddy_allocator.h"
#include <utility>
#include <cmath>
#include <iostream>
#include <fstream>
#include <list>
#include <vector>
#include "config.hpp"
#include "simulator.h"

namespace ParametricDramDirectoryMSI
{
	BuddyAllocator::BuddyAllocator(int max_order):
		m_max_order(max_order)
	{
		for(int i=0;i<m_max_order + 1;i++)
		{
			std::vector<std::pair<UInt64, UInt64>> vec;
			free_list.push_back(vec);
		}

		init();

		try
		{
			String frag_file_name = Sim()->getCfg()->getStringArray("perf_model/pmem_alloc/fragmentation_file", 0);
			if(frag_file_name != "")
			{
				BuddyInitializer in(this);
				in.perform_init_file(frag_file_name);
				std::cout<<"[Memory Allocator] Finished Initialization from file"<<std::endl;
			}
		}
		catch(const std::exception& e)
		{

		}
	}

	void BuddyAllocator::init()
	{
		free_list.reserve(m_max_order);

		UInt64 pages_in_block = pow(2, m_max_order); //start with max
		UInt64 current_order = m_max_order;
		UInt64 total_mem_in_pages = m_memory_size * 1024 / 4;
		m_total_pages = total_mem_in_pages;
		m_free_pages = m_total_pages;
		UInt64 available_mem_in_pages = total_mem_in_pages;
		UInt64 current_free = 0;

		while(current_free < total_mem_in_pages)
		{
			while(available_mem_in_pages >= pages_in_block)
			{
				free_list[current_order].push_back(std::make_pair(current_free, current_free + pages_in_block - 1));
				current_free += pages_in_block;
				available_mem_in_pages -= pages_in_block;
			}
			current_order--;
			pages_in_block = pow(2, current_order);
		}

		//print_allocator();
	}

	UInt64 BuddyAllocator::allocate(UInt64 bytes)
	{
		UInt64 region_begin;
		int pages = 0;
		if(bytes < 1024*4)
			pages = 1;
		else
			pages = ceil(bytes/1024*4);

		//std::cout<<"[ALLOCATOR] Required "<<pages<<" pages."<<std::endl;

		int ind = ceil(log(pages)/log(2));
		if(free_list[ind].size() > 0)
		{
			std::pair<UInt64, UInt64> temp = free_list[ind][0];

			free_list[ind].erase(free_list[ind].begin());
			//std::cout<<"Memory from "<<temp.first <<" to "<<temp.second<<" allocated."<<std::endl;
			m_free_pages -= pow(2, ind);

			allocated_map[temp.first] = temp.second - temp.first + 1;
			region_begin = temp.first;
		}
		else
		{
			int i;
			for(i=ind; i<= m_max_order; i++)
			{
				if(free_list[i].size() != 0)
					break;
			}

			// std::cout<<"Gonna break page at ind "<<i<<std::endl;

			if(i == m_max_order +1)
				std::cout<<"[ERROR] FAILED TO ALLOCATE IN BUDDY???????????"<<std::endl;
			else
			{
				std::pair<UInt64, UInt64> temp;
				temp = free_list[i][0];
				//std::cout<<"Taking out the buddy "<<temp.first <<" "<<temp.second<<std::endl;

				free_list[i].erase(free_list[i].begin());
				i--;

				while(i >= ind)
				{
					std::pair<UInt64, UInt64> pair1, pair2;
					pair1 = std::make_pair(temp.first, temp.first + (temp.second - temp.first)/2);
					pair2 = std::make_pair(temp.first + (temp.second - temp.first + 1)/2, temp.second);

					free_list[i].push_back(pair1);
					free_list[i].push_back(pair2);
					temp = free_list[i][0];
					free_list[i].erase(free_list[i].begin());

					i--;
				}
				//std::cout<<"Memory from "<<temp.first <<" to "<<temp.second<<" allocated."<<std::endl;
				allocated_map[temp.first] = temp.second - temp.first + 1;
				region_begin = temp.first;
				m_free_pages -= pow(2, ind);

			}

		}

		return region_begin;
	}
	std::vector<Range> BuddyAllocator::allocate_eager_paging(UInt64 bytes)
	{
		UInt64 region_begin;
		std::pair<UInt64, UInt64> temp;

		int pages = 0;
		if(bytes < 1024*4)
			pages = 1;  
		else
			pages = ceil(bytes/1024*4);

		//std::cout<<"[ALLOCATOR] Required "<<pages<<" pages."<<std::endl;

		int ind = ceil(log(pages)/log(2));

		std::cout<<"[ALLOCATOR] Required " << pages <<" pages"<< "Order: " << ind << std::endl;

		bool above_max_order = ind < m_max_order;
		bool free_list_check = (above_max_order)? false : (free_list[ind].size()>0);

		std::vector<Range> ranges;

		while( pages > 0 ){ // We allocate the biggest possible contiguous segments 

			int order = ceil(log(pages)/log(2));

			int first_seg_flag = 1;
			std::cout << "[Eager Paging] Start eager allocation" << std::endl;
			for(int i = m_max_order; i >= 0; i--) // Start from max_order
			{

				if(free_list[i].size() > 0 && pages >= pow(2,i)){ //if the segment is free and the pages are more or same, allocate it


					std::pair<UInt64, UInt64> temp;
					temp = free_list[i][0];
					std::cout << "[Eager Paging]" << temp.first << " to " << temp.second << " allocated." << std::endl;

					free_list[i].erase(free_list[i].begin());
					allocated_map[temp.first] = temp.second - temp.first + 1;
					
					Range range;
					range.vpn = temp.first;
					range.bounds = temp.second-temp.first;
					ranges.push_back(range);

					pages -= pow(2,i);
					m_free_pages -= pow(2, i);
					break;
				}


				if(free_list[i].size() > 0 && pages < pow(2,i) && first_seg_flag){ //if the segment is free and the pages are less than it,

					std::pair<UInt64, UInt64> temp;
					temp = free_list[i][0];
					//std::cout<<"Taking out the buddy "<<temp.first <<" "<<temp.second<<std::endl;

					free_list[i].erase(free_list[i].begin());
					i--;

					while(i >= ind)
					{
						std::pair<UInt64, UInt64> pair1, pair2;
						pair1 = std::make_pair(temp.first, temp.first + (temp.second - temp.first)/2);
						pair2 = std::make_pair(temp.first + (temp.second - temp.first + 1)/2, temp.second);

						free_list[i].push_back(pair1);
						free_list[i].push_back(pair2);
						temp = free_list[i][0];
						free_list[i].erase(free_list[i].begin());

						i--;
					}
					//std::cout<<"Memory from "<<temp.first <<" to "<<temp.second<<" allocated."<<std::endl;
					std::cout << "[Eager Paging]" << temp.first << " to " << temp.second << " allocated." << std::endl;
					allocated_map[temp.first] = temp.second - temp.first + 1;
					Range range;
					range.vpn = temp.first;
					range.bounds = temp.second-temp.first;
					ranges.push_back(range);
					pages -= pow(2,ind);
					m_free_pages -= pow(2, ind);
					break;
						
				}

			}
		}
		std::cout << "[Eager Paging] Finished eager allocation" << std::endl;
		return ranges;
	}

	void BuddyAllocator::deallocate(UInt64 region_begin)
	{
		if(allocated_map.find(region_begin) == allocated_map.end())
		{
			//std::cout<<"[ERROR] FAILED TO FIND A BUDDY TO DEALLOCATE AT RANGE "<<region_begin<<std::endl;
			return;
		}

		int ind = ceil(log(allocated_map[region_begin]) / log(2));
		int i, buddyNumber, buddyAddress;

		free_list[ind].push_back(std::make_pair(region_begin, region_begin + pow(2, ind) - 1));
		//std::cout<<"Returned memory block from "<<region_begin<<" to "<<region_begin + pow(2, ind)-1<<std::endl;
		m_free_pages += pow(2, ind);

		//If we are at the max order, no need to try mergning
		if(ind == m_max_order)
		{
			allocated_map.erase(region_begin);
			return;
		}

		buddyNumber = region_begin / allocated_map[region_begin];
		buddyAddress = (buddyNumber % 2 != 0) ? (region_begin - pow(2, ind)) : (region_begin + pow(2, ind));

		//Begin trying to merge the buddies
		while(true)
		{
			bool merged = false;
			UInt64 new_region_begin;
			for(i = 0; i<free_list[ind].size(); i++)
			{
				//If the buddy is found and free
				if(free_list[ind][i].first == buddyAddress)
				{
					if(buddyNumber % 2 == 0)
					{
						free_list[ind + 1].push_back(std::make_pair(region_begin, region_begin + 2*pow(2, ind) - 1));
						//std::cout<<"Mergning blocks starting at "<<region_begin<<" and "<<buddyAddress<<std::endl;
						new_region_begin = region_begin;
					}
					else
					{
						free_list[ind+1].push_back(std::make_pair(buddyAddress, buddyAddress + 2*pow(2, ind)));
						//std::cout<<"Mergning blocks starting at "<<buddyAddress<<" and "<<region_begin<<std::endl;
						new_region_begin = buddyAddress;
					}

					free_list[ind].erase(free_list[ind].begin() + i);
					free_list[ind].erase(free_list[ind].begin() + free_list[ind].size() - 1);
					merged = true;
					break;
				}
			}
			//If we did not merge buddies, no point in continuing to try with next level
			if(!merged)
				break;

			ind++;
			//If we have the largest buddy, we cannot merge it further
			if(ind == m_max_order)
				break;

			allocated_map.erase(region_begin);
			region_begin = new_region_begin;
			buddyNumber = 2;
			buddyAddress = region_begin + pow(2, ind);
		}

		allocated_map.erase(region_begin);
	}

	void BuddyAllocator::print_allocator()
	{ std::cout<<"Printing buddy datastructures"<<std::endl;
		std::cout<<"Max order: "<<m_max_order<<std::endl;
		std::cout<<"Free List:"<<std::endl;
		for(int i=0;i<m_max_order + 1;i++)
		{
			std::cout<<"Order["<<i<<"]"<<std::endl;
			for(int j=0;j<free_list[i].size();j++)
				std::cout<<free_list[i][j].first<<" - "<<free_list[i][j].second<<std::endl;
		}


		std::cout<<"FRAGMENTATION?"<<std::endl;
		std::cout<<getFragmentationPercentage()<<std::endl;

	}

	UInt64 BuddyAllocator::getFreePages() const
	{
		return m_free_pages;
	}

	UInt64 BuddyAllocator::getTotalPages() const
	{
		return m_total_pages;
	}

	double BuddyAllocator::getFragmentationPercentage() const
	{
		UInt64 max_regions = m_total_pages/2;
		UInt64 cur_allocated_regions = 1;

		if(allocated_map.size() == 0)
			return 0;

		auto it = allocated_map.begin();
		it++;
		for(; it != allocated_map.end(); it++)
		{
			if(it->first != (std::prev(it))->first +(std::prev(it))->second)
			{
				cur_allocated_regions++;
				//std::cout<<"For "<<std::prev(it)->first + (std::prev(it))->second<<" and "<<it->first<<" decided yes."<<std::endl;
			}
			else
			{
				//std::cout<<"For "<<std::prev(it)->first + (std::prev(it))->second<<" and "<<it->first<<" decided no."<<std::endl;
			}

		}

		return (double)cur_allocated_regions/(double)max_regions;
	}

	BuddyAllocator::BuddyInitializer::BuddyInitializer(BuddyAllocator *allocator)
	{
		this->m_allocator = allocator;
	}

	void BuddyAllocator::BuddyInitializer::perform_init_file(String input_file_name)
	{
		std::ifstream file(input_file_name.c_str());
		std::string line;
		double frag = 0;

		//Read Max Order, Total Pages, Free Pages
		std::getline(file, line);
		m_allocator->m_max_order = std::stoi(line);

		std::getline(file, line);
		m_allocator->m_total_pages = std::stoi(line);

		std::getline(file, line);
		m_allocator->m_free_pages = std::stoi(line);

		std::cout<<"Read first 3"<<std::endl;

		//TODO add check if order is the same in the file and in the allocator
		for(int p=0;p<m_allocator->free_list.size(); p++)
		{
			std::getline(file, line);
			//std::cout<<"Reading "<<line<<std::endl;
			UInt64 i = std::stoi(line);
			m_allocator->free_list[i].clear();

			std::getline(file, line);
			//std::cout<<"Reading "<<line<<std::endl;
			UInt64 size = std::stoi(line);
			for(int j=0;j<size;j++)
			{
				std::getline(file, line);
				UInt64 first = std::stoi(line);
				//std::cout<<"Reading "<<line<<std::endl;


				std::getline(file, line);
				UInt64 second  = std::stoi(line);
				//std::cout<<"Reading "<<line<<std::endl;


				auto pair = std::make_pair(first, second);
				m_allocator->free_list[i].push_back(pair);
			}
		}

		std::cout<<"Read free list"<<std::endl;

		std::getline(file, line);
		UInt64 size = std::stoi(line);
		//std::cout<<"Read size for allocated list "<<size<<std::endl;
		m_allocator->allocated_map.clear();
		for(int i=0; i<size; i++)
		{
			std::getline(file, line);
			UInt64 first = std::stoi(line);

			std::getline(file, line);
			UInt64 second  = std::stoi(line);

			m_allocator->allocated_map[first] = second;
			//std::cout<<"Added "<<first<<" and "<<second<<std::endl;
		}

		std::cout<<"Read allocated list"<<std::endl;

	}

	void BuddyAllocator::BuddyInitializer::perform_init_random(double target_fragmentation, double target_memory_percent)
	{
		UInt64 pages = (UInt64) (m_allocator->m_total_pages * target_memory_percent);
		std::cout<<"MemInit allocating "<<pages<<" pages out of "<<m_allocator->m_total_pages<<std::endl;
		//m_allocator->allocate(pages);
		std::vector<UInt64> used_pages(pages);
		UInt64 chunk = 1;
		for(int i=0;i<pages;i++)
		{
			used_pages[i] = m_allocator->allocate(chunk);
		}

		double current_fragmentation = m_allocator->getFragmentationPercentage();
		double prev_fragmentation = 1;
		while(current_fragmentation < target_fragmentation)
		{
			auto r = (UInt64) rand() % pages;
			for(int i=0;i<1000;i+=2)
				m_allocator->deallocate(used_pages[r+i]);
			prev_fragmentation = current_fragmentation;
			current_fragmentation = m_allocator->getFragmentationPercentage();
			used_pages[r] = used_pages[pages-1];
			pages--;


			if(current_fragmentation <prev_fragmentation)
			{
				std::cout<<"Current less than previous, managed to stop for "<<current_fragmentation<<std::endl;
				//break;
			}

			std::cout<<"Current fragmentation "<<current_fragmentation<<std::endl;
		}

		std::cout<<"Initializator done "<<current_fragmentation<<" and "<<m_allocator->m_free_pages<<"/"<<m_allocator->m_total_pages<<std::endl;
		std::cout<<"Saving to file...."<<std::endl;

		std::ofstream file;
		file.open("fragmentation16GB30PER");
		//Save Max Order, Total Pages, Free Pages
		file<<m_allocator->m_max_order<<std::endl;
		file<<m_allocator->m_total_pages<<std::endl;
		file<<m_allocator->m_free_pages<<std::endl;

		//first save free_list
		for(int i=0;i<m_allocator->free_list.size(); i++)
		{
			file<<i<<std::endl;
			file<<m_allocator->free_list[i].size()<<std::endl;
			for(int j=0;j<m_allocator->free_list[i].size();j++)
			{
				file<<m_allocator->free_list[i][j].first<<std::endl;
				file<<m_allocator->free_list[i][j].second<<std::endl;
			}
		}
		//now save allocated map
		file<<m_allocator->allocated_map.size()<<std::endl;
		for(auto it = m_allocator->allocated_map.begin(); it != m_allocator->allocated_map.end();it++)
		{
			file<<it->first<<std::endl;
			file<<it->second<<std::endl;
		}

		std::cout<<"Done writing to the file."<<std::endl;


		//m_allocator->print_allocator();
	}
}

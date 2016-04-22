#include <list>
#include <stack>
#include <deque>
#include <algorithm>
#include <iostream>
#include <stdint.h>
#include "interface.hh"
#include "base/trace.hh" 

using namespace std;

#define ENTRY_LIMIT 100
#define DELTAS_ENTRY 6 
#define DELTA_BITFIELD_WIDTH 10
#define MAX_DELTA ((1 << ((DELTA_BITFIELD_WIDTH) - 1)) - 1)
#define MIN_DELTA 0
#define INFLIGHT_ELEMENTS 32

typedef uint64_t Delta_t;

list<Addr> candidates;
list<Addr> prefetches;

deque<Addr> inFlight;

struct entry{
   entry(Addr pc);

   Addr pc;
   Addr lastAddress;
   Addr lastPrefetch;
   
   deque<Delta_t> deltaArray;
};

struct DCPTtable{
   DCPTtable();
   entry *find_entry(Addr pc);

   list<entry *> entrylist;
   int amountofentries;
};

entry::entry(Addr pc): pc(0), lastAddress(0), lastPrefetch(0){ }

static DCPTtable *table;

DCPTtable::DCPTtable() : amountofentries(0){ }

/*Find existing entry from pc search, or create a new entry to the entrylist */
entry *DCPTtable::find_entry(Addr pc){
   list<entry *>::iterator i = entrylist.begin();
   for(; i != entrylist.end(); ++i){ 
      entry *search_entry = *i;
      
      if(pc == search_entry->pc){ 
         return search_entry; //returning the existing entry with the right pc
      }
   }
return NULL; //not found an entry for the pc
}

void deltaCorrelation(entry *entry){
   candidates.clear();

   deque<Delta_t>::iterator it_deltaArray = entry->deltaArray.end();
 
   it_deltaArray-=1;
   Delta_t delta1 = *it_deltaArray; //last delta
   it_deltaArray-=1;
   Delta_t delta2 = *it_deltaArray; //second last delta
   Addr address = entry->lastAddress;
   
   it_deltaArray = entry->deltaArray.begin();
   while(it_deltaArray != entry->deltaArray.end()){
      Delta_t u = *it_deltaArray;
      it_deltaArray++;
      Delta_t v = *it_deltaArray;
      /*Search pair by pair for a match between the two last deltas and the pairs from the beginning of the buffer */ 
      if(u==delta2 && v==delta1){ //found a pattern from the previous deltas
            for(it_deltaArray++; it_deltaArray != entry->deltaArray.end(); ++it_deltaArray){
            address += *it_deltaArray * BLOCK_SIZE; //add the rest of deltas to the address
            candidates.push_back(address);
         }
      }
   }
}

void prefetch_filter(entry *entry){
   prefetches.clear();
  
   list<Addr>::iterator it_candidates;

   for(it_candidates=candidates.begin(); it_candidates!=candidates.end(); ++it_candidates){

      /*Check if the prefetch candidate is already in cache, mshr or is inFlight(a buffer that holds other prefetch requests that have not been completed */
      if(std::find(inFlight.begin(), inFlight.end(), *it_candidates) == inFlight.end() && !in_mshr_queue(*it_candidates) && !in_cache(*it_candidates)){
         prefetches.push_back(*it_candidates);
         
         if(inFlight.size() == INFLIGHT_ELEMENTS){
            inFlight.pop_front();
         }

         inFlight.push_back(*it_candidates);
         entry->lastPrefetch = *it_candidates;
      }

      /*if(*it_candidates == entry->lastPrefetch)
            prefetches.clear(); */
   }
}

void prefetch_init(void){
   /* Called before any calls to prefetch_access. */
   /* This is the place to initialize data structures. */
   table = new DCPTtable;
   DPRINTF(HWPrefetch, "Initialized dcpt prefetcher\n");
}


void prefetch_access(AccessStat stat){
      entry *current_entry = table->find_entry(stat.pc);
      int64_t Delta;

      if(current_entry == NULL){ //create a new entry for the pc
         
      /* Remove the oldest entry if the table reaches the maximum amount of entries */
      if(table->amountofentries == ENTRY_LIMIT)
         table->entrylist.pop_back();

         entry *newentry = new entry(stat.pc);
         newentry->pc = stat.pc;
         newentry->lastAddress = stat.mem_addr;
         newentry->lastPrefetch = 0;
         newentry->deltaArray.push_front(1); 
         table->entrylist.push_front(newentry);
         current_entry = newentry;

         if(table->amountofentries < ENTRY_LIMIT)
            ++table->amountofentries;

         cout << "Amount of entries: " << table->amountofentries << endl;
      }
      else{
         Delta = (int64_t)stat.mem_addr - (int64_t)current_entry->lastAddress; 
         Delta /= BLOCK_SIZE >> 1;
         
         if(Delta != 0){ //there is no overflow or equal address
            if(Delta < MIN_DELTA || Delta > MAX_DELTA)
              Delta = MIN_DELTA;

            if(current_entry->deltaArray.size() == DELTAS_ENTRY)
               current_entry->deltaArray.pop_front();
            
            current_entry->deltaArray.push_back((Delta_t)Delta);
            current_entry->lastAddress = stat.mem_addr;
         }

         deltaCorrelation(current_entry);
         prefetch_filter(current_entry);
      
         list<Addr>::iterator iterator;
         for (iterator = prefetches.begin(); iterator != prefetches.end(); ++iterator) {
            issue_prefetch(*iterator); //issue a prefetch for the addresses in prefetches list
         }
      }
}

void prefetch_complete(Addr addr) {
}

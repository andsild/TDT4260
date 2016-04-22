#include <map>
#include "interface.hh"

#define ENTRY_LIMIT 100
using namespace std;

struct RPTentry{
   RPTentry(Addr pc);
   
   Addr pc;
   Addr lastAddress;
   int delta; //stride 
   RPTentry *next;
   RPTentry *prev;
};

struct RPTmap{
   RPTmap();
   RPTentry *find_entry(Addr pc);

   int amountofentries;
   RPTentry *head_ptr, *tail_ptr;
   map<Addr, RPTentry *> entryMap;
};

static RPTmap *table; //the table is initialized in the beginning of execution

/*Construct the entry */
RPTentry::RPTentry(Addr pc): pc(pc), lastAddress(0), delta(0), next(0), prev(0){ }

/*Construct the entry */
RPTmap::RPTmap() : amountofentries(0), head_ptr(0), tail_ptr(0){ }

RPTentry *RPTmap::find_entry(Addr pc){
   if(entryMap.find(pc) == entryMap.end()){ //couldnt find pc by iterator
      RPTentry *newEntry = new RPTentry(pc); //create a new entry with the program counter address
     
      /* Remove the oldest entry if the table reaches the maximum amount of entries */
      if(amountofentries == ENTRY_LIMIT){
         RPTentry *remove_entry = tail_ptr;
         tail_ptr = remove_entry->next;
         tail_ptr->prev = 0;
         entryMap.erase(remove_entry->pc);
         delete remove_entry;
      }

      //already a list (not empty)
      if(head_ptr != 0){
         head_ptr->next = newEntry;
         newEntry->prev = head_ptr;
      }
      else{ //empty table
         tail_ptr = newEntry;
         newEntry->next = 0;
         newEntry->prev = 0;
      }

      head_ptr = newEntry;
      entryMap[pc] = newEntry; //add the entry into the map pointing to the PC
      
      if(amountofentries < ENTRY_LIMIT) ++amountofentries;
   }
   
   return entryMap[pc];
}

void prefetch_init(void){
/* Called before any calls to prefetch_access. */
/* This is the place to initialize data structures. */
   DPRINTF(HWPrefetch, "Initialized sequential-on-access prefetcher\n");

   /*initiate the map-table structure */
   table = new RPTmap;
}

void prefetch_access(AccessStat stat){
/* pf_addr is now an address within the _next_ cache block */
//   Addr pf_addr = stat.mem_addr + BLOCK_SIZE;
/*
* Issue a prefetch request if a demand miss occured,
* and the block is not already in cache.
*/
   if (stat.miss){
      
      /*Get the entry from the map and then issue a prefetch */
      RPTentry *entry = table->find_entry(stat.pc);

      int newdelta = stat.mem_addr - entry->lastAddress; //New address - Old address
      /*Check if it found a pattern and if the data is already in cache */
      if(entry->delta == newdelta && !in_cache(stat.mem_addr + entry->delta) && !in_mshr_queue(stat.mem_addr + entry->delta)){
         issue_prefetch(stat.mem_addr + entry->delta); 
      }
      entry->delta = newdelta;
      entry->lastAddress = stat.mem_addr;
   }
}

void prefetch_complete(Addr addr) {
/*
* Called when a block requested by the prefetcher has been loaded.
*/
}

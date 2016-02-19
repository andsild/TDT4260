// Multi-level Adaptive Sequential Tagged Prefetching
// based on Performance Gradient Tracking
// Ramos, Briz, Ibanez, Vinals
// Later Adapted for L2-caching 

// Prefetch bit: a line that has been prefetched but not requested
// E.g. asking for address 0xA and getting 0xA and 0xB (0xB goes in cache with prefetch bit, 0xA without it)

#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "interface.hh"


void SequentialTaggingL1_cycle(AccessStat *L1Data);
void SequentialTaggingL2(AccessStat *L2Data);

// SEQT Degree Automaton (SDA)
Addr SDA_last_addr;
char SDA_degree=0;

// SEQT L1 Degree Automaton (SDA1)
Addr SDA1_last_addr;
char SDA1_degree=0;

typedef struct MSHR_entry MSHR_entry;
struct MSHR_entry {
    Addr addr;
    char valid;
};
typedef struct MissStatusHandlingRegister MissStatusHandlingRegister;
struct MissStatusHandlingRegister{
    int size;
    int tail;
    int num;
    int head;
    MSHR_entry entry[32];
};
MissStatusHandlingRegister * PrefetchMissAddressFile1;
MissStatusHandlingRegister * MissStatusHandlingRegisterD2;
MissStatusHandlingRegister * PrefetchMissAddressFile2;

void MissStatusHandlingRegister_ini (void);
int MissStatusHandlingRegister_lookup (MissStatusHandlingRegister * MissStatusHandlingRegister, Addr addr);   // if hit it returns 1
void MissStatusHandlingRegister_insert (MissStatusHandlingRegister * MissStatusHandlingRegister, Addr addr);  // if full, the oldest entry is deleted
void MissStatusHandlingRegister_cycle ();               // it deletes the oldest entry if it hits on the cache (using GetPrefetchBit)

unsigned int AD_cycles=0;
Tick AD_L1_accesses=0;
Tick AD_last_L1_accesses=0;
#define AD_interval (64*1024)
#define AD_MAX_INDEX (13)   // number of different degrees
int AD_degs[AD_MAX_INDEX]={0,1,2,3,4,6,8,12,16,24,32,48,64};
int AD_deg_index=4;
int AD_degree=4;
char AD_state=0; // 0: increasing degree;  1: decreasing degree

void AD_cycle(AccessStat *L1Data);

void SequentialTaggingL2(AccessStat *L2Data)
{
    // miss or "1st use" in L2? (considering demand and prefetch references)
    if ((L2Data->miss == 1 
            && !MissStatusHandlingRegister_lookup(MissStatusHandlingRegisterD2, L2Data->mem_addr)) 
        || get_prefetch_bit(L2Data->mem_addr)==1)
    {
        if (L2Data->miss == 1)
            MissStatusHandlingRegister_insert(MissStatusHandlingRegisterD2, L2Data->mem_addr);
        else 
            clear_prefetch_bit(L2Data->mem_addr);
    }

    if (AD_degree){
        // program SDA
        SDA_last_addr=L2Data->mem_addr;
        SDA_degree=AD_degree; 
    }

    // the SDA generates 1 prefetch per cycle
    if (SDA_degree)
    {
        Addr predicted_address= SDA_last_addr+0x40; 

        // issue prefetch (if not filtered)
        if (get_prefetch_bit(predicted_address)==-1){
            if (!MissStatusHandlingRegister_lookup(PrefetchMissAddressFile2,predicted_address & 0xffff) 
                && !MissStatusHandlingRegister_lookup(MissStatusHandlingRegisterD2,predicted_address))
            {
                issue_prefetch(predicted_address);
                set_prefetch_bit(predicted_address);
                MissStatusHandlingRegister_insert(PrefetchMissAddressFile2,predicted_address & 0xffff);
            }
        }
        // program SDA
        SDA_last_addr=predicted_address;
        SDA_degree--;
    }
}

void SequentialTaggingL1_cycle(AccessStat *L1Data)
{
    if (L1Data->miss == 1 || get_prefetch_bit(L1Data->mem_addr))
    {
        if (L1Data->miss == 0)
            clear_prefetch_bit(L1Data->mem_addr);

        // program SDA1
        SDA1_last_addr=L1Data->mem_addr;
        SDA1_degree=(L1Data->miss == 1 ? 1 : 4 ); 
    } //end if

    // the SDA1 generates 1 prefetch per cycle
    if (SDA1_degree)
    {
        Addr predicted_address= SDA1_last_addr+0x40; 

        // issue prefetch (if not filtered)
        if (!get_prefetch_bit(predicted_address))
            if (!in_mshr_queue(predicted_address & 0xffff))
            {       
                issue_prefetch(predicted_address);
                set_prefetch_bit(predicted_address);
                MissStatusHandlingRegister_insert(PrefetchMissAddressFile1,
                                                  predicted_address & 0xffff);         
            }     
        // program next prefetch for the next cycle
        SDA1_last_addr=predicted_address;
        SDA1_degree--;
    }
}

void MissStatusHandlingRegister_ini (void){
    PrefetchMissAddressFile1=(MissStatusHandlingRegister *) calloc(1, sizeof(MissStatusHandlingRegister));
    PrefetchMissAddressFile1->size=32;
    MissStatusHandlingRegisterD2=(MissStatusHandlingRegister *) calloc(1, sizeof(MissStatusHandlingRegister));
    MissStatusHandlingRegisterD2->size=16;
    PrefetchMissAddressFile2=(MissStatusHandlingRegister *) calloc(1, sizeof(MissStatusHandlingRegister));
    PrefetchMissAddressFile2->size=32;
}
int MissStatusHandlingRegister_lookup (MissStatusHandlingRegister * MissStatusHandlingRegister, Addr addr){
    int i;
    Addr MASK = 0x3f; 

    /* empty */
    if (!MissStatusHandlingRegister->size || !MissStatusHandlingRegister->num)
        return 0;

    for (i=0; i < MissStatusHandlingRegister->size; i++){
        if (MissStatusHandlingRegister->entry[i].valid 
                && (MissStatusHandlingRegister->entry[i].addr == (addr & ~MASK)) ){
            return 1;
        }
    }
    return 0;
}

void MissStatusHandlingRegister_insert (MissStatusHandlingRegister * MissStatusHandlingRegister, Addr addr)
{
    Addr MASK = 0x3f; 

    if (!MissStatusHandlingRegister->size || !addr)
        return;

    MissStatusHandlingRegister->entry[MissStatusHandlingRegister->tail].valid=1;
    MissStatusHandlingRegister->entry[MissStatusHandlingRegister->tail].addr= (addr & ~MASK);
    MissStatusHandlingRegister->tail=
        (MissStatusHandlingRegister->tail+1)%MissStatusHandlingRegister->size;

    if (MissStatusHandlingRegister->num<MissStatusHandlingRegister->size)
        MissStatusHandlingRegister->num++;
    else
        MissStatusHandlingRegister->head=MissStatusHandlingRegister->tail;
}

void MissStatusHandlingRegister_cycle () 
{
    /* If we have entries and head has a `prefetched` page */
    if (PrefetchMissAddressFile1->num && 
            (get_prefetch_bit(PrefetchMissAddressFile1->entry[PrefetchMissAddressFile1->head].addr) != -1) )
    {
        /* invalidate head (and remember to update num) */
        PrefetchMissAddressFile1->num--;
        PrefetchMissAddressFile1->entry[PrefetchMissAddressFile1->head].valid=0; 
        /* Point head to the next entry
            Modulus: if head points outside of scope, then point to beginning 
        */
        PrefetchMissAddressFile1->head=(PrefetchMissAddressFile1->head+1)%PrefetchMissAddressFile1->size;
    }

    if (MissStatusHandlingRegisterD2->num 
        && (get_prefetch_bit(MissStatusHandlingRegisterD2->entry[MissStatusHandlingRegisterD2->head].addr)!= -1) )
    {
        MissStatusHandlingRegisterD2->num--;
        MissStatusHandlingRegisterD2->entry[MissStatusHandlingRegisterD2->head].valid=0;
        
        MissStatusHandlingRegisterD2->head=(MissStatusHandlingRegisterD2->head+1)%MissStatusHandlingRegisterD2->size;
    }

    if (PrefetchMissAddressFile2->num 
    && (get_prefetch_bit(PrefetchMissAddressFile2->entry[PrefetchMissAddressFile2->head].addr)!= -1) )
    {
        PrefetchMissAddressFile2->num--;
        PrefetchMissAddressFile2->entry[PrefetchMissAddressFile2->head].valid=0;
        PrefetchMissAddressFile2->head=(PrefetchMissAddressFile2->head+1)%PrefetchMissAddressFile2->size;
    }
}

void AD_cycle(AccessStat *L1Data)
{
    AD_cycles++;
    // for(i = 0; i < 4; i++) {
        // if(cycle == L1Data[i].LastRequestCycle) 
        //     AD_L1_accesses++;
    // }
    if (in_mshr_queue(L1Data->mem_addr))
        AD_L1_accesses++;

    if (AD_cycles>AD_interval)
    {
        AD_cycles=0;
        if (AD_L1_accesses<AD_last_L1_accesses){
            AD_state=!AD_state;
        }
        if (!AD_state) 
        {
            if (AD_deg_index<AD_MAX_INDEX-1) AD_deg_index++; 
        } 
        else {
            if (AD_deg_index>0) AD_deg_index--; 
        }
        AD_degree=AD_degs[AD_deg_index];
        AD_last_L1_accesses=AD_L1_accesses;
        AD_L1_accesses=0;
    }
}


void prefetch_init(void){
    /* Called before any calls to prefetch_access. */
    /* This is the place to initialize data structures. */
    MissStatusHandlingRegister_ini();
}

// Function that is called when L2 memory is requested from L1
void prefetch_access(AccessStat stat){
    //  Function that is called every cycle to issue prefetches should the
    //  prefetcher want to.  The arguments to the function are the current cycle,
    //  the demand requests to L1 and L2 cache. 
    MissStatusHandlingRegister_cycle(); 
    struct AccessStat *ptr = &stat;
    AD_cycle(ptr);
    SequentialTaggingL1_cycle(ptr);
    SequentialTaggingL2(ptr);
}

void prefetch_complete(Addr addr) {
    /*
     * Called when a block requested by the prefetcher has been loaded.
     */
}

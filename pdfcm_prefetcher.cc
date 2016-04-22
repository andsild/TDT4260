#include "interface.hh"
#include <stdio.h>
#include <stdlib.h>

#define MAX_EPOCH_CYCLES (64*256)
#define BITMASK_16 0xffffffff
#define MAX_DEGREES (13)   // number of different degrees
#define DELTATABLE_BITS (8) // doesnt seem to matter much
#define DELTA_SIZE (1 << DELTATABLE_BITS)
#define DELTAMASK (DELTA_SIZE - 1)
#define HISTORYTABLE_BITS 9
#define HISTORY_TABLE_SIZE (1 << HISTORYTABLE_BITS)
#define HISTORYTABLE_MASK (HISTORY_TABLE_SIZE - 1)

typedef struct historyTablerecord historyTablerecord;
struct historyTablerecord
{
    unsigned int PC; //16 bits
    unsigned int lastAddrMiss; // 16 bits
    unsigned int hashHistory; //16 bits
    char hitCount; //8 bits
};
historyTablerecord * HistoryTable;  

typedef struct DeltaTableRecord DeltaTableRecord;
struct DeltaTableRecord
{
    int delta;  //16 bits
};
DeltaTableRecord * DeltaTable;

unsigned int StateHashHistory;
unsigned int LastMissedAddress;
// We see our program as a state machine where one state represents
// a degree and an address
Addr StatePrediction;
char StateDegree=0;

unsigned int AdaptiveDegree_Cycles=0;
int EpochConfidence=0;
int FormerEpochConfidence=0;
int DegreeList[MAX_DEGREES]= {0,1,2,3,4,6,8,12,16,24,32,48,64};
int CurrentDegreeIndex=4;
int CurrentDegree=4;
// Adaptions mean that changes will either raise the current degree 
// or lower it at the end of an epoch.
// This confidenceer remembers: (0 = increasing)
char DegreeIsDecreasing=0;

void AdaptiveDegree_Cycle(AccessStat *);

// See the following, bottom page 6 and top 7
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.92.6198&rep=rep1&type=pdf
unsigned int CalculateHistoryMask (unsigned int previousHashHistory, int delta)
{
    unsigned int foldedBits, maskedFold;
    unsigned int select=delta; // note: this is a cast
    for(foldedBits=0; select;)
    {
        foldedBits ^= select & DELTAMASK;
        select= select >> DELTATABLE_BITS;
    }
    maskedFold = (previousHashHistory << 5) & DELTAMASK;     
    return maskedFold ^ foldedBits;
}

Addr UpdateTables (Addr pc, Addr addr, unsigned int *hashHistory)
{
    unsigned int currentHistory;
    unsigned int index = pc & HISTORYTABLE_MASK;
    unsigned int previousAddress = HistoryTable[index].lastAddrMiss;
    unsigned int previousHashHistory = HistoryTable[index].hashHistory;
    char confidence = HistoryTable[index].hitCount;

    if (HistoryTable[index].PC!=((pc>>HISTORYTABLE_BITS) & BITMASK_16))
    {
        // if it's a new PC replace the record
        HistoryTable[index].PC=((pc>>HISTORYTABLE_BITS) & BITMASK_16);
        HistoryTable[index].hashHistory=0;
        HistoryTable[index].lastAddrMiss=addr & BITMASK_16;
        HistoryTable[index].hitCount=0;
        return 0;
    }
    // compute deltas & update confidence Tick
    int actualDelta = (addr-previousAddress) & BITMASK_16;

    *hashHistory   = currentHistory   = CalculateHistoryMask(previousHashHistory, actualDelta);
    // write HistoryTable record
    HistoryTable[index].lastAddrMiss = (addr & BITMASK_16); // current missed address
    HistoryTable[index].hashHistory   = currentHistory;
    HistoryTable[index].hitCount=confidence;

    // update DeltaTable record
    DeltaTable[previousHashHistory].delta = actualDelta;

    return addr + DeltaTable[currentHistory].delta;
}

Addr MakePrediction (Addr addr, unsigned int *hashHistory,
                         unsigned int old_lastAddrMiss)
{
    // compute delta
    int delta = (addr-old_lastAddrMiss) & BITMASK_16;
    // compute new hashHistory
    *hashHistory=CalculateHistoryMask(*hashHistory, delta);
    // predict a new delta using the new hashHistory
    return addr + DeltaTable[*hashHistory].delta;
}


void AdaptiveDegree_Cycle(AccessStat *L2Snapshot)
{
    AdaptiveDegree_Cycles++;
    // confidence is how many references there were to prefetched blocks in the previous cycle
    // We use the variable below to determine determine L2 adaption degree
    if(in_cache(L2Snapshot->mem_addr))
        EpochConfidence++;

    //// MAX_EPOCH_CYCLES is an epoch (given number of cycles).
    //// In an epoch, we define success on whether or not we have many hits to prefetches (confidence)
    //// At the end of an epoch we adjust our degree (gradient)
    //// See page 7 in ./doc/doc.pdf
    if (AdaptiveDegree_Cycles>MAX_EPOCH_CYCLES)
    {
       AdaptiveDegree_Cycles=0;
       if (EpochConfidence<FormerEpochConfidence)
           DegreeIsDecreasing=!DegreeIsDecreasing;
       if (!DegreeIsDecreasing)
       {
           if (CurrentDegreeIndex<MAX_DEGREES-1) 
               CurrentDegreeIndex++;
       }
       else 
        {
           if(CurrentDegreeIndex>0) 
               CurrentDegreeIndex--;
        }
       CurrentDegree=DegreeList[CurrentDegreeIndex];
       FormerEpochConfidence=EpochConfidence;
       EpochConfidence=0;
    }
}

void PDFCM_cycle(AccessStat *L2Data)
{
    Addr predicted_address;
    unsigned int hashHistory=StateHashHistory;

    if (((L2Data->miss == 1  && ! in_mshr_queue(L2Data->mem_addr)) ))
    {

        predicted_address = UpdateTables(L2Data->pc,
                            L2Data->mem_addr, &hashHistory);

        if(CurrentDegree)
        {
            // issue prefetch (if not filtered)
            if (predicted_address && predicted_address < MAX_PHYS_MEM_ADDR  &&
                    predicted_address!=L2Data->mem_addr
                    && !in_mshr_queue(predicted_address & BITMASK_16) &&
                    ! in_cache(predicted_address)
            )
                issue_prefetch(predicted_address);

            if (predicted_address)
            {
                StateHashHistory=hashHistory;
                LastMissedAddress=L2Data->mem_addr & BITMASK_16;
                StatePrediction=predicted_address;
                StateDegree=CurrentDegree-1;
            }
        }
    }
    else if (StateDegree && StateHashHistory)
    {
        predicted_address = MakePrediction(StatePrediction, &StateHashHistory, LastMissedAddress);
        // issue prefetch (if not filtered)
        if (predicted_address && predicted_address < MAX_PHYS_MEM_ADDR 
        && predicted_address!=StatePrediction 
        && !in_mshr_queue(predicted_address & BITMASK_16) 
        && !in_cache(predicted_address))
        {
            issue_prefetch(predicted_address);
        }
        LastMissedAddress=StatePrediction & BITMASK_16;
        StatePrediction=predicted_address;
        StateDegree--;
    }
}

void prefetch_init(void)
{
    /* Called before any calls to prefetch_access. */
    HistoryTable=(historyTablerecord *) calloc(HISTORY_TABLE_SIZE, sizeof(historyTablerecord));
    DeltaTable=(DeltaTableRecord *) calloc(DELTA_SIZE, sizeof(DeltaTableRecord));
}

int myIndex = 0;
void prefetch_access(AccessStat stat)
{
    AccessStat *ptr = &stat;

    AdaptiveDegree_Cycle(ptr);
    PDFCM_cycle(ptr);
}

void prefetch_complete(Addr addr)
{ /* Intentionally blank */ }

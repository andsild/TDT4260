// submissions only allow one file, so this file is a result
// of merging a header file on top of the source cpp file

// this file still needs more adaption to make sense,
// but gives a rough overview of things.

// Prefetch bit / tag - request by demand of "intelligence"?
// PDFCM : Prefetch differential finite context machine
// PMAF: Prefetch Memory Address File
// MSHR and PMAF are both FIFO structures

#include "interface.hh"
#include <stdio.h>
#include <stdlib.h>


//TODO: experiment with these numbers
#define LOOKBACK_DEGREE 8
#define BITMASK_16 0xffff
#define MAX_EPOCH_CYCLES (64*1024)
#define MAX_DEGREES (10)   // number of different degrees
#define DELTATABLE_BITS (9) // doesnt seem to matter much
#define DELTA_SIZE (1 << DELTATABLE_BITS) // multiply by 2^DELTATABLE_BITS, e.g. 1 << 1 = 2, 1 << 2 = 4, etc
#define DELTAMASK (DELTA_SIZE - 1)
#define HISTORYTABLE_BITS 8
#define HISTORY_TABLE_SIZE (1 << HISTORYTABLE_BITS)
#define HISTORYTABLE_MASK (HISTORY_TABLE_SIZE - 1)

typedef struct historyTablerecord historyTablerecord;
struct historyTablerecord // short: 16 bits
{
    unsigned short PC;
    unsigned short lastAddrMiss;
    unsigned short hashHistory;
    char hitCount;
};
historyTablerecord * HistoryTable;  

typedef struct DeltaTableRecord DeltaTableRecord;
struct DeltaTableRecord
{
    short delta; 
};
DeltaTableRecord * DeltaTable; 

void InitializeDatastructures (void);
Addr UpdateTables (Addr pc, Addr addr,
                   unsigned short
                   *hashHistory);       
Addr MakePrediction (Addr addr, unsigned short *hashHistory,
                         unsigned short lastAddrMiss);   
unsigned short CalculateHistoryMask (unsigned short previousHashHistory,
                           short delta);
void PDFCM_cycle(AccessStat *L2Data);

unsigned short StateHashHistory;
unsigned short LastMissedAddress;
// We see our program as a state machine where one state represents
// a degree and an address
Addr StatePrediction;
char StateDegree=0;


unsigned short AdaptiveDegree_Cycles=0;
short L1AccessConfidence=0;
short FormerL1AccessConfidence=0;
short DegreeList[MAX_DEGREES]= {0,1,2,3,4,6,8,12,16,24};
short CurrentDegreeIndex=4;
short CurrentDegree=4;
// Adaptions mean that changes will either raise the current degree 
// or lower it at the end of an epoch.
// This confidenceer remembers: (0 = increasing)
char DegreeIsDecreasing=0;

void AdaptiveDegree_Cycle(AccessStat *);

void InitializeDatastructures (void)
{
    HistoryTable=(historyTablerecord *) calloc(HISTORY_TABLE_SIZE, sizeof(historyTablerecord));
    DeltaTable=(DeltaTableRecord *) calloc(DELTA_SIZE, sizeof(DeltaTableRecord));
}

Addr UpdateTables (Addr pc, Addr addr, unsigned short *hashHistory)
{
    unsigned short currentHistory;
    unsigned int index = pc & HISTORYTABLE_MASK;
    unsigned short previousAddress = HistoryTable[index].lastAddrMiss;
    unsigned short previousHashHistory = HistoryTable[index].hashHistory;
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
    short predictedDelta = DeltaTable[previousHashHistory].delta;
    short actualDelta = (addr-previousAddress) & BITMASK_16;
    if (actualDelta==predictedDelta)
    {
        if (confidence<3) { confidence++; }
    }
    else {
        if (confidence>0) { confidence--; }
    }

    *hashHistory   = currentHistory   = CalculateHistoryMask(previousHashHistory, actualDelta);
    // write HistoryTable record
    HistoryTable[index].lastAddrMiss = (addr & BITMASK_16); // current missed address
    HistoryTable[index].hashHistory   = currentHistory;
    HistoryTable[index].hitCount=confidence;

    // update DeltaTable record
    DeltaTable[previousHashHistory].delta = actualDelta;

    if (confidence<2)
        return 0;
    else
        // predict a new delta using the new hashHistory
        return addr + DeltaTable[currentHistory].delta;
}

Addr MakePrediction (Addr addr, unsigned short *hashHistory,
                         unsigned short old_lastAddrMiss)
{
    // compute delta
    short delta = (addr-old_lastAddrMiss) & BITMASK_16;
    // compute new hashHistory
    *hashHistory=CalculateHistoryMask(*hashHistory, delta);
    // predict a new delta using the new hashHistory
    return addr + DeltaTable[*hashHistory].delta;
}

// See the following, bottom page 6 and top 7
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.92.6198&rep=rep1&type=pdf
unsigned short CalculateHistoryMask (unsigned short previousHashHistory, short delta)
{
    unsigned short foldedBits, maskedFold;
    unsigned short select=delta; // note: this is a cast
    for(foldedBits=0; select;)
    {
        foldedBits ^= select & DELTAMASK;
        select= select >> DELTATABLE_BITS;
    }
    maskedFold = (previousHashHistory << 5) & DELTAMASK;     
    return maskedFold ^ foldedBits;
}

void AdaptiveDegree_Cycle(AccessStat *L1Data)
{
    AdaptiveDegree_Cycles++;
    // confidence is how many references there were to prefetched blocks to  L1 in the previous cycle
    // We use the variable below to determine determine L2 adaption degree
    if(in_cache(L1Data->mem_addr))
        L1AccessConfidence++;

    //// MAX_EPOCH_CYCLES is an epoch (given number of cycles).
    //// In an epoch, we define success on whether or not we have many hits to prefetches (confidence)
    //// At the end of an epoch we adjust our degree (gradient)
    //// See page 7 in ./doc/doc.pdf
    if (AdaptiveDegree_Cycles>MAX_EPOCH_CYCLES)
    {
       AdaptiveDegree_Cycles=0;
       if (L1AccessConfidence<FormerL1AccessConfidence)
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
       FormerL1AccessConfidence=L1AccessConfidence;
       L1AccessConfidence=0;
    }
}

void PDFCM_cycle(AccessStat *L2Data)
{
    Addr predicted_address;
    unsigned short hashHistory=StateHashHistory;

    if (((L2Data->miss == 1  && ! in_cache(L2Data->mem_addr)) ||
             get_prefetch_bit(L2Data->mem_addr)!=0)  )
    {
        if(L2Data->miss == 0)
            clear_prefetch_bit(L2Data->mem_addr);

        predicted_address = UpdateTables(L2Data->pc,
                            L2Data->mem_addr, &hashHistory);

        if(CurrentDegree)
        {
            // issue prefetch (if not filtered)
            if (predicted_address && predicted_address < MAX_PHYS_MEM_ADDR  &&
                    predicted_address!=L2Data->mem_addr &&
                    ! get_prefetch_bit(predicted_address)
                    && !in_mshr_queue(predicted_address & BITMASK_16) &&
                    ! in_cache(predicted_address)
            )
            {
                issue_prefetch(predicted_address);
                set_prefetch_bit(predicted_address);
            }
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
        && !get_prefetch_bit(predicted_address)
        && !in_mshr_queue(predicted_address & BITMASK_16) 
        && !in_cache(predicted_address))
        {
            issue_prefetch(predicted_address);
            set_prefetch_bit(predicted_address);
        }
        LastMissedAddress=StatePrediction & BITMASK_16;
        StatePrediction=predicted_address;
        StateDegree--;
    }
}

void prefetch_init(void)
{
    /* Called before any calls to prefetch_access. */
    /* This is the place to initialize data structures. */
    InitializeDatastructures();
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

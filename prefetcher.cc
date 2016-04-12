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
#define MSHR_SIZE 16
#define DELTA_MSHR_SIZE 16
#define BITMASK_16 (0xffffffff)
#define BITMASK 0xffffffff
#define READAHEAD 0x40 // 0x40 = 64 = 1000000 = 2^6
// #define BITMASK 0xffffffffffffffff
// #define BITMASK 0xffffffff
#define MAX_EPOCH_CYCLES (64*1024)
#define MAX_DEGREES (10)   // number of different degrees
#define DELTATABLE_BITS (10) // doesnt seem to matter much
#define DELTA_SIZE (1 << DELTATABLE_BITS) // multiply by 2^DELTATABLE_BITS, e.g. 1 << 1 = 2, 1 << 2 = 4, etc
#define DELTAMASK (DELTA_SIZE - 1)
#define HISTORYTABLE_BITS 8
#define HISTORY_TABLE_SIZE (1 << HISTORYTABLE_BITS)
#define HISTORYTABLE_MASK (HISTORY_TABLE_SIZE - 1)

typedef struct historyTablerecord historyTablerecord;
struct historyTablerecord // short: 16 bits
{
    int64_t PC;
    uint64_t lastAddrMiss;
    uint64_t hashHistory;
    char hitCount;
};
historyTablerecord * HistoryTable;  

typedef struct DeltaTableRecord DeltaTableRecord;
struct DeltaTableRecord
{
    int64_t delta; 
};
DeltaTableRecord * DeltaTable; 

void InitializeDatastructures (void);
Addr UpdateTables (Addr pc, Addr addr,
                   uint64_t
                   *hashHistory);       
Addr MakePrediction (Addr addr, uint64_t *hashHistory,
                         uint64_t lastAddrMiss);   
uint64_t CalculateHistoryMask (uint64_t previousHashHistory,
                           int64_t delta);
void PDFCM_cycle(AccessStat *L2Data);

uint64_t StateHashHistory;
uint64_t LastMissedAddress;
// We see our program as a state machine where one state represents
// a degree and an address
Addr StatePrediction;
char StateDegree=0;


void L1Prefetch(AccessStat *L1Data);
Addr L1LastAddress;
char L1LastDegree=0;

typedef struct MshrRecord MshrRecord;
struct MshrRecord
{
    Addr address;
    char isValid;
};
typedef struct CustomMshr CustomMshr;
struct CustomMshr
{
    short size;
    short tail;
    short num;
    short head;
    MshrRecord record[MSHR_SIZE];
};
CustomMshr * L1Mshr;
CustomMshr * L2Mshr;

int MshrLookup (CustomMshr * MSHR, Addr addr);
void MshrInsert (CustomMshr * MSHR, Addr addr);
void MshrIteration (); 

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

void AdaptiveDegree_Cycle(AccessStat *L1Data);

void InitializeDatastructures (void)
{
    HistoryTable=(historyTablerecord *) calloc(HISTORY_TABLE_SIZE, sizeof(historyTablerecord));
    DeltaTable=(DeltaTableRecord *) calloc(DELTA_SIZE, sizeof(DeltaTableRecord));

    L1Mshr=(CustomMshr *) calloc(1, sizeof(CustomMshr));
    L1Mshr->size=MSHR_SIZE;
    L2Mshr=(CustomMshr *) calloc(1, sizeof(CustomMshr));
    L2Mshr->size=MSHR_SIZE;
}

Addr UpdateTables (Addr pc, Addr addr, uint64_t *hashHistory)
{
    uint64_t currentHistory;
    unsigned int index = pc & HISTORYTABLE_MASK;
    uint64_t previousAddress = HistoryTable[index].lastAddrMiss;
    uint64_t previousHashHistory = HistoryTable[index].hashHistory;
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
    int64_t predictedDelta = DeltaTable[previousHashHistory].delta;
    int64_t actualDelta = (addr-previousAddress) & BITMASK_16;
    if (actualDelta==predictedDelta)
    {
        if (confidence<3) { confidence++; }
    }
    else {
        if (confidence>0) { confidence--; }
    }

    *hashHistory   = currentHistory   = CalculateHistoryMask(previousHashHistory, actualDelta);
    // write HistoryTable record
    HistoryTable[index].lastAddrMiss = (uint64_t)(addr & BITMASK_16); // current missed address
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

Addr MakePrediction (Addr addr, uint64_t *hashHistory,
                         uint64_t old_lastAddrMiss)
{
    // compute delta
    int64_t delta = (addr-old_lastAddrMiss) & BITMASK_16;
    // compute new hashHistory
    *hashHistory=CalculateHistoryMask(*hashHistory, delta);
    // predict a new delta using the new hashHistory
    return addr + DeltaTable[*hashHistory].delta;
}

// See the following, bottom page 6 and top 7
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.92.6198&rep=rep1&type=pdf
uint64_t CalculateHistoryMask (uint64_t previousHashHistory, int64_t delta)
{
    uint64_t foldedBits, maskedFold;
    uint64_t select=delta; // note: this is a cast
    for(foldedBits=0; select;)
    {
        foldedBits ^= select & DELTAMASK;
        select= select >> DELTATABLE_BITS;
    }
    maskedFold = (previousHashHistory << 5) & DELTAMASK;     
    return maskedFold ^ foldedBits;
}

int MshrLookup (CustomMshr * Mshr, Addr address)
{
    Addr MASK = 0x3f;
    if (!Mshr->size || !Mshr->num)
        return 0;

    for (short i=0; i < Mshr->size; i++)
        if (Mshr->record[i].isValid 
        && (Mshr->record[i].address == (address & ~MASK)) )
            return 1;
    return 0;
}
void MshrInsert (CustomMshr * Mshr, Addr addr)
{
    if (!Mshr->size || !addr)
        return;
    Addr MASK = 0x3f;

    Mshr->record[Mshr->tail].isValid=1;
    Mshr->record[Mshr->tail].address= (addr & ~MASK);
    Mshr->tail=(Mshr->tail+1)%Mshr->size;
    if (Mshr->num < Mshr->size)
        Mshr->num++;
    else
        Mshr->head=Mshr->tail;
}

void MshrIteration ()
{
    // if (L1Mshr->num && (get_prefetch_bit(L1Mshr->record[L1Mshr->head].address) != 0 ))
    // {
    //     L1Mshr->num--;
    //     L1Mshr->record[L1Mshr->head].isValid=0;
    //     L1Mshr->head=(L1Mshr->head+1)%L1Mshr->size;
    // }
    if (L2Mshr->num && (get_prefetch_bit(L2Mshr->record[L2Mshr->head].address) != 0) )
    {
        L2Mshr->num--;
        L2Mshr->record[L2Mshr->head].isValid=0;
        L2Mshr->head=(L2Mshr->head+1)%L2Mshr->size;
    }
}

struct AccessStat accessStatHistory[LOOKBACK_DEGREE];

void AdaptiveDegree_Cycle(AccessStat *L1Data)
{
    AdaptiveDegree_Cycles++;
    // confidence is how many references there were to prefetched blocks to  L1 in the previous cycle
    // Optimally, we would have an instruction counter, but sim framework is limited
    // We use the variable below to determine determine L2 adaption degree
    for(int i = 0; i < LOOKBACK_DEGREE; i++)
       // if(L1Data->time == accessStatHistory[i].time)
       if(in_cache(L1Data->mem_addr)
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
           if (CurrentDegreeIndex>0) 
               CurrentDegreeIndex--;
       CurrentDegree=DegreeList[CurrentDegreeIndex];
       FormerL1AccessConfidence=L1AccessConfidence;
       L1AccessConfidence=0;
    }
}

void L1Prefetch(AccessStat *L1Data)
{
    for (int i=0; i<LOOKBACK_DEGREE; i++)
    {
        if (L1Data->time == accessStatHistory[i].time && (accessStatHistory[i].miss == 1 ||
                get_prefetch_bit(accessStatHistory[i].mem_addr)!=0) )
        {
            if (accessStatHistory[i].miss == 0)
                clear_prefetch_bit(accessStatHistory[i].mem_addr);
    
            L1LastAddress=accessStatHistory[i].mem_addr;
            L1LastDegree=(accessStatHistory[i].miss == 0 ? 1 : 4 );
        } 
    } 
    if (L1LastDegree)
    {
        // It makes little sence to prefetch the current or adjacent address
        // Therefore we add an offset (trivially READAHEAD) to make sure we get
        // an address at the correct time
        Addr predicted_address= L1LastAddress+READAHEAD; 
        // issue prefetch (if not filtered)
        if (get_prefetch_bit(predicted_address) == 0
        // && !MshrLookup(L1Mshr,predicted_address & BITMASK ))
        && !MshrLookup(L2Mshr,predicted_address & BITMASK ))
        {
            if(predicted_address < MAX_PHYS_MEM_ADDR )
            {
                issue_prefetch(predicted_address);
                set_prefetch_bit(predicted_address);
                MshrInsert(L2Mshr,predicted_address & BITMASK);
            }
        }
        L1LastAddress=predicted_address;
        L1LastDegree--;
    }
}
void PDFCM_cycle(AccessStat *L2Data)
{
    Addr predicted_address;
    uint64_t hashHistory=StateHashHistory;

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
            if (predicted_address && predicted_address!=L2Data->mem_addr &&
                    ! get_prefetch_bit(predicted_address)
                && !MshrLookup(L2Mshr,predicted_address & BITMASK) &&
                    ! in_cache(predicted_address)
                    && predicted_address < MAX_PHYS_MEM_ADDR )
            {
                issue_prefetch(predicted_address);
                set_prefetch_bit(predicted_address);
                MshrInsert(L2Mshr,predicted_address & BITMASK);
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
        if (predicted_address && predicted_address!=StatePrediction &&
                !get_prefetch_bit(predicted_address)
                &&
                !MshrLookup(L2Mshr,predicted_address & BITMASK) &&
                !in_cache(predicted_address)
                && predicted_address < MAX_PHYS_MEM_ADDR )
            {
                issue_prefetch(predicted_address);
                set_prefetch_bit(predicted_address);
                MshrInsert(L2Mshr,predicted_address & BITMASK);
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
    struct AccessStat *tmp = (struct AccessStat*)malloc(sizeof(struct AccessStat));
    tmp->time = -1;
    tmp->pc = -1;
    tmp->mem_addr = -1;
    tmp->miss = -1;
    for(int i = 0; i < LOOKBACK_DEGREE; i++)
        accessStatHistory[i] = *tmp;
}

int myIndex = 0;
void prefetch_access(AccessStat stat)
{
    MshrIteration();

    AccessStat *ptr = &stat;

    AdaptiveDegree_Cycle(ptr);
    L1Prefetch(ptr);
    PDFCM_cycle(ptr);

    myIndex++;

    /* if(accessStatHistory[myIndex%4] != NULL)
         free(accessStatHistory[myIndex%4] ); */
   accessStatHistory[(myIndex)%LOOKBACK_DEGREE] = stat;
}

void prefetch_complete(Addr addr)
{ /* Intentionally blank */ }

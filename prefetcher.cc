// submissions only allow one file, so this file is a result
// of merging a header file on top of the source cpp file

// this file still needs more adaption to make sense,
// but gives a rough overview of things.

// Prefetch bit / tag - request by demand of "intelligence"?
// PDFCM : Prefetch differential finite context machine
// PMAF: Prefetch Memory Address File
// MSHR and PMAF are both FIFO structures

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "interface.hh"


//TODO: experiment with these numbers
#define LOOKBACK_DEGREE 4
#define MSHR_SIZE 128
#define DELTA_MSHR_SIZE 16
#define BITMASK_16 (0xffff)
#define MAX_EPOCH_CYCLES (64*1024) // TODO: next to modify!
#define MAX_DEGREES (13)   // number of different degrees
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
    int size;
    int tail;
    int num;
    int head;
    MshrRecord record[MSHR_SIZE];
};
CustomMshr * L1Mshr;
CustomMshr * DeltaMshr;
CustomMshr * L2Mshr;

int MshrLookup (CustomMshr * MSHR, Addr addr);
void MshrInsert (CustomMshr * MSHR, Addr addr);
void MshrIteration (); 

unsigned int AdaptiveDegree_Cycles=0;
Tick L1AccessConfidence=0;
Tick FormerL1AccessConfidence=0;
int DegreeList[MAX_DEGREES]= {0,1,2,3,4,6,8,12,16,24,32,48,64};
int CurrentDegreeIndex=4;
int CurrentDegree=4;
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
    DeltaMshr=(CustomMshr *) calloc(1, sizeof(CustomMshr));
    DeltaMshr->size=DELTA_MSHR_SIZE;
    L2Mshr=(CustomMshr *) calloc(1, sizeof(CustomMshr));
    L2Mshr->size=MSHR_SIZE;
}

Addr UpdateTables (Addr pc, Addr addr, unsigned short *hashHistory)
{
    unsigned short currentMissedAddress;
    unsigned short currentHistory;
    short actualDelta, predictedDelta;
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
    predictedDelta = DeltaTable[previousHashHistory].delta;
    actualDelta = (addr-previousAddress) & BITMASK_16;
    if (actualDelta==predictedDelta)
    {
        if (confidence<3) { confidence++; }
    }
    else {
        if (confidence>0) { confidence--; }
    }

    currentMissedAddress = addr & BITMASK_16;
    *hashHistory   = currentHistory   = CalculateHistoryMask(previousHashHistory, actualDelta);
    // write HistoryTable record
    HistoryTable[index].lastAddrMiss = currentMissedAddress;
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
    short delta;
    // compute delta
    delta = (addr-old_lastAddrMiss) & BITMASK_16;
    // compute new hashHistory
    *hashHistory=CalculateHistoryMask(*hashHistory, delta);
    // predict a new delta using the new hashHistory
    return addr + DeltaTable[*hashHistory].delta;
}

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

int MshrLookup (CustomMshr * Mshr, Addr address)
{
    Addr MASK = 0x3f;
    if (!Mshr->size || !Mshr->num)
        return 0;

    for (int i=0; i < Mshr->size; i++)
        if (Mshr->record[i].isValid 
        && (Mshr->record[i].address == (address & ~MASK)) )
            return 1;
    return 0;
}
void MshrInsert (CustomMshr * Mshr, Addr addr)
{
    Addr MASK = 0x3f;
    if (!Mshr->size || !addr)
        return;
    Mshr->record[Mshr->tail].isValid=1;
    Mshr->record[Mshr->tail].address= (addr & ~MASK);
    Mshr->tail=(Mshr->tail+1)%Mshr->size;
    if (Mshr->num<Mshr->size)
        Mshr->num++;
    else
        Mshr->head=Mshr->tail;
}

void MshrIteration ()
{
    if (L1Mshr->num && (get_prefetch_bit(L1Mshr->record[L1Mshr->head].address) != 0 ))
    {
        L1Mshr->num--;
        L1Mshr->record[L1Mshr->head].isValid=0;
        L1Mshr->head=(L1Mshr->head+1)%L1Mshr->size;
    }
    if (DeltaMshr->num && (get_prefetch_bit(DeltaMshr->record[DeltaMshr->head].address) != 0) )
    {
        DeltaMshr->num--;
        DeltaMshr->record[DeltaMshr->head].isValid=0;
        DeltaMshr->head=(DeltaMshr->head+1)%DeltaMshr->size;
    }
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
       if(L1Data->time == accessStatHistory[i].time)
           L1AccessConfidence++;

    //// MAX_EPOCH_CYCLES is an epoch (given number of cycles).
    //// In an epoch, we define success on whether or not we have many hits to prefetches.
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
    // degree is trivially set to 4, needs some  experimenting.
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
        Addr predicted_address= L1LastAddress+0x40;
        // issue prefetch (if not filtered)
        if (get_prefetch_bit(predicted_address) == 0
        && !MshrLookup(L1Mshr,predicted_address & 0xffff))
        {
            if(predicted_address < MAX_PHYS_MEM_ADDR )
            {
                issue_prefetch(predicted_address);
                set_prefetch_bit(predicted_address);
                MshrInsert(L1Mshr,predicted_address & 0xffff);
            }
        }
        L1LastAddress=predicted_address;
        L1LastDegree--;
    }
}
void PDFCM_cycle(AccessStat *L2Data)
{
    Addr predicted_address;
    unsigned short hashHistory=StateHashHistory;

    if (((L2Data->miss == 1  && !MshrLookup(DeltaMshr, L2Data->mem_addr)) ||
             get_prefetch_bit(L2Data->mem_addr)!=0)  )
    {
        if (L2Data->miss == 1)
            MshrInsert(DeltaMshr, L2Data->mem_addr);
        else
            clear_prefetch_bit(L2Data->mem_addr);

        predicted_address = UpdateTables(L2Data->pc,
                            L2Data->mem_addr, &hashHistory);

        if(CurrentDegree)
        {
            // issue prefetch (if not filtered)
            if (predicted_address && predicted_address!=L2Data->mem_addr &&
                    ! get_prefetch_bit(predicted_address)
                && !MshrLookup(L2Mshr,predicted_address & 0xffff) &&
                    !MshrLookup(DeltaMshr, predicted_address)
                    && predicted_address < MAX_PHYS_MEM_ADDR )
            {
                issue_prefetch(predicted_address);
                set_prefetch_bit(predicted_address);
                MshrInsert(L2Mshr,predicted_address & 0xffff);
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
                !MshrLookup(L2Mshr,predicted_address & 0xffff) &&
                !MshrLookup(DeltaMshr,predicted_address)
                && predicted_address < MAX_PHYS_MEM_ADDR )
            {
                issue_prefetch(predicted_address);
                set_prefetch_bit(predicted_address);
                MshrInsert(L2Mshr,predicted_address & 0xffff);
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

    // if(accessStatHistory[myIndex%4] != NULL)
    //     free(accessStatHistory[myIndex%4] );
   accessStatHistory[(myIndex)%4] = stat;
}

void prefetch_complete(Addr addr)
{ /* intentionally blank */ }

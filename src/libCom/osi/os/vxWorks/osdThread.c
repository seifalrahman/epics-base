/* osi/os/vxWorks/osiThread.c */

/* Author:  Marty Kraimer Date:    25AUG99 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include <vxWorks.h>
#include <taskLib.h>
#include <taskVarLib.h>
#include <sysLib.h>
/* The following not defined in an vxWorks header */
int sysClkRateGet(void);

#include "errlog.h"
#include "ellLib.h"
#include "osiThread.h"
#include "cantProceed.h"
#include "epicsAssert.h"
#include "vxLib.h"


#if CPU_FAMILY == MC680X0
#define ARCH_STACK_FACTOR 1
#elif CPU_FAMILY == SPARC
#define ARCH_STACK_FACTOR 2
#else
#define ARCH_STACK_FACTOR 2
#endif
static const unsigned stackSizeTable[threadStackBig+1] = 
   {4000*ARCH_STACK_FACTOR, 6000*ARCH_STACK_FACTOR, 11000*ARCH_STACK_FACTOR};

/* definitions for implementation of threadPrivate */
static void **papTSD = 0;
static int nthreadPrivate = 0;

static SEM_ID threadOnceMutex = 0;

/* Just map osi 0 to 99 into vx 100 to 199 */
/* remember that for vxWorks lower number means higher priority */
/* vx = 100 + (99 -osi)  = 199 - osi*/
/* osi =  199 - vx */

static unsigned int getOsiPriorityValue(int ossPriority)
{
    return(199-ossPriority);
}

static int getOssPriorityValue(unsigned int osiPriority)
{
    return(199 - osiPriority);
}


unsigned int threadGetStackSize (threadStackSizeClass stackSizeClass) 
{

    if (stackSizeClass<threadStackSmall) {
        errlogPrintf("threadGetStackSize illegal argument (too small)");
        return stackSizeTable[threadStackBig];
    }

    if (stackSizeClass>threadStackBig) {
        errlogPrintf("threadGetStackSize illegal argument (too large)");
        return stackSizeTable[threadStackBig];
    }

    return stackSizeTable[stackSizeClass];
}

void threadInit(void)
{
    static int lock = 0;

    while(!vxTas(&lock)) taskDelay(1);
    if(threadOnceMutex==0) {
        threadOnceMutex = semMCreate(
                SEM_DELETE_SAFE|SEM_INVERSION_SAFE|SEM_Q_PRIORITY);
        assert(threadOnceMutex);
    }
    lock = 0;
}

void threadOnceOsd(threadOnceId *id, void (*func)(void *), void *arg)
{
    threadInit();
    assert(semTake(threadOnceMutex,WAIT_FOREVER)==OK);
    if (*id == 0) { /*  0 => first call */
        *id = -1;   /* -1 => func() active */
        func(arg);
        *id = +1;   /* +1 => func() done (see threadOnce() macro defn) */
    }
    semGive(threadOnceMutex);
}

static void createFunction(THREADFUNC func, void *parm)
{
    int tid = taskIdSelf();

    taskVarAdd(tid,(int *)&papTSD);
    papTSD = 0;
    (*func)(parm);
    taskVarDelete(tid,(int *)&papTSD);
    free(papTSD);
}

threadId threadCreate(const char *name,
    unsigned int priority, unsigned int stackSize,
    THREADFUNC funptr,void *parm)
{
    int tid;
    if(stackSize<100) {
        errlogPrintf("threadCreate %s illegal stackSize %d\n",name,stackSize);
        return(0);
    }
    tid = taskSpawn((char *)name,getOssPriorityValue(priority),
        VX_FP_TASK, stackSize,
        (FUNCPTR)createFunction,(int)funptr,(int)parm,
        0,0,0,0,0,0,0,0);
    if(tid==0) {
        errlogPrintf("threadCreate taskSpawn failure for %s\n",name);
        return(0);
    }
    return((threadId)tid);
}

void threadSuspendSelf()
{
    STATUS status;

    status = taskSuspend(taskIdSelf());
    if(status) errlogPrintf("threadSuspendSelf failed\n");
}

void threadResume(threadId id)
{
    int tid = (int)id;
    STATUS status;

    status = taskResume(tid);
    if(status) errlogPrintf("threadResume failed\n");
}

void threadExitMain(void)
{
    errlogPrintf("threadExitMain was called for vxWorks. Why?\n");
}

unsigned int threadGetPriority(threadId id)
{
    int tid = (int)id;
    STATUS status;
    int priority = 0;

    status = taskPriorityGet(tid,&priority);
    if(status) errlogPrintf("threadGetPriority failed\n");
    return(getOsiPriorityValue(priority));
}

unsigned int threadGetPrioritySelf(void)
{
    return(threadGetPriority(threadGetIdSelf()));
}

void threadSetPriority(threadId id,unsigned int osip)
{
    int tid = (int)id;
    STATUS status;
    int priority = 0;

    priority = getOssPriorityValue(osip);
    status = taskPrioritySet(tid,priority);
    if(status) errlogPrintf("threadSetPriority failed\n");
}

threadBoolStatus threadHighestPriorityLevelBelow(
    unsigned int priority, unsigned *pPriorityJustBelow)
{
    unsigned newPriority = priority - 1;
    if (newPriority <= 99) {
        *pPriorityJustBelow = newPriority;
        return tbsSuccess;
    }
    return tbsFail;
}

threadBoolStatus threadLowestPriorityLevelAbove(
    unsigned int priority, unsigned *pPriorityJustAbove)
{
    unsigned newPriority = priority + 1;

    newPriority = priority + 1;
    if (newPriority <= 99) {
        *pPriorityJustAbove = newPriority;
        return tbsSuccess;
    }
    return tbsFail;
}

int threadIsEqual(threadId id1, threadId id2)
{
    return((id1==id2) ? 1 : 0);
}

int threadIsSuspended(threadId id)
{
    int tid = (int)id;
    return((int)taskIsSuspended(tid));
}

void threadSleep(double seconds)
{
    STATUS status;

    status = taskDelay((int)(seconds*sysClkRateGet()));
    if(status) errlogPrintf(0,"threadSleep\n");
}

threadId threadGetIdSelf(void)
{
    return((threadId)taskIdSelf());
}

threadId threadGetId(const char *name)
{
    int tid = taskNameToId((char *)name);
    return((threadId)(tid==ERROR?0:tid));
}

const char *threadGetNameSelf(void)
{
    return taskName(taskIdSelf());
}

void threadGetName (threadId id, char *name, size_t size)
{
    int tid = (int)id;
    strncpy(name,taskName(tid),size-1);
    name[size-1] = '\0';
}

void threadShowAll(unsigned int level)
{
    taskShow(0,2);
}

void threadShow(threadId id,unsigned int level)
{
    int tid = (int)id;
    taskShow(tid,level);
}

/* The following algorithm was thought of by Andrew Johnson APS/ASD .
 * The basic idea is to use a single vxWorks task variable.
 * The task variable is papTSD, which is an array of pointers to the TSD
 * The array size is equal to the number of threadPrivateIds created + 1
 * when threadPrivateSet is called.
 * Until the first call to threadPrivateCreate by a application papTSD=0
 * After first call papTSD[0] is value of nthreadPrivate when 
 * threadPrivateSet was last called by the thread. This is also
 * the value of threadPrivateId.
 * The algorithm allows for threadPrivateCreate being called after
 * the first call to threadPrivateSet.
 */
threadPrivateId threadPrivateCreate()
{
    static int lock = 0;
    threadPrivateId id;

    threadInit();
    /*lock is necessary because ++nthreadPrivate may not be indivisible*/
    while(!vxTas(&lock)) taskDelay(1);
    id = (threadPrivateId)++nthreadPrivate;
    lock = 0;
    return(id);
}

void threadPrivateDelete(threadPrivateId id)
{
    /*nothing to delete */
    return;
}

/*
 * Note that it is not necessary to have mutex for following
 * because they must be called by the same thread
 */
void threadPrivateSet (threadPrivateId id, void *pvt)
{
    int indpthreadPrivate = (int)id;

    if(!papTSD) {
        papTSD = callocMustSucceed(indpthreadPrivate + 1,sizeof(void *),
            "threadPrivateSet");
        papTSD[0] = (void *)(indpthreadPrivate);
    } else {
        int nthreadPrivate = (int)papTSD[0];
        if(nthreadPrivate < indpthreadPrivate) {
            void **ptemp;
            ptemp = realloc(papTSD,(indpthreadPrivate+1)*sizeof(void *));
            if(!ptemp) cantProceed("threadPrivateSet realloc failed\n");
            papTSD = ptemp;
            papTSD[0] = (void *)(indpthreadPrivate);
        }
    }
    papTSD[indpthreadPrivate] = pvt;
}

void *threadPrivateGet(threadPrivateId id)
{
    int indpthreadPrivate = (int)id;
    void *data;
    if(!papTSD) {
        papTSD = callocMustSucceed(indpthreadPrivate + 1,sizeof(void *),
            "threadPrivateSet");
        papTSD[0] = (void *)(indpthreadPrivate);
    }
    if ( (int) id <= (int) papTSD[0] ) {
        data = papTSD[(int)id];
    }
    else {
        data = 0;
    }
    return(data);
}

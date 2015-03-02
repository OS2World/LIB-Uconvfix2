#define INCL_BASE
#define INCL_DOS
#define INCL_DOSEXCEPTIONS
#define INCL_ERRORS
#include <os2.h>
#include <uconv.h>

#include <stdint.h>

static const char MUTEX_NAME[] = "\\SEM32\\UCONVFIX2_MUTEX";

static HMTX m_hMtx = NULLHANDLE;

int CALLCONV U32CreateUconvObject(UniChar* code_set,  UconvObject* uobj);
int CALLCONV U32FreeUconvObject(UconvObject uobj);
int CALLCONV _U32Malloc(UconvObject uobj);
int CALLCONV _U32Free(UconvObject uobj);

unsigned long _System UconvWrapInitTerm(unsigned long modHandle, unsigned long stageFlag)
{
    switch (stageFlag)
    {
    case 0: // init
        {
            APIRET rc = DosOpenMutexSem(MUTEX_NAME, &m_hMtx);
            if (rc == NO_ERROR)
            {
                break; // mutex already exists
            }

            if (rc != ERROR_SEM_NOT_FOUND)
            {
                // some unrecoverable error
                return 0;
            }

            // no mutex in the system. Let's create it
            rc = DosCreateMutexSem(
                MUTEX_NAME, // Named-shared semaphore
                &m_hMtx,
                0,          // sharing flag. Ignored
                FALSE);     // Initially unowned

            if (rc == NO_ERROR)
            {
                break; // mutex successfully created
            }

            if (rc != ERROR_DUPLICATE_NAME) {
                // some unrecoverable error
                return 0;
            }

            // OS/2 module loader serializes int/term calls so it should not be
            // possible to create it concurrently but let's open it one time more
            // for sanity
            m_hMtx = NULLHANDLE; // this function requires zero handle on input. Otherwise it always returns BAD_PARAMETER
            rc = DosOpenMutexSem(MUTEX_NAME, &m_hMtx);
            if (rc != NO_ERROR)
            {
                // still an error. Hands up.
                return 0;
            }
        }
        break;
    case 1: // term
        {
            PID ownerPid;
            TID ownerTid;
            ULONG reqCount;
            APIRET rc = DosQueryMutexSem(m_hMtx, &ownerPid, &ownerTid, &reqCount);
            if (rc == NO_ERROR)
            {
                PPIB pPIB = 0;
                PTIB pTIB = 0;
                DosGetInfoBlocks(&pTIB, &pPIB);

                // check only PID. Termination handler is always executed on
                // thread 1 but mutex owner may was on other thread
                if (ownerPid == (PID)pPIB->pib_ulpid)
                {
                    // the mutex was owned by this process. Let's release it to avoid OWNER_DIED state
                    for (; reqCount > 0; --reqCount)
                    {
                        DosReleaseMutexSem(m_hMtx);
                    }
                }
            }

            // sanity for case process hang in an exit list. In normal case the unowned mutex is closed by system
            DosCloseMutexSem(m_hMtx);
        }
        break;
    default:
        return 0; // should not happen
    }

    return 1; // init/term done
}

static inline void request(void)
{
    // this is just another layer of protection for uconv system so we allow
    // access even if some shit is happened with mutex. Too many important apps
    // relies on uconv and error return will make them unusable. Well we do the
    // best to avoid mutex going to OWNER_DIED state.
    APIRET rc = DosRequestMutexSem(m_hMtx, (ULONG)SEM_INDEFINITE_WAIT);
}

static inline void release( void )
{
    DosReleaseMutexSem(m_hMtx);
}

static ULONG _System excHandler(
    PEXCEPTIONREPORTRECORD pERepRec,
    PEXCEPTIONREGISTRATIONRECORD pERegRec,
    PCONTEXTRECORD pCtxRec,
    PVOID sysinternal)
{
    if (pERepRec->fHandlerFlags & EH_UNWINDING)
    {
        // unwinding in progress. The mutex will not be released by normal
        // means so we have to release it here
        release();
    }

    // pass to next EH
    return XCPT_CONTINUE_SEARCH;
}

int CALLCONV UniCreateUconvObject(UniChar* cpName, UconvObject* pUobj)
{
    EXCEPTIONREGISTRATIONRECORD cerr = {0, &excHandler};
    DosSetExceptionHandler(&cerr);

    request();
    int rc = U32CreateUconvObject(cpName, pUobj);
    release();

    DosUnsetExceptionHandler(&cerr);
    return rc;
}

int CALLCONV UniFreeUconvObject(UconvObject uobj)
{
    EXCEPTIONREGISTRATIONRECORD cerr = {0, &excHandler};
    DosSetExceptionHandler(&cerr);

    request();
    int rc = U32FreeUconvObject(uobj);
    release();

    DosUnsetExceptionHandler(&cerr);
    return rc;
}

int CALLCONV _UniMalloc(UconvObject uobj)
{
    EXCEPTIONREGISTRATIONRECORD cerr = {0, &excHandler};
    DosSetExceptionHandler(&cerr);

    request();
    int rc = _U32Malloc(uobj);
    release();

    DosUnsetExceptionHandler(&cerr);
    return rc;
}

int CALLCONV _UniFree(UconvObject uobj)
{
    EXCEPTIONREGISTRATIONRECORD cerr = {0, &excHandler};
    DosSetExceptionHandler(&cerr);

    request();
    int rc = _U32Free(uobj);
    release();

    DosUnsetExceptionHandler(&cerr);
    return rc;
}

#import zlib.c
#import inflate.c
#import stream.c
#import deflate.c

// Represents a 64-bit signed integer value.
typedef {
    uint32_t LowPart;
    int32_t  HighPart;
} LARGE_INTEGER;

void MyDoMinus64(LARGE_INTEGER *R, A, B) {
    R->HighPart = A.HighPart - B.HighPart;
    if (A.LowPart >= B.LowPart)
        R->LowPart = A.LowPart - B.LowPart;
    else
    {
        R->LowPart = A.LowPart - B.LowPart;
        R->HighPart --;
    }
}

// see http://msdn2.microsoft.com/library/twchhe95(en-us,vs.80).aspx for __rdtsc
void BeginCountRdtsc(LARGE_INTEGER * pbeginTime64)
{
 //   printf("rdtsc = %I64x\n",__rdtsc());
   pbeginTime64->QuadPart=__rdtsc();
}

LARGE_INTEGER GetResRdtsc(LARGE_INTEGER beginTime64, bool fComputeTimeQueryPerf)
{
    LARGE_INTEGER LIres;
    uint64_t res=__rdtsc()-((uint64_t)(beginTime64.QuadPart));
    LIres.QuadPart=res;
   // printf("rdtsc = %I64x\n",__rdtsc());
    return LIres;
}

void BeginCountPerfCounter(LARGE_INTEGER * pbeginTime64,bool fComputeTimeQueryPerf)
{
    if ((!fComputeTimeQueryPerf) || (!QueryPerformanceCounter(pbeginTime64)))
    {
        pbeginTime64->LowPart = GetTickCount();
        pbeginTime64->HighPart = 0;
    }
}

uint32_t GetMsecSincePerfCounter(LARGE_INTEGER beginTime64,bool fComputeTimeQueryPerf)
{
    LARGE_INTEGER endTime64;
    LARGE_INTEGER ticksPerSecond;
    LARGE_INTEGER ticks;
    uint64_t ticksShifted;
    uint64_t tickSecShifted;
    uint32_t dwLog=16+0;
    uint32_t dwRet;
    if ((!fComputeTimeQueryPerf) || (!QueryPerformanceCounter(&endTime64)))
        dwRet = (GetTickCount() - beginTime64.LowPart)*1;
    else
    {
        MyDoMinus64(&ticks,endTime64,beginTime64);
        QueryPerformanceFrequency(&ticksPerSecond);
        ticksShifted = Int64ShrlMod32(*(uint64_t*)&ticks,dwLog);
        tickSecShifted = Int64ShrlMod32(*(uint64_t*)&ticksPerSecond,dwLog);
        dwRet = (uint32_t)((((uint32_t)ticksShifted)*1000)/(uint32_t)(tickSecShifted));
        dwRet *=1;
    }
    return dwRet;
}

int ReadFileMemory(const char *filename, int32_t *plFileSize, uint8_t **pFilePtr) {
    FILE *stream = fopen(filename, "rb");
    if (!stream) {
        return 0;
    }

    fseek(stream,0,SEEK_END);

    *plFileSize=ftell(stream);
    fseek(stream,0,SEEK_SET);
    uint8_t *ptr = calloc((*plFileSize)+1, 1);
    if (!ptr) {
        fclose(stream);
        *pFilePtr = NULL;
        return 0;
    }

    int retVal=1;
    if (fread(ptr, 1, *plFileSize,stream) != (*plFileSize)) {
        retVal=0;
    }
    fclose(stream);
    *pFilePtr=ptr;
    return retVal;
}

int main(int argc, char *argv[])
{
    if (argc <= 1) {
        printf("run TestZlib <File> [BlockSizeCompress] [BlockSizeUncompress] [compres. level]\n");
        return 0;
    }

    int BlockSizeCompress=0x8000;
    int BlockSizeUncompress=0x8000;
    int cprLevel = zlib.Z_DEFAULT_COMPRESSION;
    if (argc>=3) {
        BlockSizeCompress=atol(argv[2]);
    }
    if (argc>=4) {
        BlockSizeUncompress=atol(argv[3]);
    }
    if (argc>=5) {
        cprLevel=(int)atol(argv[4]);
    }

    int32_t lFileSize = 0;
    uint8_t* FilePtr = NULL;
    if (ReadFileMemory(argv[1], &lFileSize, &FilePtr) == 0) {
        printf("error reading %s\n", argv[1]);
        return 1;
    }
    printf("file %s read, %u bytes\n",argv[1], lFileSize);

    int32_t lBufferSizeCpr = lFileSize + (lFileSize/0x10) + 0x200;
    uint8_t *CprPtr = calloc(1, lBufferSizeCpr + BlockSizeCompress);

    int32_t lBufferSizeUncpr = lBufferSizeCpr;
    // int32_t lCompressedSize=0;    
    uint8_t* UncprPtr;
    int32_t lSizeCpr;
    int32_t lSizeUncpr;
    uint32_t dwGetTick;
    uint32_t dwMsecQP;
    LARGE_INTEGER li_qp;
    LARGE_INTEGER li_rdtsc;
    LARGE_INTEGER dwResRdtsc;
    

    BeginCountPerfCounter(&li_qp, true);
    dwGetTick=GetTickCount();
    BeginCountRdtsc(&li_rdtsc);
    stream.z_stream zcpr;
    int ret=stream.Z_OK;
    int32_t lOrigToDo = lFileSize;
    int32_t lOrigDone = 0;
    int step=0;
    memset(&zcpr,0,sizeof(stream.z_stream));
    deflate.deflateInit(&zcpr,cprLevel);

    zcpr.next_in = FilePtr;
    zcpr.next_out = CprPtr;


    while (true) {
        int32_t all_read_before = zcpr.total_in;
        zcpr.avail_in = min(lOrigToDo,BlockSizeCompress);
        zcpr.avail_out = BlockSizeCompress;
        if (zcpr.avail_in==lOrigToDo) {
            ret = deflate.deflate(&zcpr, stream.Z_FINISH);
        } else {
            ret = deflate.deflate(&zcpr, stream.Z_SYNC_FLUSH);
        }
        lOrigDone += (zcpr.total_in-all_read_before);
        lOrigToDo -= (zcpr.total_in-all_read_before);
        step++;
        bool cont = (ret==stream.Z_OK);
        if (!cont) break;
    }

    lSizeCpr=zcpr.total_out;
    deflate.deflateEnd(&zcpr);
    dwGetTick=GetTickCount()-dwGetTick;
    dwMsecQP=GetMsecSincePerfCounter(li_qp, true);
    dwResRdtsc=GetResRdtsc(li_rdtsc, true);
    printf("total compress size = %u, in %u step\n",lSizeCpr,step);
    printf("time = %u msec = %f sec\n",dwGetTick,dwGetTick/(double)1000.);
    printf("defcpr time QP = %u msec = %f sec\n",dwMsecQP,dwMsecQP/(double)1000.);
    printf("defcpr result rdtsc = %I64x\n\n",dwResRdtsc.QuadPart);

    CprPtr=(uint8_t*)realloc(CprPtr,lSizeCpr);
    UncprPtr=(uint8_t*) calloc(1, lBufferSizeUncpr + BlockSizeUncompress);

    BeginCountPerfCounter(&li_qp, true);
    dwGetTick=GetTickCount();
    BeginCountRdtsc(&li_rdtsc);
    
    stream.z_stream zcpr;
    int ret=stream.Z_OK;
    int32_t lOrigToDo = lSizeCpr;
    int32_t lOrigDone = 0;
    int step=0;
    memset(&zcpr,0,sizeof(stream.z_stream));
    inflate.inflateInit(&zcpr);

    zcpr.next_in = CprPtr;
    zcpr.next_out = UncprPtr;

    while (true) {
        int32_t all_read_before = zcpr.total_in;
        zcpr.avail_in = min(lOrigToDo,BlockSizeUncompress);
        zcpr.avail_out = BlockSizeUncompress;
        ret = inflate.inflate(&zcpr,stream.Z_SYNC_FLUSH);
        lOrigDone += (zcpr.total_in-all_read_before);
        lOrigToDo -= (zcpr.total_in-all_read_before);
        step++;
        if (ret != stream.Z_OK) {
            break;
        }
    }

    lSizeUncpr=zcpr.total_out;
    inflate.inflateEnd(&zcpr);
    dwGetTick=GetTickCount()-dwGetTick;
    dwMsecQP=GetMsecSincePerfCounter(li_qp, true);
    dwResRdtsc=GetResRdtsc(li_rdtsc, true);
    printf("total uncompress size = %u, in %u step\n",lSizeUncpr,step);
    printf("time = %u msec = %f sec\n",dwGetTick,dwGetTick/(double)1000.);
    printf("uncpr  time QP = %u msec = %f sec\n",dwMsecQP,dwMsecQP/(double)1000.);
    printf("uncpr  result rdtsc = %I64x\n\n",dwResRdtsc.QuadPart);
    

    if (lSizeUncpr==lFileSize)
    {
        if (memcmp(FilePtr,UncprPtr,lFileSize)==0)
            printf("compare ok\n");

    }

    return 0;
}

int GetTickCount() {
    panic("todo");
}

// Retrieves the current value of the performance counter, which is a high resolution (<1us)
// time stamp that can be used for time-interval measurements.
bool QueryPerformanceCounter(int64_t *ticks) {
    (void) ticks;
    panic("todo");
}

bool QueryPerformanceFrequency(int64_t *freq) {
    (void) freq;
    panic("todo");
}

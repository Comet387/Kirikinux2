#include "Platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/resource.h>

void TVPGetMemoryInfo(TVPMemoryInfo &m)
{
    m = TVPMemoryInfo{};
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        const unsigned long long unit = info.mem_unit;
        m.MemTotal = (unsigned long)(info.totalram * unit / 1024);
        m.MemFree = (unsigned long)(info.freeram * unit / 1024);
        m.SwapTotal = (unsigned long)(info.totalswap * unit / 1024);
        m.SwapFree = (unsigned long)(info.freeswap * unit / 1024);
        m.VirtualTotal = m.MemTotal + m.SwapTotal;
        m.VirtualUsed = m.VirtualTotal - m.MemFree - m.SwapFree;
    }
    /* to read /proc/meminfo */
    FILE* meminfo;
    char buffer[100] = {0};
    char* end;
    int found = 0;

    /* Try to read /proc/meminfo, bail out if fails */
	meminfo = fopen("/proc/meminfo", "r");
    if (!meminfo) return;

    static const char
        pszMemFree[] = "MemFree:",
        pszMemTotal[] = "MemTotal:",
        pszSwapTotal[] = "SwapTotal:",
        pszSwapFree[] = "SwapFree:",
        pszVmallocTotal[] = "VmallocTotal:",
        pszVmallocUsed[] = "VmallocUsed:";

    /* Read each line untill we got all we ned */
    while( fgets( buffer, sizeof( buffer ), meminfo ) )
    {
        if( strstr( buffer, pszMemFree ) == buffer )
        {
            m.MemFree = strtol( buffer + sizeof(pszMemFree), &end, 10 );
            found++;
        }
        else if( strstr( buffer, pszMemTotal ) == buffer )
        {
            m.MemTotal = strtol( buffer + sizeof(pszMemTotal), &end, 10 );
            found++;
        }
        else if( strstr( buffer, pszSwapTotal ) == buffer )
        {
            m.SwapTotal = strtol( buffer + sizeof(pszSwapTotal), &end, 10 );
            found++;
        }
        else if( strstr( buffer, pszSwapFree ) == buffer )
        {
            m.SwapFree = strtol( buffer + sizeof(pszSwapFree), &end, 10 );
            found++;
        }
        else if( strstr( buffer, pszVmallocTotal ) == buffer )
        {
            m.VirtualTotal = strtol( buffer + sizeof(pszVmallocTotal), &end, 10 );
            found++;
        }
        else if( strstr( buffer, pszVmallocUsed ) == buffer )
        {
            m.VirtualUsed = strtol( buffer + sizeof(pszVmallocUsed), &end, 10 );
            found++;
        }
    }
    fclose(meminfo);
}

#include <sched.h>
void TVPRelinquishCPU(){
	sched_yield();
}

void TVP_utime(const char *name, time_t modtime) {
	timeval mt[2];
	mt[0].tv_sec = modtime;
	mt[0].tv_usec = 0;
	mt[1].tv_sec = modtime;
	mt[1].tv_usec = 0;
	utimes(name, mt);
}

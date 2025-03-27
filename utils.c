#include <stdio.h>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
#include <unistd.h>
#endif

int get_cpus_count()
{
#if defined(_WIN32) || defined(_WIN64)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_ONLN)
    return (int)sysconf(_SC_NPROCESSORS_ONLN);
#else
    return 1;
#endif
}

int clamp(int value, int min, int max)
{
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

int min(int a, int b)
{
    if (a > b)
    {
        return b;
    }
    return a;
}

int max(int a, int b)
{
    if (a > b)
    {
        return a;
    }
    return b;
}


int reflect_index(int i,int n){
    if (i < 0)
        return  -i;
    else if (i >= n)
        return 2 * n - i - 2;
    else
        return i;
}

#ifndef MEMORY_USED_H
#define MEMORY_USED_H

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

namespace NP {

	static long get_peak_memory_usage() {
#ifdef _WIN32
		PROCESS_MEMORY_COUNTERS pmc;
		auto currentProcess = GetCurrentProcess();
		GetProcessMemoryInfo(currentProcess, &pmc, sizeof(pmc));
		long mem_used = pmc.PeakWorkingSetSize / 1024; //in kiB
		CloseHandle(currentProcess);
#else
		struct rusage u;
		long mem_used = 0;
		if (getrusage(RUSAGE_SELF, &u) == 0)
			mem_used = u.ru_maxrss;
#endif
		return mem_used;
	}
}

#endif

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cassert>
#ifdef WIN
#include <shlobj.h>
#include <shlwapi.h>
#include <windows.h>
#else
#include <unistd.h>
#include <ctime>
#include <sys/time.h>
#endif
#ifdef MACOSX
#include <mach-o/dyld.h>
#endif
#include "Platform.h"
#include "Misc.h"

#ifdef _3DS
#include <3ds.h>
#endif

#ifdef IOS
	#include <spawn.h>
	extern char **environ;
#endif

namespace Platform
{

char *ExecutableName(void)
{
#if defined (_3DS)
	char *name = NULL;
	int res = 0;
#elif defined(WIN)
	char *name = (char *)malloc(64);
	DWORD max = 64, res;
	while ((res = GetModuleFileName(NULL, name, max)) >= max)
	{
		max *= 2;
		char* realloced_name = (char *)realloc(name, max);
		assert(realloced_name != NULL);
		name = realloced_name;
		memset(name, 0, max);
	}
#elif defined MACOSX
	char *fn = (char*)malloc(64),*name = (char*)malloc(PATH_MAX);
	uint32_t max = 64, res;
	if (_NSGetExecutablePath(fn, &max) != 0)
	{
		char *realloced_fn = (char*)realloc(fn, max);
		assert(realloced_fn != NULL);
		fn = realloced_fn;
		_NSGetExecutablePath(fn, &max);
	}
	if (realpath(fn, name) == NULL)
	{
		free(fn);
		free(name);
		return NULL;
	}
	res = 1;
#else
	char fn[64], *name = (char *)malloc(64);
	size_t max = 64, res;
	sprintf(fn, "/proc/self/exe");
	memset(name, 0, max);
	while ((res = readlink(fn, name, max)) >= max-1)
	{
		max *= 2;
		char* realloced_name = (char *)realloc(name, max);
		assert(realloced_name != NULL);
		name = realloced_name;
		memset(name, 0, max);
	}
#endif
#ifndef MACOSX
#endif
	if (res <= 0)
	{
		free(name);
		return NULL;
	}
	return name;
}

void OpenURI(std::string uri)
{
#if defined(WIN)
	ShellExecute(0, "OPEN", uri.c_str(), NULL, NULL, 0);
#elif defined(MACOSX)
	char *cmd = (char*)malloc(7+uri.length());
	strcpy(cmd, "open ");
	strappend(cmd, (char*)uri.c_str());
	#ifdef IOS
        /*
		pid_t pid;
		char * argv[2]; argv[0] = cmd; argv[1] = NULL;
		posix_spawn(&pid, argv[0], NULL, NULL, argv, environ);
        */
	#else
		system(cmd);
	#endif
#elif defined(LIN)
	char *cmd = (char*)malloc(11+uri.length());
	strcpy(cmd, "xdg-open ");
	strappend(cmd, (char*)uri.c_str());
	system(cmd);
#else
	printf("Cannot open browser\n");
#endif
}

void Millisleep(long int t)
{
#ifdef WIN
	Sleep(t);
#elif defined (_3DS)
	svcSleepThread(t * 10000000);
#else
	struct timespec s;
	s.tv_sec = t / 1000;
	s.tv_nsec = (t % 1000) * 10000000;
	nanosleep(&s, NULL);
#endif
}

long unsigned int GetTime()
{
#ifdef WIN
	return GetTickCount();
#elif defined(MACOSX) || defined(_3DS)
	struct timeval s;
	gettimeofday(&s, NULL);
	return (unsigned int)(s.tv_sec * 1000 + s.tv_usec / 1000);
#else
	struct timespec s;
	clock_gettime(CLOCK_MONOTONIC, &s);
	return s.tv_sec * 1000 + s.tv_nsec / 1000000;
#endif
}

}

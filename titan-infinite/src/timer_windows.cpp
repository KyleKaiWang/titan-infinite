/*
 * Copyright (C) 2021 Kyle Wang
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
*/
#include "pch.h"
#include <time.h>
#include <stdint.h>
#include <windows.h>
#include <timeapi.h>

/************************************************************************/
// Time Related Functions
/************************************************************************/
uint32_t getSystemTime() { return (uint32_t)timeGetTime(); }

uint32_t getTimeSinceStart() { return (uint32_t)time(NULL); }

static int64_t highResTimerFrequency = 0;

void initTime()
{
	LARGE_INTEGER frequency;
	if (QueryPerformanceFrequency(&frequency))
	{
		highResTimerFrequency = frequency.QuadPart;
	}
	else
	{
		highResTimerFrequency = 1000LL;
	}
}

int64_t getTimerFrequency()
{
	if (highResTimerFrequency == 0)
		initTime();

	return highResTimerFrequency;
}

// Make sure timer frequency is initialized before anyone tries to use it
struct StaticTime
{
	StaticTime() { initTime(); }
} staticTimeInst;

int64_t getUSec()
{
	LARGE_INTEGER counter;
	QueryPerformanceCounter(&counter);
	return counter.QuadPart * (int64_t)1e6 / getTimerFrequency();
}

float counterToSecondsElapsed(int64_t start, int64_t end)
{
	return (float)(end - start) / (float)1e6;
}

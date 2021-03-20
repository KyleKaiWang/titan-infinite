/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */

#pragma once
#include "stdint.h"

// High res timer functions
int64_t getUSec();
int64_t getTimerFrequency();
float counterToSecondsElapsed(int64_t start, int64_t end);

// Time related functions
uint32_t getSystemTime();
uint32_t getTimeSinceStart();

/// Low res OS timer
class Timer
{
public:
	Timer();
	uint32_t GetMSec(bool reset);
	void Reset();

private:
	uint32_t m_startTime;
};
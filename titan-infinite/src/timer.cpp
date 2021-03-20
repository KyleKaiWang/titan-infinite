/*
 * Vulkan Renderer Program
 *
 * Copyright (C) 2020 Kyle Wang
 */
#include "pch.h"
#include "timer.h"

Timer::Timer() { Reset(); }

unsigned Timer::GetMSec(bool reset)
{
	unsigned currentTime = getSystemTime();
	unsigned elapsedTime = currentTime - m_startTime;
	if (reset)
		m_startTime = currentTime;

	return elapsedTime;
}

void Timer::Reset() { m_startTime = getSystemTime(); }
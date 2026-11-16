/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#ifdef PCVIEWER_INTEGRATION_TEST
#include "../tests/pcviewer_win32_fake.h"
#else
#include <windows.h>
#endif
#include "pcviewer_cli.h"
BOOL PcviewerResolveTarget(const PcviewerRequest* request, DWORD* drive_index);
BOOL PcviewerTargetSelected(const PcviewerRequest* request);
BOOL PcviewerProfileMatches(void);
void PcviewerLockControls(void);

/* 0: workers pending; -1: observation/close failed; 1: terminal, handles closed. */
int PcviewerPollWorkers(PcviewerSession* session, HANDLE* boot, HANDLE* format);

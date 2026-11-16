/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#ifdef PCVIEWER_COMPACT_UI_TEST
#include "../tests/pcviewer_compact_fake.h"
#else
#include <windows.h>
#endif
void PcviewerCompactEnable(BOOL enabled);
BOOL PcviewerCompactIsEnabled(void);
BOOL PcviewerCompactInit(HWND hidden_main, HINSTANCE instance,
	BOOL (*cancel_request)(void), void (*view_lost)(void));
BOOL PcviewerCompactPrepare(const char* target, const char* finishing_text);
BOOL PcviewerCompactShowBeforeFormat(void);
HWND PcviewerCompactGetOwner(HWND requested);
BOOL PcviewerCompactIsDialogMessage(MSG* message);
void PcviewerCompactSnapshotStatus(const char* message);
void PcviewerCompactSetElapsed(const char* elapsed);
void PcviewerCompactSetMarquee(BOOL marquee);
void PcviewerCompactSetPhaseFinishing(void);
void PcviewerCompactModalEnter(void);
void PcviewerCompactModalLeave(void);
void PcviewerCompactError(const char* message);
/* Only after all workers and result notifications have completed. */
void PcviewerCompactDestroyTerminal(void);

#define PCVIEWER_COMPACT_DISPATCH (WM_APP + 0x249)
BOOL PcviewerCompactCanFinish(void);
void PcviewerCompactDispatchPending(void);

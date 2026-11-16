/* SPDX-License-Identifier: GPL-3.0-or-later */
/* PCViewer's versioned invocation contract. No operating-system dependencies. */
#ifndef PCVIEWER_CLI_H
#define PCVIEWER_CLI_H

#include <stdint.h>
#include <wchar.h>

enum PcviewerParseResult {
	PCVIEWER_PARSE_NORMAL = 0,
	PCVIEWER_PARSE_VALID = 1,
	PCVIEWER_PARSE_INVALID = 64
};

enum PcviewerResult {
	PCVIEWER_SUCCESS = 0,
	PCVIEWER_FAILURE = 10,
	PCVIEWER_CANCELLED = 20,
	PCVIEWER_REJECTED = 64
};

typedef struct {
	char volume_guid[37];
	uint32_t expected_disk_number;
} PcviewerRequest;

typedef enum {
	PCVIEWER_PARSED,
	PCVIEWER_READY,
	PCVIEWER_CHECKING,
	PCVIEWER_CONFIRMING,
	PCVIEWER_FORMATTING,
	PCVIEWER_FINISHING,
	PCVIEWER_TERMINAL
} PcviewerPhase;

typedef struct {
	PcviewerPhase phase;
	int result;
	int format_started;
	int result_latched;
	int terminal_check_failed;
} PcviewerSession;

/* argv[0] is the executable. NORMAL leaves request zeroed, not partly parsed. */
int PcviewerParse(int argc, const wchar_t* const* argv, PcviewerRequest* request);
void PcviewerSessionInit(PcviewerSession* session);
int PcviewerMarkReady(PcviewerSession* session);
int PcviewerBeginChecking(PcviewerSession* session);
int PcviewerBeginConfirming(PcviewerSession* session);
/* Call only after successful creation of the actual formatting worker. */
int PcviewerBeginFormatting(PcviewerSession* session);
/* First completion wins. 0 requires the actual formatting phase. */
int PcviewerLatchResult(PcviewerSession* session, int result);
/* A failed/inconsistent worker termination observation permanently prevents 0. */
void PcviewerTerminalCheckFailed(PcviewerSession* session);
/* Caller must verify ALL retained workers signaled and close their handles first. */
int PcviewerFinish(PcviewerSession* session, int all_workers_finished);

#endif

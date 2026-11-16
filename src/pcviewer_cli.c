/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pcviewer_cli.h"
#include <string.h>

static int starts_with(const wchar_t* value, const wchar_t* prefix)
{
	return wcsncmp(value, prefix, wcslen(prefix)) == 0;
}

static int reserved_value_option(const wchar_t* value, const wchar_t* name)
{
	size_t length = wcslen(name);
	return wcsncmp(value, name, length) == 0 &&
		(value[length] == L'\0' || value[length] == L'=');
}

static int parse_guid(const wchar_t* value, char* output)
{
	size_t i;
	wchar_t c;
	if (wcslen(value) != 36)
		return 0;
	for (i = 0; i < 36; i++) {
		c = value[i];
		if (i == 8 || i == 13 || i == 18 || i == 23) {
			if (c != L'-')
				return 0;
		} else {
			if (c >= L'A' && c <= L'F')
				c += L'a' - L'A';
			if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f')))
				return 0;
		}
		output[i] = (char)c;
	}
	output[36] = '\0';
	return 1;
}

static int parse_disk(const wchar_t* value, uint32_t* output)
{
	uint32_t number = 0, digit;
	if (*value == L'\0')
		return 0;
	for (; *value != L'\0'; value++) {
		if (*value < L'0' || *value > L'9')
			return 0;
		digit = (uint32_t)(*value - L'0');
		if (number > (UINT32_MAX - digit) / 10)
			return 0;
		number = number * 10 + digit;
	}
	*output = number;
	return 1;
}

int PcviewerParse(int argc, const wchar_t* const* argv, PcviewerRequest* request)
{
	PcviewerRequest candidate = { { 0 }, 0 };
	int i, reserved = 0, mode = 0, guid = 0, disk = 0;
	if (request == NULL)
		return PCVIEWER_PARSE_INVALID;
	memset(request, 0, sizeof(*request));
	if (argc < 1 || argv == NULL || argv[0] == NULL)
		return PCVIEWER_PARSE_INVALID;
	for (i = 1; i < argc; i++) {
		if (argv[i] == NULL)
			return PCVIEWER_PARSE_INVALID;
		if (starts_with(argv[i], L"--pcviewer-") ||
			reserved_value_option(argv[i], L"--target-volume-guid") ||
			reserved_value_option(argv[i], L"--expected-disk-number"))
			reserved = 1;
	}
	if (!reserved)
		return PCVIEWER_PARSE_NORMAL;
	for (i = 1; i < argc; i++) {
		if (wcscmp(argv[i], L"--pcviewer-initialize-v1") == 0) {
			if (mode++)
				return PCVIEWER_PARSE_INVALID;
		} else if (wcscmp(argv[i], L"--target-volume-guid") == 0) {
			if (guid++ || ++i == argc || !parse_guid(argv[i], candidate.volume_guid))
				return PCVIEWER_PARSE_INVALID;
		} else if (wcscmp(argv[i], L"--expected-disk-number") == 0) {
			if (disk++ || ++i == argc || !parse_disk(argv[i], &candidate.expected_disk_number))
				return PCVIEWER_PARSE_INVALID;
		} else {
			return PCVIEWER_PARSE_INVALID;
		}
	}
	if (mode != 1 || guid != 1 || disk != 1)
		return PCVIEWER_PARSE_INVALID;
	*request = candidate;
	return PCVIEWER_PARSE_VALID;
}

void PcviewerSessionInit(PcviewerSession* session)
{
	memset(session, 0, sizeof(*session));
	session->phase = PCVIEWER_PARSED;
	session->result = PCVIEWER_FAILURE;
}

static int advance(PcviewerSession* session, PcviewerPhase from, PcviewerPhase to)
{
	if (session->phase != from || session->result_latched)
		return 0;
	session->phase = to;
	return 1;
}

int PcviewerMarkReady(PcviewerSession* session)
{
	return advance(session, PCVIEWER_PARSED, PCVIEWER_READY);
}

int PcviewerBeginChecking(PcviewerSession* session)
{
	return advance(session, PCVIEWER_READY, PCVIEWER_CHECKING);
}

int PcviewerBeginConfirming(PcviewerSession* session)
{
	return advance(session, PCVIEWER_CHECKING, PCVIEWER_CONFIRMING);
}

int PcviewerBeginFormatting(PcviewerSession* session)
{
	if (!advance(session, PCVIEWER_CONFIRMING, PCVIEWER_FORMATTING))
		return 0;
	session->format_started = 1;
	return 1;
}

int PcviewerLatchResult(PcviewerSession* session, int result)
{
	if (session->result_latched || session->phase == PCVIEWER_TERMINAL)
		return 0;
	if (result != PCVIEWER_SUCCESS && result != PCVIEWER_FAILURE &&
		result != PCVIEWER_CANCELLED && result != PCVIEWER_REJECTED)
		result = PCVIEWER_FAILURE;
	if (result == PCVIEWER_SUCCESS && (!session->format_started ||
		session->phase != PCVIEWER_FORMATTING || session->terminal_check_failed))
		result = PCVIEWER_FAILURE;
	if (result == PCVIEWER_REJECTED && session->format_started)
		result = PCVIEWER_FAILURE;
	session->result = result;
	session->result_latched = 1;
	session->phase = PCVIEWER_FINISHING;
	return 1;
}

void PcviewerTerminalCheckFailed(PcviewerSession* session)
{
	if (session->phase == PCVIEWER_TERMINAL)
		return;
	session->terminal_check_failed = 1;
	if (session->result == PCVIEWER_SUCCESS)
		session->result = PCVIEWER_FAILURE;
}

int PcviewerFinish(PcviewerSession* session, int all_workers_finished)
{
	if (session->phase != PCVIEWER_FINISHING || !session->result_latched || !all_workers_finished)
		return 0;
	session->phase = PCVIEWER_TERMINAL;
	return 1;
}

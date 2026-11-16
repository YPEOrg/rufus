/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pcviewer_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned assertions;
#define CHECK(x) do { assertions++; if (!(x)) { \
	fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
#define GUID L"30C5C872-A26C-11F0-8BDC-48684A7A1E02"
#define MODE L"--pcviewer-initialize-v1"
#define TARGET L"--target-volume-guid"
#define DISK L"--expected-disk-number"

static void expect(int argc, const wchar_t* const* argv, int expected)
{
	PcviewerRequest r;
	memset(&r, 0xff, sizeof(r));
	CHECK(PcviewerParse(argc, argv, &r) == expected);
	if (expected != PCVIEWER_PARSE_VALID) {
		CHECK(r.volume_guid[0] == '\0');
		CHECK(r.expected_disk_number == 0);
	}
}

static void parser_tests(void)
{
	PcviewerRequest r;
	const wchar_t* args[] = { L"rufus.exe", MODE, TARGET, GUID, DISK, L"6" };
	const wchar_t* bad_disks[] = { L"", L"-1", L"+1", L" 1", L"1 ", L"0x1", L"4294967296",
		L"99999999999999999999999999999999999999", L"\xff11", L"1\n", L"1.0", L"1a" };
	const wchar_t* bad_guids[] = { L"", L"{30c5c872-a26c-11f0-8bdc-48684a7a1e02}",
		L"30c5c872a26c11f08bdc48684a7a1e02", L"30g5c872-a26c-11f0-8bdc-48684a7a1e02",
		L"30c5c872_a26c-11f0-8bdc-48684a7a1e02", L"30c5c872-a26c-11f0-8bdc-48684a7a1e020",
		L"30c5c872-a26c-11f0-8bdc-48684a7a1e0", L"\uFF130c5c872-a26c-11f0-8bdc-48684a7a1e02" };
	const wchar_t* extras[] = { MODE, TARGET, DISK, L"--unknown", L"extra", L"--help", L"--gui",
		L"--iso", L"--filesystem", L"--", L"" };
	size_t i;
	CHECK(PcviewerParse(6, args, &r) == PCVIEWER_PARSE_VALID);
	CHECK(strcmp(r.volume_guid, "30c5c872-a26c-11f0-8bdc-48684a7a1e02") == 0);
	CHECK(r.expected_disk_number == 6);
	args[5] = L"4294967295";
	CHECK(PcviewerParse(6, args, &r) == PCVIEWER_PARSE_VALID);
	CHECK(r.expected_disk_number == UINT32_MAX);
	args[5] = L"00000000000000000000000000000000000000";
	CHECK(PcviewerParse(6, args, &r) == PCVIEWER_PARSE_VALID);
	CHECK(r.expected_disk_number == 0);
	args[5] = L"0006";
	CHECK(PcviewerParse(6, args, &r) == PCVIEWER_PARSE_VALID);
	CHECK(r.expected_disk_number == 6);
	for (i = 0; i < sizeof(bad_disks) / sizeof(bad_disks[0]); i++) {
		args[5] = bad_disks[i];
		expect(6, args, PCVIEWER_PARSE_INVALID);
	}
	args[5] = L"6";
	for (i = 0; i < sizeof(bad_guids) / sizeof(bad_guids[0]); i++) {
		args[3] = bad_guids[i];
		expect(6, args, PCVIEWER_PARSE_INVALID);
	}
	args[3] = GUID;
	for (i = 0; i < sizeof(extras) / sizeof(extras[0]); i++) {
		const wchar_t* extra[] = { L"rufus", MODE, TARGET, GUID, DISK, L"6", extras[i] };
		expect(7, extra, PCVIEWER_PARSE_INVALID);
	}
	/* All six permutations of the three complete options. */
	{
		const wchar_t* permutations[][6] = {
			{ L"rufus", MODE, TARGET, GUID, DISK, L"6" },
			{ L"rufus", MODE, DISK, L"6", TARGET, GUID },
			{ L"rufus", TARGET, GUID, MODE, DISK, L"6" },
			{ L"rufus", TARGET, GUID, DISK, L"6", MODE },
			{ L"rufus", DISK, L"6", MODE, TARGET, GUID },
			{ L"rufus", DISK, L"6", TARGET, GUID, MODE }
		};
		for (i = 0; i < 6; i++)
			expect(6, permutations[i], PCVIEWER_PARSE_VALID);
	}
	{
		const wchar_t* normal[] = { L"rufus", L"--help", L"--iso=test.iso", L"--target-volume-guid-other" };
		const wchar_t* reserved[] = { L"--pcviewer-", L"--pcviewer-initialize-v2", L"--pcviewer-initialize-v1=x",
			L"--target-volume-guid=x", L"--expected-disk-number=6", TARGET, DISK, MODE };
		expect(1, normal, PCVIEWER_PARSE_NORMAL);
		expect(4, normal, PCVIEWER_PARSE_NORMAL);
		for (i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++) {
			const wchar_t* single[] = { L"rufus", reserved[i] };
			expect(2, single, PCVIEWER_PARSE_INVALID);
		}
	}
	/* Remove each complete required option, including the mode marker. */
	{
		const wchar_t* missing_mode[] = { L"rufus", TARGET, GUID, DISK, L"6" };
		const wchar_t* missing_guid[] = { L"rufus", MODE, DISK, L"6" };
		const wchar_t* missing_disk[] = { L"rufus", MODE, TARGET, GUID };
		const wchar_t* duplicate_guid[] = { L"rufus", MODE, TARGET, GUID, DISK, L"6", TARGET, GUID };
		const wchar_t* duplicate_disk[] = { L"rufus", MODE, TARGET, GUID, DISK, L"6", DISK, L"6" };
		expect(5, missing_mode, PCVIEWER_PARSE_INVALID);
		expect(4, missing_guid, PCVIEWER_PARSE_INVALID);
		expect(4, missing_disk, PCVIEWER_PARSE_INVALID);
		expect(8, duplicate_guid, PCVIEWER_PARSE_INVALID);
		expect(8, duplicate_disk, PCVIEWER_PARSE_INVALID);
	}
	expect(5, args, PCVIEWER_PARSE_INVALID);
	expect(0, args, PCVIEWER_PARSE_INVALID);
	expect(1, NULL, PCVIEWER_PARSE_INVALID);
	args[3] = NULL;
	expect(6, args, PCVIEWER_PARSE_INVALID);
	CHECK(PcviewerParse(6, args, NULL) == PCVIEWER_PARSE_INVALID);
}

static void to_formatting(PcviewerSession* s)
{
	PcviewerSessionInit(s);
	CHECK(s->result == PCVIEWER_FAILURE);
	CHECK(!PcviewerBeginChecking(s));
	CHECK(PcviewerMarkReady(s));
	CHECK(!PcviewerMarkReady(s));
	CHECK(PcviewerBeginChecking(s));
	CHECK(!PcviewerBeginChecking(s));
	CHECK(!PcviewerBeginFormatting(s));
	CHECK(PcviewerBeginConfirming(s));
	CHECK(!PcviewerBeginConfirming(s));
	CHECK(PcviewerBeginFormatting(s));
	CHECK(!PcviewerBeginFormatting(s));
	CHECK(s->format_started);
}

static void state_tests(void)
{
	PcviewerSession s;
	int codes[] = { PCVIEWER_FAILURE, PCVIEWER_CANCELLED, PCVIEWER_REJECTED, 123 };
	size_t i;
	PcviewerSessionInit(&s);
	CHECK(!PcviewerFinish(&s, 1));
	CHECK(PcviewerLatchResult(&s, PCVIEWER_SUCCESS));
	CHECK(s.result == PCVIEWER_FAILURE); /* No actual format worker. */
	CHECK(!PcviewerFinish(&s, 0));
	CHECK(PcviewerFinish(&s, 1));
	CHECK(!PcviewerFinish(&s, 1));
	for (i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
		PcviewerSessionInit(&s);
		CHECK(PcviewerLatchResult(&s, codes[i]));
		CHECK(s.result == (codes[i] == 123 ? PCVIEWER_FAILURE : codes[i]));
		CHECK(!PcviewerLatchResult(&s, PCVIEWER_SUCCESS));
		CHECK(!PcviewerMarkReady(&s));
		CHECK(PcviewerFinish(&s, 1));
	}
	to_formatting(&s);
	CHECK(PcviewerLatchResult(&s, PCVIEWER_SUCCESS));
	CHECK(s.result == PCVIEWER_SUCCESS);
	CHECK(!PcviewerFinish(&s, 0));
	CHECK(s.phase == PCVIEWER_FINISHING);
	CHECK(!PcviewerLatchResult(&s, PCVIEWER_CANCELLED)); /* Late close cannot rewrite completion. */
	CHECK(!PcviewerBeginChecking(&s));
	CHECK(PcviewerFinish(&s, 1));
	CHECK(s.phase == PCVIEWER_TERMINAL);
	PcviewerTerminalCheckFailed(&s);
	CHECK(s.result == PCVIEWER_SUCCESS); /* Terminal is immutable. */
	to_formatting(&s);
	CHECK(PcviewerLatchResult(&s, PCVIEWER_SUCCESS));
	PcviewerTerminalCheckFailed(&s);
	CHECK(s.result == PCVIEWER_FAILURE);
	CHECK(!PcviewerFinish(&s, 0));
	CHECK(PcviewerFinish(&s, 1)); /* Recovered worker observation, still failure. */
	to_formatting(&s);
	PcviewerTerminalCheckFailed(&s);
	CHECK(PcviewerLatchResult(&s, PCVIEWER_SUCCESS));
	CHECK(s.result == PCVIEWER_FAILURE);
	to_formatting(&s);
	CHECK(PcviewerLatchResult(&s, PCVIEWER_REJECTED));
	CHECK(s.result == PCVIEWER_FAILURE); /* 64 cannot describe an already-started format. */
	to_formatting(&s);
	CHECK(PcviewerLatchResult(&s, PCVIEWER_CANCELLED));
	CHECK(!PcviewerLatchResult(&s, PCVIEWER_SUCCESS));
	CHECK(s.result == PCVIEWER_CANCELLED);
	CHECK(!PcviewerFinish(&s, 0));
	CHECK(PcviewerFinish(&s, 1));
}

int main(void)
{
	parser_tests();
	state_tests();
	printf("PCVIEWER_CLI_TEST_PASS assertions=%u; pure parser/state; no device calls\n", assertions);
	return 0;
}

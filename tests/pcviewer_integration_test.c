/* SPDX-License-Identifier: GPL-3.0-or-later */
/* All Windows entry points below are fakes. No process, device or UI calls. */
#include "pcviewer_integration.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned assertions;
#define CHECK(x) do { assertions++; if (!(x)) { \
	fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
typedef struct { int id, selection, enabled; intptr_t data; } Control;
typedef struct { DWORD status; int closes, fail_close; } Worker;
static Control controls[12];
HWND hDeviceList = &controls[0], hBootType = &controls[1], hPartitionScheme = &controls[2];
HWND hFileSystem = &controls[3], hClusterSize = &controls[4], hMainDialog = &controls[11];
BOOL force_large_fat32, zero_drive, write_as_image, write_as_esp, save_image;
static Worker volume;
static int open_failure, disk_number, opens, disk_queries;
static char opened_path[64];
static unsigned quick, extended, badblocks;

HANDLE CreateFileA(const char* path, DWORD access, DWORD share, void* security,
	DWORD creation, DWORD flags, HANDLE template_handle)
{
	CHECK(access == 0 && share == (FILE_SHARE_READ | FILE_SHARE_WRITE));
	CHECK(security == NULL && creation == OPEN_EXISTING && flags == 0 && template_handle == NULL);
	opens++;
	snprintf(opened_path, sizeof(opened_path), "%s", path);
	return open_failure ? INVALID_HANDLE_VALUE : &volume;
}
BOOL CloseHandle(HANDLE handle)
{
	Worker* worker = handle;
	CHECK(worker != NULL && handle != INVALID_HANDLE_VALUE);
	if (worker->fail_close)
		return FALSE;
	worker->closes++;
	CHECK(worker->closes == 1);
	return TRUE;
}
int GetDriveNumber(HANDLE handle, char* path)
{
	CHECK(handle == &volume && strcmp(path, opened_path) == 0);
	disk_queries++;
	return disk_number;
}
int ComboBox_GetCurSel(HWND control) { return ((Control*)control)->selection; }
intptr_t ComboBox_GetCurItemData(HWND control) { return ((Control*)control)->data; }
unsigned IsDlgButtonChecked(HWND window, int id)
{
	CHECK(window == hMainDialog);
	if (id == IDC_QUICK_FORMAT) return quick;
	if (id == IDC_EXTENDED_LABEL) return extended;
	CHECK(id == IDC_BAD_BLOCKS);
	return badblocks;
}
HWND GetWindow(HWND window, unsigned command)
{
	if (command == GW_CHILD) {
		CHECK(window == hMainDialog);
		return &controls[0];
	}
	CHECK(command == GW_HWNDNEXT);
	return window == &controls[10] ? NULL : (Control*)window + 1;
}
int GetDlgCtrlID(HWND control) { return ((Control*)control)->id; }
BOOL EnableWindow(HWND control, BOOL enable)
{
	((Control*)control)->enabled = enable;
	return TRUE;
}
DWORD WaitForSingleObject(HANDLE handle, DWORD timeout)
{
	Worker* worker = handle;
	CHECK(timeout == 0 && worker != NULL && worker->closes == 0);
	return worker->status;
}

static void reset(void)
{
	int i;
	memset(controls, 0, sizeof(controls));
	memset(&volume, 0, sizeof(volume));
	for (i = 0; i < 11; i++) {
		controls[i].id = 200 + i;
		controls[i].enabled = TRUE;
	}
	controls[5].id = IDCANCEL;
	controls[6].id = IDC_LOG;
	controls[7].id = IDC_STATUS;
	controls[8].id = IDC_PROGRESS;
	controls[9].id = IDC_MULTI_TOOLBAR;
	controls[0].data = DRIVE_INDEX_MIN + 6;
	controls[1].data = BT_NON_BOOTABLE;
	controls[2].data = PARTITION_STYLE_MBR;
	controls[3].data = FS_FAT32;
	controls[4].data = 32768;
	open_failure = 0; disk_number = 6; opens = disk_queries = 0;
	force_large_fat32 = TRUE;
	zero_drive = write_as_image = write_as_esp = save_image = FALSE;
	quick = BST_CHECKED; extended = badblocks = BST_UNCHECKED;
}

static void target_profile_tests(void)
{
	PcviewerRequest r = { "30c5c872-a26c-11f0-8bdc-48684a7a1e02", 6 };
	DWORD index = 999;
	int i;
	BOOL* flags[] = { &zero_drive, &write_as_image, &write_as_esp, &save_image };
	reset();
	CHECK(PcviewerResolveTarget(&r, &index));
	CHECK(index == DRIVE_INDEX_MIN + 6 && volume.closes == 1);
	CHECK(strcmp(opened_path, "\\\\?\\Volume{30c5c872-a26c-11f0-8bdc-48684a7a1e02}") == 0);
	reset(); open_failure = 1;
	CHECK(!PcviewerResolveTarget(&r, &index));
	CHECK(disk_queries == 0 && volume.closes == 0);
	reset(); disk_number = -1;
	CHECK(!PcviewerResolveTarget(&r, &index));
	CHECK(volume.closes == 1);
	reset(); disk_number = 7;
	CHECK(!PcviewerResolveTarget(&r, &index));
	reset(); CHECK(PcviewerTargetSelected(&r));
	reset(); controls[0].data++;
	CHECK(!PcviewerTargetSelected(&r));
	reset(); controls[0].selection = CB_ERR;
	CHECK(!PcviewerTargetSelected(&r));
	reset(); CHECK(PcviewerProfileMatches());
	for (i = 1; i <= 4; i++) {
		reset(); controls[i].data++;
		CHECK(!PcviewerProfileMatches());
	}
	reset(); force_large_fat32 = FALSE; CHECK(!PcviewerProfileMatches());
	reset(); quick = BST_UNCHECKED; CHECK(!PcviewerProfileMatches());
	reset(); extended = BST_CHECKED; CHECK(!PcviewerProfileMatches());
	reset(); badblocks = BST_CHECKED; CHECK(!PcviewerProfileMatches());
	for (i = 0; i < 4; i++) {
		reset(); *flags[i] = TRUE;
		CHECK(!PcviewerProfileMatches());
	}
	reset(); PcviewerLockControls();
	for (i = 0; i <= 10; i++)
		CHECK(controls[i].enabled == (i >= 5 && i <= 9));
}

static void completed(PcviewerSession* s, int result)
{
	PcviewerSessionInit(s);
	CHECK(PcviewerMarkReady(s));
	CHECK(PcviewerBeginChecking(s));
	CHECK(PcviewerBeginConfirming(s));
	CHECK(PcviewerBeginFormatting(s));
	CHECK(PcviewerLatchResult(s, result));
}
static void worker_tests(void)
{
	PcviewerSession s;
	Worker b = { WAIT_TIMEOUT, 0, 0 }, f = { WAIT_TIMEOUT, 0, 0 };
	HANDLE boot = &b, format = &f;
	completed(&s, PCVIEWER_SUCCESS);
	CHECK(PcviewerPollWorkers(&s, &boot, &format) == 0);
	CHECK(s.phase == PCVIEWER_FINISHING && b.closes == 0 && f.closes == 0);
	b.status = WAIT_OBJECT_0;
	CHECK(PcviewerPollWorkers(&s, &boot, &format) == 0);
	CHECK(b.closes == 0 && f.closes == 0);
	f.status = WAIT_FAILED;
	CHECK(PcviewerPollWorkers(&s, &boot, &format) == -1);
	CHECK(s.result == PCVIEWER_FAILURE && b.closes == 0 && f.closes == 0);
	f.status = WAIT_OBJECT_0;
	CHECK(PcviewerPollWorkers(&s, &boot, &format) == 1);
	CHECK(s.phase == PCVIEWER_TERMINAL && s.result == PCVIEWER_FAILURE);
	CHECK(boot == NULL && format == NULL && b.closes == 1 && f.closes == 1);
	CHECK(PcviewerPollWorkers(&s, &boot, &format) == 1);
	CHECK(b.closes == 1 && f.closes == 1);
	b.closes = f.closes = 0; boot = &b; format = &f;
	completed(&s, PCVIEWER_SUCCESS);
	CHECK(PcviewerPollWorkers(&s, &boot, &format) == 1);
	CHECK(s.result == PCVIEWER_SUCCESS);
	b.closes = f.closes = 0; boot = &b; format = &f;
	completed(&s, PCVIEWER_CANCELLED);
	f.status = WAIT_TIMEOUT;
	CHECK(PcviewerPollWorkers(&s, &boot, &format) == 0);
	f.status = WAIT_OBJECT_0;
	CHECK(PcviewerPollWorkers(&s, &boot, &format) == 1);
	CHECK(s.result == PCVIEWER_CANCELLED);
	/* Close failure keeps the remaining live handle available for retry. */
	b.closes = f.closes = 0; boot = &b; format = &f; f.fail_close = 1;
	completed(&s, PCVIEWER_SUCCESS);
	CHECK(PcviewerPollWorkers(&s, &boot, &format) == -1);
	CHECK(boot == NULL && format == &f && b.closes == 1 && f.closes == 0);
	f.fail_close = 0;
	CHECK(PcviewerPollWorkers(&s, &boot, &format) == 1);
	CHECK(s.result == PCVIEWER_FAILURE && b.closes == 1 && f.closes == 1);
	/* No worker is valid for pre-start cancellation or rejection, not success. */
	PcviewerSessionInit(&s); boot = format = NULL;
	CHECK(PcviewerLatchResult(&s, PCVIEWER_REJECTED));
	CHECK(PcviewerPollWorkers(&s, &boot, &format) == 1);
	CHECK(s.result == PCVIEWER_REJECTED);
	completed(&s, PCVIEWER_SUCCESS);
	CHECK(PcviewerPollWorkers(&s, &boot, &format) == -1);
	CHECK(s.phase == PCVIEWER_FINISHING && s.result == PCVIEWER_FAILURE);
	completed(&s, PCVIEWER_SUCCESS); boot = format = &f;
	CHECK(PcviewerPollWorkers(&s, &boot, &format) == -1);
	CHECK(s.result == PCVIEWER_FAILURE);
}
int main(void)
{
	target_profile_tests();
	worker_tests();
	printf("PCVIEWER_INTEGRATION_TEST_PASS assertions=%u; fake Windows API only; no device calls\n", assertions);
	return 0;
}

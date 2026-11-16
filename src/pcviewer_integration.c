/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef PCVIEWER_INTEGRATION_TEST
#include "../tests/pcviewer_win32_fake.h"
#else
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include "rufus.h"
#include "drive.h"
#include "resource.h"
#endif
#include <stdio.h>
#include "pcviewer_integration.h"
extern BOOL force_large_fat32, zero_drive, write_as_image, write_as_esp, save_image;

BOOL PcviewerResolveTarget(const PcviewerRequest* request, DWORD* drive_index)
{
	char path[64];
	HANDLE volume;
	int disk;
	snprintf(path, sizeof(path), "\\\\?\\Volume{%s}", request->volume_guid);
	volume = CreateFileA(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, 0, NULL);
	if (volume == INVALID_HANDLE_VALUE)
		return FALSE;
	disk = GetDriveNumber(volume, path);
	CloseHandle(volume);
	if (disk < 0 || (uint32_t)disk != request->expected_disk_number)
		return FALSE;
	*drive_index = (DWORD)disk + DRIVE_INDEX_MIN;
	return TRUE;
}

BOOL PcviewerTargetSelected(const PcviewerRequest* request)
{
	DWORD index;
	return PcviewerResolveTarget(request, &index) &&
		ComboBox_GetCurSel(hDeviceList) != CB_ERR &&
		(DWORD)ComboBox_GetCurItemData(hDeviceList) == index;
}

BOOL PcviewerProfileMatches(void)
{
	return ComboBox_GetCurItemData(hBootType) == BT_NON_BOOTABLE &&
		ComboBox_GetCurItemData(hPartitionScheme) == PARTITION_STYLE_MBR &&
		ComboBox_GetCurItemData(hFileSystem) == FS_FAT32 &&
		ComboBox_GetCurItemData(hClusterSize) == 32768 && force_large_fat32 &&
		IsDlgButtonChecked(hMainDialog, IDC_QUICK_FORMAT) == BST_CHECKED &&
		IsDlgButtonChecked(hMainDialog, IDC_EXTENDED_LABEL) == BST_UNCHECKED &&
		IsDlgButtonChecked(hMainDialog, IDC_BAD_BLOCKS) == BST_UNCHECKED &&
		!zero_drive && !write_as_image && !write_as_esp && !save_image;
}

void PcviewerLockControls(void)
{
	HWND child;
	/* Keep the existing cancel/progress/confirmation machinery, not another UI. */
	for (child = GetWindow(hMainDialog, GW_CHILD); child != NULL;
		child = GetWindow(child, GW_HWNDNEXT)) {
		int id = GetDlgCtrlID(child);
		if (id != IDCANCEL && id != IDC_LOG && id != IDC_STATUS && id != IDC_PROGRESS &&
			id != IDC_MULTI_TOOLBAR)
			EnableWindow(child, FALSE);
	}
}

int PcviewerPollWorkers(PcviewerSession* session, HANDLE* boot, HANDLE* format)
{
	DWORD boot_status, format_status;
	if (session->phase == PCVIEWER_TERMINAL)
		return 1;
	if (session->phase != PCVIEWER_FINISHING)
		return 0;
	/* A missing actual worker or aliased handles cannot prove termination. */
	if ((session->format_started && *format == NULL) ||
		(*boot != NULL && *boot == *format)) {
		PcviewerTerminalCheckFailed(session);
		return -1;
	}
	boot_status = (*boot == NULL) ? WAIT_OBJECT_0 : WaitForSingleObject(*boot, 0);
	format_status = (*format == NULL) ? WAIT_OBJECT_0 : WaitForSingleObject(*format, 0);
	if ((boot_status != WAIT_OBJECT_0 && boot_status != WAIT_TIMEOUT) ||
		(format_status != WAIT_OBJECT_0 && format_status != WAIT_TIMEOUT)) {
		PcviewerTerminalCheckFailed(session);
		return -1;
	}
	if (boot_status == WAIT_TIMEOUT || format_status == WAIT_TIMEOUT)
		return 0;
	/* Both workers are signaled before either handle is released. */
	if (*boot != NULL) {
		if (!CloseHandle(*boot)) {
			PcviewerTerminalCheckFailed(session);
			return -1;
		}
		*boot = NULL;
	}
	if (*format != NULL) {
		if (!CloseHandle(*format)) {
			PcviewerTerminalCheckFailed(session);
			return -1;
		}
		*format = NULL;
	}
	return PcviewerFinish(session, 1) ? 1 : -1;
}

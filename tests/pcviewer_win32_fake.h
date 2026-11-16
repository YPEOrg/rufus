/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PCVIEWER_WIN32_FAKE_H
#define PCVIEWER_WIN32_FAKE_H
#include <stddef.h>
#include <stdint.h>
typedef int BOOL;
typedef uint32_t DWORD;
typedef void* HANDLE;
typedef void* HWND;
#define TRUE 1
#define FALSE 0
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define FILE_SHARE_READ 1
#define FILE_SHARE_WRITE 2
#define OPEN_EXISTING 3
#define CB_ERR (-1)
#define DRIVE_INDEX_MIN 0x80
#define BT_NON_BOOTABLE 0
#define PARTITION_STYLE_MBR 0
#define FS_FAT32 1
#define BST_CHECKED 1
#define BST_UNCHECKED 0
#define GW_CHILD 5
#define GW_HWNDNEXT 2
#define IDCANCEL 2
#define IDC_LOG 100
#define IDC_STATUS 101
#define IDC_PROGRESS 102
#define IDC_MULTI_TOOLBAR 103
#define IDC_QUICK_FORMAT 104
#define IDC_EXTENDED_LABEL 105
#define IDC_BAD_BLOCKS 106
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define WAIT_FAILED UINT32_MAX
extern HWND hDeviceList, hBootType, hPartitionScheme, hFileSystem, hClusterSize, hMainDialog;
HANDLE CreateFileA(const char*, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
BOOL CloseHandle(HANDLE);
int GetDriveNumber(HANDLE, char*);
int ComboBox_GetCurSel(HWND);
intptr_t ComboBox_GetCurItemData(HWND);
unsigned IsDlgButtonChecked(HWND, int);
HWND GetWindow(HWND, unsigned);
int GetDlgCtrlID(HWND);
BOOL EnableWindow(HWND, BOOL);
DWORD WaitForSingleObject(HANDLE, DWORD);
#endif

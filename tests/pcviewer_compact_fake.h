/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PCVIEWER_COMPACT_FAKE_H
#define PCVIEWER_COMPACT_FAKE_H
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>
typedef int BOOL;
typedef void* HWND;
typedef void* HINSTANCE;
typedef unsigned UINT;
typedef uintptr_t UINT_PTR;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef intptr_t LONG_PTR;
typedef intptr_t INT_PTR;
typedef unsigned long DWORD;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; } MSG;
typedef int SRWLOCK;
#define SRWLOCK_INIT 0
#define CALLBACK
#define TRUE 1
#define FALSE 0
#define MAX_PROGRESS 65535
#define LOWORD(n) ((unsigned)(n) & 0xffff)
#define MAKEINTRESOURCEW(n) ((const wchar_t*)(uintptr_t)(n))
#define IDCANCEL 2
#define IDOK 1
#define IDD_PCV_PROGRESS 600
#define IDC_PCV_INFO 1600
#define IDC_PCV_STATUS 1601
#define IDC_PCV_TIME 1602
#define IDC_PCV_TARGET 1603
#define IDC_PROGRESS 100
#define WM_INITDIALOG 0x110
#define WM_COMMAND 0x111
#define WM_TIMER 0x113
#define WM_APP 0x8000
#define WM_CLOSE 0x10
#define WM_DESTROY 2
#define PBM_GETPOS 1
#define PBM_GETSTATE 2
#define PBM_SETPOS 3
#define PBM_SETSTATE 4
#define PBM_SETMARQUEE 5
#define PBM_SETRANGE32 6
#define PBS_MARQUEE 8
#define GWL_STYLE (-16)
#define SW_SHOW 5
#define MB_OK 0
#define MB_ICONERROR 16
#define MSG_042 42
#define MSG_375 375
#define MSG_007 7
extern HWND hProgress, hMainDialog;
void SetDarkModeForDlg(HWND);
void SetDarkModeForChild(HWND);
void CenterDialog(HWND, HWND);
BOOL PostMessage(HWND, UINT, WPARAM, LPARAM);
BOOL IsWindow(HWND);
BOOL IsWindowVisible(HWND);
BOOL IsWindowEnabled(HWND);
HWND GetDlgItem(HWND, int);
void AcquireSRWLockShared(SRWLOCK*);
void ReleaseSRWLockShared(SRWLOCK*);
void AcquireSRWLockExclusive(SRWLOCK*);
void ReleaseSRWLockExclusive(SRWLOCK*);
BOOL EnableWindow(HWND, BOOL);
LRESULT SendMessage(HWND, UINT, WPARAM, LPARAM);
LONG_PTR GetWindowLongPtr(HWND, int);
LONG_PTR SetWindowLongPtr(HWND, int, LONG_PTR);
int GetWindowTextU(HWND, char*, int);
BOOL SetWindowTextU(HWND, const char*);
HWND CreateDialogParamW(HINSTANCE, const wchar_t*, HWND, INT_PTR (*)(HWND,UINT,WPARAM,LPARAM), LPARAM);
UINT_PTR SetTimer(HWND, UINT_PTR, UINT, void*);
BOOL KillTimer(HWND, UINT_PTR);
BOOL DestroyWindow(HWND);
BOOL ShowWindow(HWND, int);
BOOL SetForegroundWindow(HWND);
HWND SetFocus(HWND);
BOOL IsDialogMessageW(HWND, MSG*);
const char* lmprintf(int);
int Notification(int, const char*, const char*, ...);
#endif

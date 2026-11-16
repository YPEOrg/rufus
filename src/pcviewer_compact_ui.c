/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pcviewer_compact_ui.h"
#include <string.h>
#ifndef PCVIEWER_COMPACT_UI_TEST
#include <commctrl.h>
#include "rufus.h"
#include "resource.h"
#include "localization.h"
#include "msapi_utf8.h"
#include "darkmode.h"
#endif

#define PCV_TEXT_CAPACITY 2049
#define PCV_VIEW_TIMER 0x5043

typedef struct {
	char status[PCV_TEXT_CAPACITY];
	char elapsed[PCV_TEXT_CAPACITY];
	BOOL marquee, hooks_enabled;
	HWND visible_owner;
	unsigned modal_depth;
	BOOL notification_inflight, has_pending_error;
	char pending_error[PCV_TEXT_CAPACITY];
} CompactSnapshot;

static SRWLOCK snapshot_lock = SRWLOCK_INIT;
static CompactSnapshot snapshot;
static HWND host, view;
static HINSTANCE module_instance;
static BOOL mode_enabled, prepared, visible, finishing, destroying, fault_reported;
static BOOL cancel_in_flight, cancel_accepted;
static BOOL (*request_cancel)(void);
static void (*report_view_loss)(void);
static char finish_text[PCV_TEXT_CAPACITY];
static char last_error[PCV_TEXT_CAPACITY];
static BOOL displayed_marquee;

/* Own the bytes and never leave a truncated UTF-8 sequence in the snapshot. */
static void copy_utf8(char* destination, const char* source)
{
	size_t used = 0, width, i;
	unsigned char c;
	if (source == NULL)
		source = "";
	while (*source != '\0' && used < PCV_TEXT_CAPACITY - 1) {
		c = (unsigned char)*source;
		width = (c < 0x80) ? 1 : ((c >= 0xc2 && c <= 0xdf) ? 2 :
			((c >= 0xe0 && c <= 0xef) ? 3 : ((c >= 0xf0 && c <= 0xf4) ? 4 : 0)));
		if (width == 0) {
			destination[used++] = '?';
			source++;
			continue;
		}
		for (i = 1; i < width; i++) {
			if (source[i] == '\0' || ((unsigned char)source[i] & 0xc0) != 0x80)
				break;
		}
		if (i != width) {
			destination[used++] = '?';
			source++;
			continue;
		}
		if (used + width >= PCV_TEXT_CAPACITY)
			break;
		memcpy(destination + used, source, width);
		used += width;
		source += width;
	}
	destination[used] = '\0';
}

void PcviewerCompactEnable(BOOL enabled) { mode_enabled = enabled; }
BOOL PcviewerCompactIsEnabled(void) { return mode_enabled; }

static BOOL required_controls(void)
{
	static const int ids[] = { IDC_PCV_INFO, IDC_PCV_STATUS, IDC_PCV_TIME,
		IDC_PCV_TARGET, IDC_PROGRESS, IDCANCEL };
	size_t i;
	if (!IsWindow(view))
		return FALSE;
	for (i = 0; i < sizeof(ids) / sizeof(ids[0]); i++)
		if (!IsWindow(GetDlgItem(view, ids[i])))
			return FALSE;
	return TRUE;
}

static void report_fault(void)
{
	if (fault_reported || destroying)
		return;
	fault_reported = TRUE;
	if (report_view_loss != NULL)
		report_view_loss();
}

static void cancel_once(void)
{
	unsigned modal_depth;
	AcquireSRWLockShared(&snapshot_lock);
	modal_depth = snapshot.modal_depth;
	ReleaseSRWLockShared(&snapshot_lock);
	if (!visible || finishing || cancel_accepted || cancel_in_flight || modal_depth != 0 ||
		!IsWindowEnabled(view) || request_cancel == NULL)
		return;
	cancel_in_flight = TRUE;
	cancel_accepted = request_cancel();
	cancel_in_flight = FALSE;
	if (cancel_accepted && IsWindow(view))
		EnableWindow(GetDlgItem(view, IDCANCEL), FALSE);
}

static void refresh_view(void)
{
	CompactSnapshot current;
	char info[PCV_TEXT_CAPACITY];
	HWND progress;
	LRESULT position, state;
	if (!prepared || destroying)
		return;
	if (!required_controls() || (visible && !IsWindowVisible(view))) {
		report_fault();
		return;
	}
	AcquireSRWLockShared(&snapshot_lock);
	current = snapshot;
	ReleaseSRWLockShared(&snapshot_lock);
	/* Every window operation happens outside the snapshot lock, on the UI thread. */
	progress = GetDlgItem(view, IDC_PROGRESS);
	position = SendMessage(hProgress, PBM_GETPOS, 0, 0);
	state = SendMessage(hProgress, PBM_GETSTATE, 0, 0);
	SendMessage(progress, PBM_SETPOS, (WPARAM)position, 0);
	SendMessage(progress, PBM_SETSTATE, (WPARAM)state, 0);
	if (displayed_marquee != current.marquee) {
		LONG_PTR style = GetWindowLongPtr(progress, GWL_STYLE);
		SetWindowLongPtr(progress, GWL_STYLE, current.marquee ? style | PBS_MARQUEE : style & ~PBS_MARQUEE);
		SendMessage(progress, PBM_SETMARQUEE, current.marquee, 30);
		displayed_marquee = current.marquee;
	}
	info[0] = '\0';
	GetWindowTextU(hProgress, info, sizeof(info));
	SetWindowTextU(GetDlgItem(view, IDC_PCV_INFO), info);
	SetWindowTextU(GetDlgItem(view, IDC_PCV_STATUS), finishing ? finish_text : current.status);
	SetWindowTextU(GetDlgItem(view, IDC_PCV_TIME), current.elapsed);
}

static INT_PTR CALLBACK CompactCallback(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	(void)window;
	(void)lparam;
	switch (message) {
	case WM_INITDIALOG:
		SetDarkModeForDlg(window);
		CenterDialog(window, NULL);
		SetDarkModeForChild(window);
		return TRUE;
	case WM_COMMAND:
		if (LOWORD(wparam) == IDCANCEL || LOWORD(wparam) == IDOK) {
			cancel_once();
			return TRUE;
		}
		break;
	case WM_CLOSE:
		cancel_once();
		return TRUE;
	case WM_TIMER:
		if (wparam == PCV_VIEW_TIMER) {
			refresh_view();
			PcviewerCompactDispatchPending();
			return TRUE;
		}
		break;
	case WM_DESTROY:
		if (prepared && !destroying)
			report_fault();
		return TRUE;
	}
	return FALSE;
}

BOOL PcviewerCompactInit(HWND hidden_main, HINSTANCE instance,
	BOOL (*cancel_request)(void), void (*view_lost)(void))
{
	if (!mode_enabled || view != NULL || !IsWindow(hidden_main))
		return FALSE;
	host = hidden_main;
	module_instance = instance;
	request_cancel = cancel_request;
	report_view_loss = view_lost;
	prepared = visible = finishing = destroying = fault_reported = FALSE;
	cancel_in_flight = cancel_accepted = displayed_marquee = FALSE;
	last_error[0] = '\0';
	AcquireSRWLockExclusive(&snapshot_lock);
	memset(&snapshot, 0, sizeof(snapshot));
	snapshot.hooks_enabled = TRUE;
	ReleaseSRWLockExclusive(&snapshot_lock);
	return TRUE;
}

BOOL PcviewerCompactPrepare(const char* target, const char* finishing_text)
{
	if (!mode_enabled || !IsWindow(host) || view != NULL)
		return FALSE;
	copy_utf8(finish_text, finishing_text);
	/* Not owned by the hidden main: otherwise Windows may hide/disable the visible view. */
	view = CreateDialogParamW(module_instance, MAKEINTRESOURCEW(IDD_PCV_PROGRESS), NULL, CompactCallback, 0);
	if (!required_controls())
		goto failed;
	SetWindowTextU(GetDlgItem(view, IDC_PCV_TARGET), target == NULL ? "" : target);
	SetWindowTextU(view, lmprintf(MSG_375));
	SetWindowTextU(GetDlgItem(view, IDCANCEL), lmprintf(MSG_007));
	SendMessage(GetDlgItem(view, IDC_PROGRESS), PBM_SETRANGE32, 0, MAX_PROGRESS);
	if (SetTimer(view, PCV_VIEW_TIMER, 100, NULL) == 0)
		goto failed;
	prepared = TRUE;
	return TRUE;
failed:
	destroying = TRUE;
	if (IsWindow(view))
		DestroyWindow(view);
	view = NULL;
	destroying = FALSE;
	return FALSE;
}

BOOL PcviewerCompactShowBeforeFormat(void)
{
	if (!prepared || finishing || !required_controls())
		return FALSE;
	ShowWindow(view, SW_SHOW);
	if (!IsWindowVisible(view) || !required_controls())
		return FALSE;
	visible = TRUE;
	AcquireSRWLockExclusive(&snapshot_lock);
	snapshot.visible_owner = view;
	ReleaseSRWLockExclusive(&snapshot_lock);
	refresh_view();
	SetForegroundWindow(view);
	SetFocus(GetDlgItem(view, IDCANCEL));
	return !fault_reported;
}

HWND PcviewerCompactGetOwner(HWND requested)
{
	HWND owner;
	if (!mode_enabled || (requested != host && requested != hMainDialog))
		return requested;
	AcquireSRWLockShared(&snapshot_lock);
	owner = snapshot.visible_owner;
	ReleaseSRWLockShared(&snapshot_lock);
	return IsWindow(owner) && IsWindowVisible(owner) ? owner : NULL;
}

BOOL PcviewerCompactIsDialogMessage(MSG* message)
{
	return mode_enabled && visible && IsWindow(view) && IsDialogMessageW(view, message);
}

void PcviewerCompactSnapshotStatus(const char* message)
{
	AcquireSRWLockExclusive(&snapshot_lock);
	if (snapshot.hooks_enabled)
		copy_utf8(snapshot.status, message);
	ReleaseSRWLockExclusive(&snapshot_lock);
}

void PcviewerCompactSetElapsed(const char* elapsed)
{
	AcquireSRWLockExclusive(&snapshot_lock);
	if (snapshot.hooks_enabled)
		copy_utf8(snapshot.elapsed, elapsed);
	ReleaseSRWLockExclusive(&snapshot_lock);
}

void PcviewerCompactSetMarquee(BOOL marquee)
{
	AcquireSRWLockExclusive(&snapshot_lock);
	if (snapshot.hooks_enabled)
		snapshot.marquee = marquee;
	ReleaseSRWLockExclusive(&snapshot_lock);
}

void PcviewerCompactSetPhaseFinishing(void)
{
	finishing = TRUE;
	if (IsWindow(view))
		EnableWindow(GetDlgItem(view, IDCANCEL), FALSE);
	refresh_view();
}

void PcviewerCompactModalEnter(void)
{
	AcquireSRWLockExclusive(&snapshot_lock);
	snapshot.modal_depth++;
	ReleaseSRWLockExclusive(&snapshot_lock);
}
static void post_pending_error(void)
{
	HWND destination = host != NULL ? host : hMainDialog;
	/* Failure to queue retains the pending error and therefore forbids finishing. */
	if (destination != NULL)
		PostMessage(destination, PCVIEWER_COMPACT_DISPATCH, 0, 0);
}

void PcviewerCompactModalLeave(void)
{
	BOOL dispatch;
	AcquireSRWLockExclusive(&snapshot_lock);
	if (snapshot.modal_depth != 0)
		snapshot.modal_depth--;
	dispatch = snapshot.modal_depth == 0 && snapshot.has_pending_error;
	ReleaseSRWLockExclusive(&snapshot_lock);
	/* NotificationEx has cleanup after MyDialogBox returns. Never nest here. */
	if (dispatch)
		post_pending_error();
}

BOOL PcviewerCompactCanFinish(void)
{
	BOOL allowed;
	AcquireSRWLockShared(&snapshot_lock);
	allowed = snapshot.modal_depth == 0 && !snapshot.notification_inflight &&
		!snapshot.has_pending_error;
	ReleaseSRWLockShared(&snapshot_lock);
	return allowed && !cancel_in_flight;
}

void PcviewerCompactError(const char* message)
{
	char owned[PCV_TEXT_CAPACITY];
	copy_utf8(owned, message);
	AcquireSRWLockExclusive(&snapshot_lock);
	if (strcmp(last_error, owned) == 0) {
		ReleaseSRWLockExclusive(&snapshot_lock);
		return;
	}
	copy_utf8(last_error, owned);
	copy_utf8(snapshot.pending_error, owned);
	snapshot.has_pending_error = TRUE;
	ReleaseSRWLockExclusive(&snapshot_lock);
	post_pending_error();
}

void PcviewerCompactDispatchPending(void)
{
	char owned[PCV_TEXT_CAPACITY];
	BOOL again;
	AcquireSRWLockExclusive(&snapshot_lock);
	if (!snapshot.has_pending_error || snapshot.modal_depth != 0 || snapshot.notification_inflight || cancel_in_flight) {
		ReleaseSRWLockExclusive(&snapshot_lock);
		return;
	}
	copy_utf8(owned, snapshot.pending_error);
	snapshot.has_pending_error = FALSE;
	snapshot.notification_inflight = TRUE;
	ReleaseSRWLockExclusive(&snapshot_lock);
	/* Main UI message dispatch only, after the outer notification has cleaned up. */
	Notification(MB_OK | MB_ICONERROR, lmprintf(MSG_042), "%s", owned);
	AcquireSRWLockExclusive(&snapshot_lock);
	snapshot.notification_inflight = FALSE;
	again = snapshot.has_pending_error;
	ReleaseSRWLockExclusive(&snapshot_lock);
	if (again)
		post_pending_error();
}

void PcviewerCompactDestroyTerminal(void)
{
	destroying = TRUE;
	if (IsWindow(view))
		KillTimer(view, PCV_VIEW_TIMER);
	AcquireSRWLockExclusive(&snapshot_lock);
	snapshot.hooks_enabled = FALSE;
	snapshot.visible_owner = NULL;
	ReleaseSRWLockExclusive(&snapshot_lock);
	if (IsWindow(view))
		DestroyWindow(view);
	view = NULL;
	visible = prepared = FALSE;
}

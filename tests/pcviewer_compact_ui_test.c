/* SPDX-License-Identifier: GPL-3.0-or-later */
#define PCVIEWER_COMPACT_UI_TEST
#include "../src/pcviewer_compact_ui.c"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
static unsigned assertions;
#define CHECK(x) do { assertions++; if (!(x)) { \
	fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
typedef struct {
	BOOL exists, shown, enabled;
	char text[4096];
	LONG_PTR style;
	LRESULT pos, state, range, marquee;
} Window;
static Window main_window, mini, progress_host, children[6], unrelated;
HWND hMainDialog = &main_window, hProgress = &progress_host;
static INT_PTR (*dialog_callback)(HWND,UINT,WPARAM,LPARAM);
static int fail_create, fail_timer, fail_control, fail_show;
static int timer_live, destroy_count, cancel_calls, accepted, reenter, faults, notices;
static int lock_depth, ui_calls, destruction_order, invalidation_order, order;
static char notice_text[4096];
static int posted, fail_post, nested_error;
static void ui(void) { CHECK(lock_depth == 0); ui_calls++; }
void SetDarkModeForDlg(HWND w) { ui(); CHECK(w == &mini); }
void SetDarkModeForChild(HWND w) { ui(); CHECK(w == &mini); }
void CenterDialog(HWND w, HWND parent) { ui(); CHECK(w == &mini && parent == NULL); }
BOOL PostMessage(HWND w,UINT message,WPARAM wp,LPARAM lp)
{
	ui(); CHECK(w == hMainDialog && message == PCVIEWER_COMPACT_DISPATCH && wp == 0 && lp == 0);
	if (fail_post) return FALSE;
	posted++;
	return TRUE;
}
BOOL IsWindow(HWND w) { ui(); return w != NULL && ((Window*)w)->exists; }
BOOL IsWindowVisible(HWND w) { ui(); return w != NULL && ((Window*)w)->exists && ((Window*)w)->shown; }
BOOL IsWindowEnabled(HWND w) { ui(); return w != NULL && ((Window*)w)->enabled; }
HWND GetDlgItem(HWND w, int id)
{
	int index;
	ui();
	if (w != &mini || !mini.exists) return NULL;
	index = id >= IDC_PCV_INFO && id <= IDC_PCV_TARGET ? id - IDC_PCV_INFO :
		(id == IDC_PROGRESS ? 4 : (id == IDCANCEL ? 5 : -1));
	if (id == fail_control || index < 0) return NULL;
	return &children[index];
}
void AcquireSRWLockShared(SRWLOCK* l) { (void)l; CHECK(lock_depth == 0); lock_depth++; }
void ReleaseSRWLockShared(SRWLOCK* l) { (void)l; CHECK(lock_depth == 1); lock_depth--; }
void AcquireSRWLockExclusive(SRWLOCK* l) { AcquireSRWLockShared(l); }
void ReleaseSRWLockExclusive(SRWLOCK* l)
{
	if (!snapshot.hooks_enabled && destroying) invalidation_order = ++order;
	ReleaseSRWLockShared(l);
}
BOOL EnableWindow(HWND w, BOOL enable) { ui(); if (!w) return FALSE; ((Window*)w)->enabled = enable; return TRUE; }
LRESULT SendMessage(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
	Window* x = w; ui(); CHECK(x != NULL && x->exists);
	switch(msg) {
	case PBM_GETPOS: return x->pos;
	case PBM_GETSTATE: return x->state;
	case PBM_SETPOS: x->pos = (LRESULT)wp; break;
	case PBM_SETSTATE: x->state = (LRESULT)wp; break;
	case PBM_SETMARQUEE: x->marquee = (LRESULT)wp; break;
	case PBM_SETRANGE32: x->range = lp; break;
	default: CHECK(0);
	}
	return 0;
}
LONG_PTR GetWindowLongPtr(HWND w,int n) { ui(); CHECK(n == GWL_STYLE); return ((Window*)w)->style; }
LONG_PTR SetWindowLongPtr(HWND w,int n,LONG_PTR style) { ui(); CHECK(n == GWL_STYLE); ((Window*)w)->style=style; return 0; }
int GetWindowTextU(HWND w,char* text,int capacity) { ui(); snprintf(text,(size_t)capacity,"%s",((Window*)w)->text); return (int)strlen(text); }
BOOL SetWindowTextU(HWND w,const char* text) { ui(); CHECK(w != NULL); snprintf(((Window*)w)->text,sizeof(((Window*)w)->text),"%s",text); return TRUE; }
HWND CreateDialogParamW(HINSTANCE instance,const wchar_t* resource,HWND owner,
	INT_PTR (*callback)(HWND,UINT,WPARAM,LPARAM),LPARAM param)
{
	int i; ui(); (void)instance; (void)param;
	CHECK((uintptr_t)resource == IDD_PCV_PROGRESS && owner == NULL);
	if (fail_create) return NULL;
	mini.exists = mini.enabled = TRUE;
	mini.shown = FALSE;
	for(i=0;i<6;i++) children[i].exists = children[i].enabled = TRUE;
	dialog_callback=callback;
	callback(&mini,WM_INITDIALOG,0,0);
	return &mini;
}
UINT_PTR SetTimer(HWND w,UINT_PTR id,UINT period,void* callback)
{
	ui(); CHECK(w == &mini && id == PCV_VIEW_TIMER && period == 100 && callback == NULL);
	if(fail_timer) return 0;
	timer_live=1; return id;
}
BOOL KillTimer(HWND w,UINT_PTR id) { ui(); CHECK(w == &mini && id == PCV_VIEW_TIMER); timer_live=0; ++order; return TRUE; }
BOOL DestroyWindow(HWND w)
{
	ui(); CHECK(w == &mini); destroy_count++;
	destruction_order=++order;
	mini.exists=mini.shown=FALSE;
	if(dialog_callback) dialog_callback(w,WM_DESTROY,0,0);
	return TRUE;
}
BOOL ShowWindow(HWND w,int cmd) { ui(); CHECK(w == &mini && cmd == SW_SHOW); if(!fail_show) mini.shown=TRUE; return TRUE; }
BOOL SetForegroundWindow(HWND w) { ui(); CHECK(w == &mini); return TRUE; }
HWND SetFocus(HWND w) { ui(); CHECK(w == &children[5]); return NULL; }
BOOL IsDialogMessageW(HWND w,MSG* message) { ui(); CHECK(w == &mini); dialog_callback(w,message->message,message->wParam,message->lParam); return TRUE; }
const char* lmprintf(int id) { return id == MSG_042 ? "Error" : (id == MSG_007 ? "Cancel" : "SD initialization"); }
int Notification(int type,const char* title,const char* format,...)
{
	va_list args; ui(); CHECK(type == (MB_OK|MB_ICONERROR) && strcmp(title,"Error")==0);
	CHECK(!PcviewerCompactCanFinish());
	if(nested_error) {
		nested_error=0;
		PcviewerCompactError("queued second error");
		PcviewerCompactDispatchPending();
		CHECK(notices == 0);
	}
	notices++; va_start(args,format); vsnprintf(notice_text,sizeof(notice_text),format,args); va_end(args); return 1;
}
static BOOL cancel_handler(void)
{
	cancel_calls++;
	CHECK(!PcviewerCompactCanFinish());
	if(reenter) dialog_callback(&mini,WM_CLOSE,0,0);
	return accepted;
}
static void fault_handler(void) { faults++; PcviewerCompactError("view lost"); }
static void initialize(void)
{
	if(view != NULL) PcviewerCompactDestroyTerminal();
	memset(&mini,0,sizeof(mini)); memset(children,0,sizeof(children));
	main_window.exists=main_window.enabled=TRUE;
	progress_host.exists=TRUE; unrelated.exists=TRUE;
	fail_create=fail_timer=fail_control=fail_show=0;
	cancel_calls=accepted=reenter=faults=notices=timer_live=destroy_count=0;
	order=destruction_order=invalidation_order=0;
	posted=fail_post=nested_error=0;
	PcviewerCompactEnable(TRUE);
	CHECK(PcviewerCompactInit(hMainDialog,(HINSTANCE)1,cancel_handler,fault_handler));
}
static void prepare_show(void)
{
	initialize();
	CHECK(PcviewerCompactPrepare("SD test","Finishing"));
	CHECK(!mini.shown && timer_live && children[4].range==MAX_PROGRESS);
	CHECK(PcviewerCompactGetOwner(hMainDialog)==NULL);
	CHECK(PcviewerCompactShowBeforeFormat());
	CHECK(PcviewerCompactGetOwner(hMainDialog)==&mini);
	CHECK(PcviewerCompactGetOwner(&unrelated)==&unrelated);
}
static void snapshot_tests(void)
{
	char long_text[4096], copied[PCV_TEXT_CAPACITY];
	int i, before;
	prepare_show();
	memset(long_text,'a',2047); memcpy(long_text+2047,"\xe3\x81\x82",4);
	copy_utf8(copied,long_text); CHECK(strlen(copied)==2047);
	memset(long_text,'a',2045); memcpy(long_text+2045,"\xe3\x81\x82",4);
	copy_utf8(copied,long_text); CHECK(strlen(copied)==2048);
	copy_utf8(copied,"\xe3"); CHECK(strcmp(copied,"?")==0);
	PcviewerCompactSnapshotStatus("original");
	strcpy(long_text,"owned copy"); PcviewerCompactSnapshotStatus(long_text); long_text[0]='X';
	CHECK(strcmp(snapshot.status,"owned copy")==0);
	before=ui_calls;
	for(i=0;i<50;i++) { PcviewerCompactSnapshotStatus("status"); PcviewerCompactSetElapsed("00:12"); PcviewerCompactSetMarquee(TRUE); }
	CHECK(ui_calls==before); /* Hooks touch no windows, even if called by a worker. */
	strcpy(progress_host.text,"formatting"); progress_host.pos=123; progress_host.state=2;
	dialog_callback(&mini,WM_TIMER,PCV_VIEW_TIMER,0);
	CHECK(children[4].pos==123 && children[4].state==2 && children[4].marquee);
	CHECK(strcmp(children[0].text,"formatting")==0 && strcmp(children[1].text,"status")==0);
	CHECK(strcmp(children[2].text,"00:12")==0);
	PcviewerCompactSetMarquee(FALSE); dialog_callback(&mini,WM_TIMER,PCV_VIEW_TIMER,0);
	CHECK(!children[4].marquee && !(children[4].style&PBS_MARQUEE));
	PcviewerCompactDestroyTerminal();
	CHECK(timer_live==0 && destruction_order>invalidation_order && invalidation_order>0);
	PcviewerCompactSnapshotStatus("ignored"); CHECK(strcmp(snapshot.status,"status")==0);
}
static void input_tests(void)
{
	MSG msg={0};
	prepare_show(); reenter=1;
	dialog_callback(&mini,WM_COMMAND,IDCANCEL,0); CHECK(cancel_calls==1); /* No -> retry allowed. */
	PcviewerCompactModalEnter(); PcviewerCompactModalEnter();
	dialog_callback(&mini,WM_CLOSE,0,0); CHECK(cancel_calls==1);
	PcviewerCompactModalLeave(); dialog_callback(&mini,WM_CLOSE,0,0); CHECK(cancel_calls==1);
	PcviewerCompactModalLeave();
	mini.enabled=FALSE; dialog_callback(&mini,WM_CLOSE,0,0); CHECK(cancel_calls==1); mini.enabled=TRUE;
	accepted=1;
	msg.message=WM_COMMAND; msg.wParam=IDOK;
	CHECK(PcviewerCompactIsDialogMessage(&msg)); CHECK(cancel_calls==2 && !children[5].enabled);
	dialog_callback(&mini,WM_COMMAND,IDCANCEL,0); dialog_callback(&mini,WM_CLOSE,0,0);
	CHECK(cancel_calls==2 && destroy_count==0);
	prepare_show(); PcviewerCompactSetPhaseFinishing();
	dialog_callback(&mini,WM_CLOSE,0,0); dialog_callback(&mini,WM_COMMAND,IDOK,0);
	CHECK(cancel_calls==0 && !children[5].enabled && strcmp(children[1].text,"Finishing")==0);
	CHECK(destroy_count==0);
}
static void failure_tests(void)
{
	initialize(); fail_create=1; CHECK(!PcviewerCompactPrepare("SD","Finishing")); CHECK(!timer_live);
	initialize(); fail_control=IDC_PCV_INFO; CHECK(!PcviewerCompactPrepare("SD","Finishing")); CHECK(destroy_count==1);
	initialize(); fail_timer=1; CHECK(!PcviewerCompactPrepare("SD","Finishing")); CHECK(destroy_count==1);
	initialize(); CHECK(PcviewerCompactPrepare("SD","Finishing")); fail_show=1; CHECK(!PcviewerCompactShowBeforeFormat());
	PcviewerCompactError("hidden error"); PcviewerCompactError("hidden error"); CHECK(notices==0);
	CHECK(!PcviewerCompactCanFinish()); PcviewerCompactDispatchPending(); CHECK(notices==1);
	prepare_show(); children[0].exists=FALSE;
	dialog_callback(&mini,WM_TIMER,PCV_VIEW_TIMER,0); dialog_callback(&mini,WM_TIMER,PCV_VIEW_TIMER,0);
	CHECK(faults==1 && notices==1 && destroy_count==0);
	prepare_show(); PcviewerCompactSetPhaseFinishing(); mini.shown=FALSE;
	dialog_callback(&mini,WM_TIMER,PCV_VIEW_TIMER,0); CHECK(faults==1 && notices==1);
	PcviewerCompactDestroyTerminal(); CHECK(faults==1);
}

static void pending_error_tests(void)
{
	initialize();
	CHECK(PcviewerCompactCanFinish());
	PcviewerCompactModalEnter();
	CHECK(!PcviewerCompactCanFinish());
	PcviewerCompactError("outer modal error");
	CHECK(posted == 1 && notices == 0);
	PcviewerCompactDispatchPending();
	CHECK(notices == 0 && !PcviewerCompactCanFinish());
	PcviewerCompactModalLeave();
	CHECK(notices == 0 && posted == 2); /* Still outside NotificationEx cleanup. */
	CHECK(!PcviewerCompactCanFinish());
	PcviewerCompactDispatchPending();
	CHECK(notices == 1 && PcviewerCompactCanFinish());
	initialize(); nested_error=1;
	PcviewerCompactError("first error");
	PcviewerCompactDispatchPending();
	CHECK(notices==1 && !PcviewerCompactCanFinish());
	PcviewerCompactDispatchPending();
	CHECK(notices==2 && strcmp(notice_text,"queued second error")==0);
	CHECK(PcviewerCompactCanFinish());
	/* A failed post retains the copy for the view/finishing timer fallback. */
	prepare_show(); fail_post=1;
	PcviewerCompactError("post failed");
	CHECK(posted==0 && notices==0 && !PcviewerCompactCanFinish());
	dialog_callback(&mini,WM_TIMER,PCV_VIEW_TIMER,0);
	CHECK(notices==1 && PcviewerCompactCanFinish());
}

int main(void)
{
	PcviewerCompactEnable(FALSE); CHECK(PcviewerCompactGetOwner(hMainDialog)==hMainDialog);
	PcviewerCompactEnable(TRUE); CHECK(PcviewerCompactGetOwner(hMainDialog)==NULL);
	snapshot_tests(); input_tests(); failure_tests(); pending_error_tests();
	printf("PCVIEWER_COMPACT_UI_TEST_PASS assertions=%u; fake window APIs; no device calls\n",assertions);
	return 0;
}

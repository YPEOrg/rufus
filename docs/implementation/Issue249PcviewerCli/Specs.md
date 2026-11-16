# Issue 249 実装計画

## Goal

既存GUI処理へ専用起動契約を接続し、対象/固定条件/一操作結果だけを追加する。通常GUIを別実装に置換しない。

## Constraints

基準`cef215734b35f50b514125a273ef6b71c7fc9c38`、`ypp-rufus`、開始clean。本repoにAGENTSなし、共通規約と計画スキルを適用。計画のみ。既存巨大rufus.c全体の分割はせず、新規契約解析/状態を小moduleへ分ける。Windows build/manualはユーザー担当。

## Acceptance criteria

[Requirements](Requirements.md) AC1〜7へ下記Task/Vケースを対応。静的読取りと設計上の決定であり、動作実証済みではない。

## 実在根拠と変更マップ

| IM | 基準source | 変更境界 |
|---|---|---|
| IM1 | src/rufus.c: WinMain L3359〜、long_options L3375、引数変換L3524〜、getopt L3571〜、out L4288、return0 L4349 | 専用引数preflightと専用exit値。通常getopt経路を維持。 |
| IM2 | 同: WM_INITDIALOG L2860〜、IDC_START L2681〜、UM_FORMAT_START L3093〜 | 対象/固定UI値の整合後に既存Startを1回投入。既存確認を維持。 |
| IM3 | 同: aborted_start L3158〜、UM_FORMAT_COMPLETED L3175〜/ErrorStatus reset L3256、WM_CLOSE L3038〜、IDCANCEL L2344〜 | 専用stateラッチと終端確認。通常取消/進捗は再利用。 |
| IM4 | src/drive.c:GetDriveNumber L1062〜、src/dev.c:GetDevices、rufus.hのDRIVE_INDEX_MIN/MAX | GUIDを開いて既存disk解決・DeviceList対応を利用。物理番号と内部DriveIndexを混同しない。 |
| IM5 | rufus.c:SetComboEntry/SetFileSystems/SetClusterSizes、Alt-L L4093〜; format.c L1798〜/FormatPartition L1810 | UI item dataを用いて固定値を適用。force_large_fat32も専用modeでtrue。workerはUI値を読むためUI無効化だけでは足りない。 |
| IM6 | format.c:out L2035〜unlockclose L2041〜/completion通知L2077/ExitThread(0)L2078 | 再利用/参照のみ。TRUE通知とthread exit0はエラーでも同じ。engine改修・全面監査は対象外。 |
| IM7 | src/Makefile.am/.in、.vs/rufus.vcxproj/.filters、rufus.sln、.github/workflows/vs2026.yml | 新moduleを両build定義へ登録。既存READMEのVS2026/MinGW方式を維持。CI自動成功を主張せずpushしない。 |

新規候補: `src/pcviewer_cli.c/.h`（引数と純粋state/結果）、`src/pcviewer_integration.c/.h`（対象解決/固定profileのWindows/UI境界）、`tests/pcviewer_cli_test.c`（非破壊契約試験）。名前は新設であり既存APIではない。既存rufus.cにはmode guardと既存handler接続のみを局所追加し、WinMain全体の再設計や別formatterを作らない。

## D1 専用解析と初期化

専用preflightは既存GUI初期化・getoptの副作用より前に`CommandLineToArgvW(GetCommandLineW())`からUnicode argvを取得し、所有bufferをLocalFreeする。[Microsoft仕様](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-commandlinetoargvw)に従いtoken化と契約文法検査を分ける。専用optionを含まない通常起動はpreflight情報を捨て既存getopt/GUIへ戻す。

予約`--pcviewer-` prefix、`--target-volume-guid`/`--expected-disk-number`（完全一致または直後`=`の形式）のいずれかを含む場合は専用要求として厳密parserへ渡す。v2等未知専用modeや専用引数だけの要求も64。v1はvalueなし、GUIDとdiskは別token各1つ、順序自由、`--name=value`省略形式は認めない。その他legacy option（help/iso/gui/filesystem含む）との混在、位置引数、末尾余剰は64。GUIDは8-4-4-4-12のASCII hexとハイフン、括弧なし。diskは先頭0可、逐桁overflow検査で0〜UINT32_MAX、符号/空白/hex/Unicode数字拒否。parserはOSアクセス/設定変更を行わない。

有効専用要求はimmutable target/profileと初期結果10を保持。通常GUIの最終0と別変数にする。専用解析を既存getoptにもう一度通さず、locale/INI/UI等の通常初期化は必要な部分を再利用する。preflightのargv取得自体が失敗しmode判別不能なら10で起動失敗とし、GUIもformatも開始しない（通常GUIの正常終了0を変更する意味ではない）。専用modeではconsole hogger/別プロセスlauncherを作らず、本体processの寿命を観測対象にする。

## D2 指定対象と固定profile

GUI初期化・保存設定読込み・GetDevices完了後、GUIDから`\\?\Volume{GUID}`形のvolume deviceを開き、既存GetDriveNumberで物理disk番号を解決。成功番号とexpectedを照合する。MAX_DRIVES以上・extent取得成功で複数extentと判明した場合・open/解決失敗は64。GetDriveNumberはextent取得失敗等では既存IOCTL_STORAGE_GET_DEVICE_NUMBERへfallbackするため、その場合に複数extentを検出できるとは保証しない。既存sourceのfallback解決規則はそのまま利用し、新しい全媒体保護基盤へ拡張しない。

DeviceList item dataのDriveIndexとdisk番号はDRIVE_INDEX_MIN差を既存定義に従って変換し、一致項目1つだけ選択。UI列挙にない対象を強制登録せず64。対象ラベル/GUID/sizeを同一性の代替保証にはしない。

対象選択による通常UI更新後、SetComboEntry等を使いboot=BT_NON_BOOTABLE/partition=MBR/fs=FAT32/cluster=32768を設定、force_large_fat32=true/quick checked/extended label unchecked/badblocks unchecked。image/zero/write_as_image等の別操作は無効。項目やサイズが既存engine/UIで表現不能なら64、別FS/clusterへ自動変更しない。専用固定値をユーザーの通常設定へ保存しない。

対象とformat設定controlsを専用modeでは編集不可とし、IDC_START前および確認後のFormatThread生成直前にGUID→disk→選択itemと固定profileを再確認する。確認中hotplug/再列挙で消失・不一致なら64、他候補へ切替えない。worker開始後のengineの媒体エラーは10/20で扱い64へ戻さない。GUI/保存設定/shortcutが固定値へ上書きされないよう、専用modeではformat条件を変えるcommandとAlt shortcut処理を抑止する（取消/確認/通常window操作は残す）。Alt-L等の設定保存副作用も専用modeでは実行しない。通常modeへは変更を波及させない。

## D3 既存Start・確認・進捗・取消

WM_INITDIALOGのInitDialog/GetDevices/EnableControls/UpdateImage完了後、専用要求がReadyなら内部messageを1回postして既存IDC_STARTへ接続する。新規one-shot stateが`Ready→Checking`を先に確定し、連打/重複post/再列挙から二重Startを防ぐ。既存format_thread非NULL判定だけに依存しない（BootCheck中はNULL）。外部SendKeys/PostMessage自動化は使わない。

`IDC_START→BootCheckThread→UM_FORMAT_START→既存容量/使用中/消去/複数partition確認→FormatThread`を維持。いずれの確認も自動OK/YESへ変えない。ユーザー待ちが長くても承認を推測しない。通常進捗とCancel確認は既存UIを使用し、Cancel確認でNoなら継続、Yesなら既存ErrorStatus=ERROR_CANCELLEDの協調停止を使う。未開始で閉じる/確認取消は20。既存op_in_progress中のWM_CLOSE抑止も維持。

## D4 結果とprocess寿命

専用state: Parsed→Ready→Checking→Confirming→Formatting→Finishing→Terminal。一方向・一回終端。結果初期10、format_started=false。専用modeでのみBootCheck/FormatThreadの実handleを所有/保持し、既存global format_threadをNULLへ戻す処理とは独立させる。

| 経路 | 専用結果・条件 |
|---|---|
| 引数/対象/profile拒否、format前の再照合失敗 | 64、FormatThread未起動 |
| 起動初期化/mutex競合/BootCheck失敗/worker生成失敗 | 10、未実行でも0にしない |
| 未開始UI閉じる、既存確認Cancel/No、BOOTCHECK_CANCEL | 20。aborted_startのGUI復帰break経路にも専用終端を追加 |
| engine完了時ErrorStatusがERROR_CANCELLED | 20、変更有無は保証しない |
| engine完了時IS_ERROR(ErrorStatus) | 10 |
| 実FormatThread開始済み、今回完了通知、reset前ErrorStatus正常、全worker終了確認済み | 0 |
| 完了通知/worker handle/終端確認が不整合 | 10候補、終了未確認の間は成功終了しない |

UM_FORMAT_COMPLETEDは`format_thread=NULL`/`ErrorStatus=0`より前に専用結果をラッチし、その後の既存成功/失敗/取消表示をそのまま行う。完了通知TRUEまたはthread exit0だけで0にしない。aborted_startのBOOTCHECK_CANCELは完了通知へ来ないため別に20をラッチ。既存失敗/取消通知のcloseを待つことは維持し、勝手に通知を閉じない。完了後のCancel/CloseやErrorStatus resetでラッチを変更しない。既存completion処理はcontrolsを再有効化しformat_threadをNULLにするため、それを専用process終了許可には使わない。専用modeのTerminal許可前はWM_CLOSE/IDCANCEL/IDOK等の通常終了、および言語変更等のrelaunchを専用Finishingへ集約し、全worker終端未確認ならPostQuitMessage/cleanupをdeferする。既存の作業中Cancel/確認UIは維持し、Finishing中は追加Startも許可しない。専用modeでrelaunchしない。OSの強制終了/shutdown完了やcrashまで阻止・契約値へ変換する保証はしない。

通知処理後は専用finishing timerで保持worker handleへWaitForSingleObject(handle,0)相当の非blocking確認を行う。全worker signaled後にhandleをcloseして既存終了cleanupへ進み、専用resultを返す。GUI threadを無期限Waitで止めず、プロセスkillによる取消を実装しない。待機失敗/不整合はエラー表示して成功終了せず、既存thread継続有無を確認できるまで保持する。OS強制終了/crashのcodeを契約0/10/20/64へ偽装しない。

固定非boot/FAT32経路のFormatThreadは終了通知前にhLogicalVolume/hPhysicalDriveのunlockcloseを行うことを現sourceで確認。0はその既存engineの正常終端報告を意味し、内部write全経路の新たな監査保証ではない。format.c本体の新formatter/再設計は行わない。

## 検証設計

| V / AC | 具体入力/操作 | 期待証拠 |
|---|---|---|
| V1 / AC1 | 正規GUID/大文字/uint32 0/max/先頭0、負数/符号/空白/hex/overflow/全角 | 純粋parserのnormalize/拒否64。maxは文法成功でも実対象MAX_DRIVES制限で64。 |
| V2 / AC1 | 各option欠落/重複/未知/余剰、順序変更、legacy混在、専用v2、専用引数のみ、`--target-volume-guid=...`単独等の禁止省略形式 | valid順序だけ許可、他64、対象I/O/format呼出0。通常CLIテスト不変。 |
| V3 / AC2 | fake GUID解決/DeviceListなし/複数/番号不一致/unsupported/extent取得成功かつmulti-extent、extent取得失敗から既存fallback | 指定一致1件だけ選択、別候補fallback0。既存extent失敗fallbackと複数extent検出済拒否を区別。Windows実volume解決は後段。 |
| V4 / AC3 | 通常保存設定がexFAT/GPT、small/large媒体、cluster項目なし、Alt-L/対象変更 | 固定profile適用、変更不能、設定永続化なし。表現不能64、Start0。 |
| V5 / AC4 | 通常Start導線の各確認OK/Cancel/No、取消確認No→継続/Yes→停止 | 既存dialog内容/順序/進捗を維持、勝手な承認0、専用20。PCViewer側10分timeoutとRufus待機は別。 |
| V6 / AC4 | Checking中のStart二重post/クリック、確認中hotplug/再列挙 | 一操作一worker、消失/不一致64、他disk初期化0。 |
| V7 / AC5/6 | BootCheck生成失敗/format生成失敗/engine error/cancel、完了後ErrorStatus reset | 10/20を保持、未開始success0。通知TRUE/thread exit0のみを入力してもsuccess0にならない。 |
| V8 / AC6 | 完了通知到着時worker未終了、handle signaled遅延、待機失敗、完了直前Cancel、completion後worker未終了のClose/IDOK/言語relaunch要求 | 実worker終了前process0禁止、UI応答維持、Terminal許可前は通常終了要求でもprocess終了回数0（worker終端確認まで終了延期）、結果ラッチ1回、handle close1回。 |
| V9 / AC7 | 専用flagなし通常GUI/従来CLI、通常Start/取消/表示変更/設定保存 | 従来動作/正常GUI終了0維持。専用終了を通常modeへ漏らさない。 |
| W1 / AC1〜7 | ユーザーが対応版Windows build、廃棄可能な許可済SDで専用起動→既存確認操作→完了 | binary/commit/hash、引数、exit、固定MBR/FAT32/32KiBと対象一致、PCViewer process観測を記録。実施前にデータ消去を明示。 |
| W2 / AC4〜7 | 同じ対象版で確認取消/作業中取消/失敗/通常GUI回帰 | UI維持と終了値、未変更を推測しない。破壊試験はユーザー担当。 |

将来追加するpure C parser/state testsは`cc -std=c11 -Wall -Wextra -Werror -Isrc src/pcviewer_cli.c tests/pcviewer_cli_test.c -o <temp>/pcviewer-cli-tests`でMacでも実行可能な設計（Windowsヘッダーを純粋moduleに入れない）。コマンド対象は現時点で未作成。target/UI adapter試験はfakeを使い実SDへアクセスしない。

WindowsはREADME/rufus.sln/.github/workflows/vs2026.ymlのRelease/x64を初回基準とし、ユーザーへMSBuildと手動W1/W2を引継ぐ。MinGWの新source登録も整合させるが今回実行しない。既存CIの全platform成功を未実証で主張しない。失敗/未実施を成功としない。

## WBS・引継ぎ・切戻し

読む順序: Requirements→本書→Review→IM1〜6→既存build定義。指定branch/HEAD/作業差分を着手直前に再確認。

| Task | 対象/依存 | Done・commit単位 |
|---|---|---|
| T1 契約核 | 新pcviewer_cli、D1/D4、V1/V2/V7/V8 | 純粋parser/state試験と既存normal mode分岐を実装。単一目的commit A。 |
| T2 既存導線接続 | IM1〜5/新integration、T1、D2〜4 | 固定対象/profile/one-shot/既存dialog維持/終端handleを接続、fake adapter試験。T1と分ける場合commit Bは同じ機能の統合目的。 |
| T3 buildと契約文書 | IM7、T1/T2 | source登録/起動例/exit表/通常GUI回帰手順を同期、コード独立レビュー。T2と同一commit可。 |
| T4 Windows受入 | T1〜3、W1/W2 | ユーザーbuild/手動証拠。未実施の間は実機結合受入Hold。 |

今回の開始ゲートがPassでも実装は始めない。局所変数名/既存整形は実装裁量。確認省略、fixed条件変更、別formatter、PCViewer復元保証追加は裁量外。

rollbackは専用mode分岐/module/build登録をrevertし通常GUIに戻す。既存SDデータはコードrevertでは戻らない。新規保存データ形式/設定移行なし。

## 未実証・リスク

Windows UI/対象解決とthread終端の結合、通常GUI非回帰は静的根拠のみ。UI event再入/shortcut/saved設定による固定値逸脱とErrorStatus reset競合をV4〜8で重点確認する。これらは実装後検証事項で、未決仕様の放置ではない。実装開始可否は独立Reviewで判定する。

## U: 小型UI追加計画（2026-11-16、現在の実装対象）

基準`7e503512`。上記D1〜D4の処理/結果契約を維持し、D3の可視本体UIだけを本節へ置換。今回の依頼は計画commitまでで、実装しない。以前の実装/試験結果は[ImplementationReport](ImplementationReport.md)の履歴であり、新UIの成功証拠ではない。

### UI調査・影響マップ

| IM | 実在箇所 / 変更 |
|---|---|
| UI1 | `rufus.c`: WM_INITDIALOGのtopmost/foreground/focus、WinMainのShowWindow/UpdateWindow、MainCallback/ClockTimer/UM_PROGRESS_INIT・EXIT、PcviewerFinishTimer/Later。専用だけ非表示にし、小型viewへの状態投影を接続。 |
| UI2 | `stdlg.c`: MyDialogBoxは必ずShowWindow(hMainDialog,SW_NORMAL)、NotificationExはhMainDialogをowner指定。専用owner解決/本体再表示抑止を追加。本文・button・返値・既存Cancel確認は変えない。 |
| UI3 | `ui.c`: UpdateProgressはhProgressへPBM_SETPOS。`localization.c`: PrintInfoMessageはprogress text、PrintStatusMessageはowner-draw statusへ文字列。既存送信先を変更せず、Rufus内snapshotへ表示データだけ投影。 |
| UI4 | 新規`src/pcviewer_compact_ui.c/.h`: 小型modeless window、進捗/情報/状態/時間/取消、snapshot/owner/lifetime。新規`IDD_PCV_PROGRESS`とcontrol IDを`rufus.rc/resource.h`へ。必要な新規文言は既存`res/loc/rufus.loc`方式の英語fallback/日本語へ追加、既存Cancel/通知文言は再利用。VS/MinGW登録。 |
| UI5 | `tests/` 新規pure view-state/fake window tests、既存CLI/adapter試験維持。実Windowsのowner/描画/入力は後段。 |

`IDD_DIALOG`は現行resourceにWS_VISIBLEなし。したがって生成直後hideするだけでなく、専用modeでは最初からShow/foreground/focusを行わない。hMainDialog/hProgress/hStatusやGetDlgItem依存を小型へ付け替えない。隠した既存mainは処理ホストとして生存させ、追加viewは表示と取消の委譲のみを担当する。既存大クラス全体の再構成や別formatterはしない。

### U1 起動・確認・owner

1. 専用引数検査と既存初期化は維持。hidden mainを作り、既存controlsを固定する。
2. 既存Startをpostする**前**に小型windowと必須controls/timerの準備を完了する。小型もこの段階は非表示。作成/初期化失敗は10をラッチしworker起動0。主画面をfallback表示して勝手に続行しない。
3. 既存Start→BootCheck→既存警告/消去確認へ。専用MyDialogBoxは本体をShowしない。小型非表示時の通知ownerはNULL（既存CenterDialogのscreen中央配置を使用）、小型可視時は小型をownerにする。owner差替えはhMainDialogを指定するroot通知に限定し、通知内more-info等の子dialogの既存ownerは維持する。専用modeの共通MyDialogBoxはどの経路でもmainをShowしない。通常modeは今のowner/ShowWindow動作を維持。通知は従来modalで、button/意味/返値を変えない。確認中は小型の取消操作を受け付けない。
4. 最終確認と対象/profile再照合を終え、FormatThread生成**直前**に小型を表示して可視性/必須controlsを確認する。失敗なら10・worker起動0。既存Start内部からFormatThreadを起動し、進捗を小型へ投影する。生成失敗は既存失敗通知＋10。
5. 確認取消は小型を出さず既存20終端へ。必要な失敗/取消通知は表示してその閉じ操作を待つ。UAC/署名警告/二重起動警告等の既存起動時通知は非表示化の対象にしない。

visible ownerの選択は新規の狭い関数をMyDialogBox/Notificationの境界から利用する。一般dialog frameworkは作らず、通常表示経路へ専用判断をばらまかない。mainのtopmost/foreground、MyDialogBox、終了時再表示の全経路をUI1/2で確認する。Taskbar進捗/Flashの表示先も専用の可視小型へ解決し、既存`dialog_handle`やengine用hMainDialogは置換しない。ログ画面を本体代わりに自動表示しない。

### U2 進捗と取消・寿命

小型は対象の表示名、進捗bar、処理情報、状態文、経過時間、取消buttonを持つ。初期幅約360 DIP、既存Segoe UI/配色・翻訳文字列を使いDPI/長文で折返し。別のパーセント算出はせず既存MAX_PROGRESS、PBM状態/marquee、PrintInfo/PrintStatus/ClockTimerの値を使う。

既存windowへの更新は維持。新snapshotへコピーするhookだけ追加し、workerからview controlsへ直接触らない。snapshot文字列は呼出中に所有コピー（上限2048 UTF-8 bytes、境界で安全に切詰め、元文字列のpointer保持禁止）、短時間lockで更新。UI側100ms timerがsnapshotを読み、lockを離してからcontrolsを更新する。lock中SendMessage/Notification/waitをしない。進捗数値はhidden hProgressのPBM_GETPOS/GETSTATEをUI timerが参照でき、marqueeと経過時間は既存UI handlerからsnapshotへ渡す。statusbar owner-draw pointerをSB_GETTEXTで文字列扱いしない。進捗はプロセス外へ送らない。

小型Cancel/×/Alt-F4はMainCallbackの既存IDCANCELへ一度委譲し、自前DestroyWindow/PostQuit/TerminateThreadを行わない。取消確認Noは継続、Yesは既存ERROR_CANCELLED協調停止、取消受付後もworker終端を待つ。WinMainのIsDialogMessage対象へ専用modeless viewも追加し、Tab移動を保つ。小型には破壊承認buttonを置かずEnter/Esc/Alt-F4も取消要求として同じ経路へ委譲し、二重送信しない。通知表示中は小型を無効化し再入を抑止（worker threadからの既存通知でもowner無効状態を維持）。Finishingでは取消/×を無効扱いとし「後処理・終了確認中」を表示、100%でも閉じない。

小型とsnapshotはworker終端・既存結果通知の完了までは維持し、保持worker全終了・handle解放・Terminal確定後にtimer停止→投影hook無効化→view破棄→mainを含む既存process cleanupの順で破棄する。wait/close/SetTimer失敗は小型が可視ならそこへ理由を表示し、確認取消後など小型非表示/未作成の場合は既存Notificationで必ず可視に知らせる（同一異常の通知を連打しない）。hidden controlsへの文字設定だけで通知済みとしない。開始前ならworker起動0のまま、成功偽装/強制killなし。実行中に小型表示/controlsが失われた場合は、成功ラッチ後のFinishingも含めPcviewerTerminalCheckFailed等で成功資格を失効させる（既確定10/20は保持）。workerが稼働中なら既存協調取消を要求し、別の既存Notificationで状況を可視化して終了を待つ（main表示fallbackなし、0にしない）。OS強制終了まで復旧できる保証はしない。

### UI検証計画

| Test / AC | 操作・入力 | 期待証拠 |
|---|---|---|
| UV1 / UAC1,5 | 専用/通常の起動→確認→終了、瞬間window可視性を記録 | 専用main visible回数0、通常は従来表示。UAC/manifest不変。 |
| UV2 / UAC2 | 各条件付き警告/消去確認OK/Cancel、通知owner・最小化/Alt-Tab/前面 | 既存本文/ボタン/順序、無断承認0、操作可能、通知中小型入力0。 |
| UV3 / UAC3 | 0/途中/100%、marquee/paused/error、長文・マルチbyte・DPI、snapshot更新競合 | 既存値を投影、壊れた文字列なし、UI threadのみ操作、PCViewer通信0。 |
| UV4 / UAC3,4 | 小型Cancel/×/Enter/Esc/Alt-F4/Tab、確認No→継続/Yes→停止、連打、Finishing閉じ要求 | 既存取消1回、worker生存時destroy/exit0回、Terminal後のみ破棄。 |
| UV5 / UAC4 | 小型/必須control/timer失敗、format生成失敗、実行中view喪失、worker wait/close異常、確認取消後の小型hidden状態で終端timer失敗、成功ラッチ後Finishingのview喪失 | 起動前はworker0/10、開始後は協調取消＋可視通知・成功0禁止。確認取消後の終端異常も可視Notification必須、main表示0/worker追加起動0。 |
| UV6 / UAC4,5 | 全worker遅延・通知再入・normalGUI/CLI、固定profile回帰 | 既存471assertions継続、normal動作不変、独立ラッチ/終了条件不変。 |

pure/fakeはstate→表示命令/owner/取消委譲/破棄順を新規testで検証し、VS/MinGW登録を静的確認。旧274/197件が追加UIを実行しないことを明記。Windows表示・modal owner・DPI・入力はWindows build後の結合検証が必要。今回の計画工程では全て未実施。実formatを伴う試験は別の消去許可がある時だけ実施し、現在の確認画面停止という許可を拡張しない。

### WBS・ゲート・切戻し

| Task | 対象/依存 | Done / commit目的 |
|---|---|---|
| U-T1 | UI4/5、U2 | 新view-state/snapshot/取消委譲とfake試験。表示専用責務、CLI/engine非変更。 |
| U-T2 | UI1/2/3、U-T1 | hidden main/owner/表示投影と既存Start接続、UV1〜6の非破壊試験。U-T1と同じ小型UI機能commitに統合可。 |
| U-T3 | build/resource/文書、U-T2 | 独立コードレビュー、既存試験/新fake試験、Windows引継ぎ。 |
| U-T4 | U-T3 | Windows build・実可視性/操作検証。媒体消去は別許可まで行わない。 |

読む順序: Requirements追加UAC→本節→Review最新判定→UI1〜5。UI公開挙動は本節で確定、局所レイアウト/識別子は実装裁量。実装開始ゲートは独立レビュー後に判定し、Windows受入は未実証のまま分離する。切戻しは追加view/owner分岐/hook/resource/buildをrevertして`7e503512`の専用本体表示へ戻す。CLI/設定形式移行なし、SD内容をrollbackで復元できるとはしない。

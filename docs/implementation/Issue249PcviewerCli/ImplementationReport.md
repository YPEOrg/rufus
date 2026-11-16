# Issue 249 実装記録

## 範囲・依頼の更新

2026-11-16、計画commit `d9bf30120ed03a5d427da0bd21b74eb3b90694f2` 後の明示依頼により実装へ移行。Requirements/Specsの「今回計画のみ」はその計画時点の作業制限であり、今回の承認は専用CLI実装・独立レビュー・Windowsビルド・指定SDの既存消去確認画面まで。**消去確認を確定しない。実format成功試験は未許可・未実施。** 通常GUIの既存経路を再利用し、媒体保護基盤やformatterの追加改修はしない。

## 変更

- `pcviewer_cli`: Unicode argvの厳密解析、通常起動判別、専用one-shot状態・結果ラッチ。
- `pcviewer_integration`: 既存GetDriveNumberによるGUID所属disk照合、現在選択/固定profile検査、controls固定、保持worker終端確認。
- `rufus.c`: 初期化後に指定対象/profileを設定、既存Startを一度送る。既存確認・進捗・取消を維持。結果をErrorStatus reset前に保存し、worker終端とhandle解放後に終了。通常モードは別分岐。
- VS/MinGW source登録、純粋核・偽Win32境界の非破壊試験。

## 検証と限界

| ケース | 方法 | 結果 |
|---|---|---|
| V1/V2、V6/V7/V8の純粋state部分 | Mac C11 `-Wall -Wextra -Werror`、ASan/UBSan | 274 assertions Pass（純粋核のみ）。 |
| V3/V4/V8 adapter | Mac fake Win32境界 | 197 assertions Pass、ASan/UBSanでもPass。実Windows API/GUIの実証ではない。 |
| Windows compile / V5/V9 実GUI | Windows担当 | この文書作成時点では新版未実施。旧 `d9bf301` の起動成功とは区別。 |
| W1/W2 実format成功/作業中取消/媒体内容確認 | 実SD | 未実施。今回の許可は確認画面までであり確定は禁止。 |

274件の純粋試験をMainCallback/Windows GUI/媒体列挙の成功とは扱わない。MacではWindows SDK/MinGWクロスコンパイラが確認できず、製品WindowsバイナリのコンパイルはWindows段階で確認する。通常GUI非回帰、保存設定組合せ、hotplug・確認中イベント、完了通知とUI再入はWindows結合の未検証事項。

## 独立レビュー

アーキテクチャ/安全の2担当へ差分レビュー依頼。純粋核は必須指摘なし。統合で検出された変数scope2件（boot_worker/DeviceNum）を修正。worker待機/タイマー失敗時に終了を延期し状態欄へ理由を表示。最終判定は追記する。

## Windows担当への手順

1. 対象commitを取得しRelease/x64をビルド。実行ファイルSHAとbuildログを保存。
2. 既存Rufusを終了し、通常GUIが起動することを確認。開始ボタンは押さない。
3. 最新のI:のGUIDとdisk番号を読取り取得し、ユーザー指定対象と一致を確認する。古い番号やGUIDをそのまま再利用しない。
4. 専用v1引数で起動。指定媒体/非boot/MBR/FAT32/32KiB/quick等の表示と既存消去確認の表示を観察する。**OK/YESで確定しない**。
5. 読取り証拠取得後、親/ユーザーの指示でCancelする場合は取消20を取得。format完了0をこの試験から主張しない。

実装前計画のPassと実装のWindows受入は別。未検証のW1/W2は保留を維持する。

## 再現コマンド（Mac、非破壊）

```sh
cc -std=c11 -Wall -Wextra -Werror -Isrc src/pcviewer_cli.c tests/pcviewer_cli_test.c -o /tmp/pcviewer-cli-tests
/tmp/pcviewer-cli-tests
cc -std=c11 -Wall -Wextra -Werror -DPCVIEWER_INTEGRATION_TEST -Isrc src/pcviewer_cli.c src/pcviewer_integration.c tests/pcviewer_integration_test.c -o /tmp/pcviewer-integration-tests
/tmp/pcviewer-integration-tests
```

実行結果: `PCVIEWER_CLI_TEST_PASS assertions=274`、`PCVIEWER_INTEGRATION_TEST_PASS assertions=197`。両者とも実device呼出なし。追加で担当が`-fsanitize=address,undefined -fno-omit-frame-pointer`付きで両試験を確認。VSプロジェクト/filters XML解析、`git diff --check`もPass。

fake adapterはGetDriveNumberそのもののWin32内部実装・実DeviceList初期選択・profile適用を行うMainCallback・Windows message再入を実行していない。これらは明確にWindows後段の未検証項目。

## WBS実装状況

- 🟩 T1: 純粋CLI/stateと非破壊試験。
- 🟩 T2: 既存導線接続とfake adapter試験（実Windows結合の成功を意味しない）。
- 🟩 T3: source登録・専用契約/結果の引継ぎ記録。独立レビュー最終結果は下記追記。
- 保留 T4: Windows新版build/UI/実媒体受入。今回のWindows操作は確認画面までに限定し、W1の実format成功は行わない。

## 最終独立レビュー結果

2026-11-16、アーキテクチャ担当・安全担当が修正後の実ファイルを再確認。必須未解消指摘なし。**Windowsビルドと既存消去確認画面までの検証へ渡すためのコミット可**。実Windows結合・実formatの受入Passではない。上記未検証範囲と消去確定禁止を維持する。

最終コードで担当自身も2試験を再実行し、ASan/UBSan結果を[TestEvidence.txt](TestEvidence.txt)へ保存した。

## 小型UI実装（計画5e2a7b1に対する追加、2026-11-16）

専用モードだけhidden mainを処理hostに保持し、小型modeless進捗/取消viewを追加。最終消去確認後、worker生成前に表示する。MyDialogBoxの本体再表示を専用時に抑止し、root通知ownerを小型へ解決（非表示ならNULL）。子dialog owner/通常GUIは維持。UAC/manifest/CLI/進捗IPC/formatter変更なし。

既存progress controlsは置換せず、status/time/marqueeは所有snapshotへコピー、UI timerで既存progress値とともに小型へ投影。取消は既存MainCallbackへ委譲、worker終了前にview/processを破棄しない。新dialogはresource/既存theme/localization方式を使う。

独立レビューで、異常Notificationのmodal loop中に終了timerが再入し先に終了する経路、および既存通知中の異常Notificationによる共有通知データ再入を検出。終端確定前のmodal/in-flight/pending gateとUI messageへのエラー遅延表示を実装し、NotificationEx全寿命を外側のmodal guardで保護した。成功資格の失効/協調停止は遅延しない。

小型fake 1668、既存CLI 274、adapter 197 assertions を通常およびASan/UBSan付きで再実行し全Pass。VS XML解析とdiff --checkもPass。[CompactUiTestEvidence.txt](CompactUiTestEvidence.txt)に結果保存。fakeはCanFinish条件・通知再入を検証するが、実MainCallback timer/実NotificationExの結合は未実証。Windows追加UIの描画・前面/入力・DPI・実confirmationはまだ未実証。Windows担当への検証は現在I:のread-only再確認、既存消去確認表示と取消まで。OK/実formatは行わない。

追加UIの最終独立レビュー: アーキテクチャ・安全の両担当が修正後コードを確認し、必須未解消なし。Windowsビルドおよび消去確認表示・取消まで渡す限定コミット可。実Windows受入/実formatのPassではない。

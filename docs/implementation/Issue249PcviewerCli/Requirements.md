# Issue 249 PCViewer専用CLI 要求

## Goal

[PCViewer #249](https://github.com/YPEOrg/CycleDR-PCViewer/issues/249)の契約をRufusへ追加し、指定SDを固定条件で既存フォーマット経路へ渡して結果を終了コードで返す。

## Constraints

- 対象はYPEOrg/rufusの`ypp-rufus`、基準`cef215734b35f50b514125a273ef6b71c7fc9c38`。2026-11-16にIssue本文・指定branchを取得。ユーザー最新条件は**Start後の既存確認ダイアログ・進捗・取消UIを維持**。Issue本文への未記載分よりこの指示を優先。
- 今回は調査→計画→独立レビュー→計画のみ指定branchへローカルcommit。製品/テスト変更、ビルド、媒体操作、pushなし。Windowsビルド〜手動試験はユーザー。
- formatterは既存FormatThread/FormatPartitionを利用。内部write全面監査、追加保護基盤、フォーマット方式再設計を行わない。PCViewer UI/設定退避復元は範囲外。
- 通常GUIの操作・保存設定・確認・終了契約は維持。専用modeだけ一操作後に終了する。

## Acceptance criteria

| ID | 期待結果 |
|---|---|
| AC1 | `--pcviewer-initialize-v1 --target-volume-guid <GUID> --expected-disk-number <uint32>`各1回必須。GUIDは括弧なしD形式36字、小文字正規化。uint32はASCII十進のみ。未知/重複/欠落/余剰/不正は64。 |
| AC2 | GUIDの現在の所属diskとexpected番号を照合して既存DeviceListの単一対象へ選択。対象なし/不一致/未サポートは64。disk番号だけ/別候補へのfallbackなし。 |
| AC3 | 非boot/MBR/Large FAT32/cluster32768/quick ON/拡張label-icon OFF/badblock OFFを固定。UI・保存設定・shortcutから別条件へ変わらない。 |
| AC4 | 既存Start→BootCheck→確認→FormatThreadの導線で1回だけ開始。確認を自動承認/省略せず、既存進捗/取消を維持。 |
| AC5 | 0=実format成功、10=失敗、20=取消、64=format開始前拒否。未実行終了/通常GUIのexit0/worker exit0/completion通知のみを成功にしない。 |
| AC6 | 成功は既存engineの正常終端状態と実worker終了・handle解放後のみ返す。失敗/取消は媒体未変更を保証しない。専用modeの一操作結果を通常ErrorStatus reset後も保持。 |
| AC7 | 従来GUI/CLIは意図的に変更しない。#222と引数・終了値が一致し、Windowsの実結果/対象版を記録する。 |

設定保持は渡さず、format後の媒体再検出/関連付けと復元はPCViewer責任。GUID/drive letterがformat後も不変という保証を追加しない。UACで起動自体が拒否された時はPCViewer側のlaunch失敗で、Rufus終了値が存在するとは扱わない。

## 2026-11-16 追加要求: 専用モードの小型UI（今回の正本）

基準 `7e5035129512d51954e97fae2f2ae143b6213183`、`ypp-rufus`、着手時clean。既存AC1〜7のCLI・固定profile・処理・終了契約は維持し、専用モードの表示だけを以下へ変更する。通常GUIを隠す仕様変更ではない。今回は調査・計画・独立レビュー・計画commitのみ。コード/テスト変更・Windowsビルド/手動操作は行わない。

| ID | 追加受入条件 |
|---|---|
| UAC1 | 専用モードのRufus本体画面は起動から終了まで表示しない（一瞬の露出も不可）。UACの表示・権限・manifestは変更しない。 |
| UAC2 | 初期化承認は既存消去確認のOK/取消で行う。既存の条件付き容量/使用中/複数partition等の警告や失敗通知も必要時に維持し、自動承認/省略しない。「確認だけ表示」は不要な本体画面を出さない意味で、必要警告の削除ではない。 |
| UAC3 | 承認後はRufus側の小型進捗・取消ダイアログを表示。既存の進捗/状態/経過時間を表示し、Cancel/×は既存協調取消へ委譲。PCViewerへの進捗通信・新引数/結果protocolは追加しない。 |
| UAC4 | 結果0/10/20/64と全worker終端確認を維持。100%表示だけで成功/終了しない。見えるUIの準備に失敗したらformatを始めず10、待機・表示異常を成功扱いしない。 |
| UAC5 | 通常モードの本体画面・確認・進捗・取消・表示設定・UACは従来どおり。Windowsでの表示/入力/受入は計画後の検証で、今回成功としない。 |

見た目の局所寸法/既存配色との調整は実装裁量。確認省略、UAC回避、formatter変更、PCViewer進捗IPCは裁量外。

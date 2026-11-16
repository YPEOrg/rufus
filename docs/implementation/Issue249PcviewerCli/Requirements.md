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

# Issue 249 計画レビュー

## Goal

既存Rufus導線・確認UIを維持した専用契約追加の着手可否を独立判断する。

## Constraints

指定branch ypp-rufus、基準cef215734b35f50b514125a273ef6b71c7fc9c38。文書のみ。Windows未試験を成功にしない。外部CLI・破壊処理との連携を変更するため標準3文書を使用。

## Acceptance criteria

G1〜G6の根拠、独立指摘と解消結果を記録する。正本は[Requirements](Requirements.md)と[Specs](Specs.md)。

## Layer 1: 開始判定

| Gate | 判定 | 根拠 |
|---|---|---|
| G1 要求 | Pass | #249契約とユーザー最新の既存確認・進捗・取消維持を適用。PCViewer/formatter再設計は対象外。 |
| G2 既存根拠 | Pass | 指定checkoutのStart/確認/format/終了status/buildを読取。GetDriveNumberのfallback限界も明示。 |
| G3 設計 | Pass | 専用module、normal mode分離、one-shot、既存UIとengine再利用。終端前の通常終了/relaunch抑止を確定。 |
| G4 検証 | Pass | pure/fake/Windows実機を分離、V1〜9/W1〜2。引数=形式/worker遅延/通常終了要求競合も具体化。 |
| G5 作業 | Pass | T1〜4依存と一目的commit、読み順・対象map明記。今回計画のみ。 |
| G6 影響 | Pass | 通常GUI/設定保全、rollback限界を明記。SD消去をrevertで戻せるとはしない。 |

**実装開始ゲート: Pass。Human Decision Required: なし。** 計画上T1から開始可能だが、ユーザーの今回依頼は計画までなので実装しない。

**Windows実機/通常GUI結合受入: Hold（未実証）**。Windowsビルド〜手動試験はユーザー担当。計画レビューPassは実装・テスト成功ではない。

## Layer 2: 独立レビューと修正

2026-11-16、独立アーキテクチャ担当と安全担当がRequirements/Specsおよび関連既存sourceを照合。T1〜T4、IM1〜IM7、D1〜D4/V1〜V9を確認した。

| 指摘 | 修正・確認 |
|---|---|
| completion時にformat_thread=NULL/controls再有効化され、worker終了待ちよりWM_CLOSE/IDCANCELが先に通常終了へ進める | D4で専用Terminal許可まで普通の終了・IDOK・relaunchをFinishingへ集約/defer。V8で終了回数0とhandle終端後終了を検証。最終差分で解消確認。 |
| `--target-volume-guid=...`等だけだと専用予約検出を抜ける可能性 | D1で完全一致/直後=を予約検出。厳密parserは=形式を64。V2追加。 |
| 複数extentの既存拒否とextent取得失敗時fallbackの保証範囲が曖昧 | D2/V3で取得成功の複数拒否と失敗時既存fallbackを区別。未取得時の複数検出を保証しない。 |
| 「process終了0」がexit0と紛らわしい | V8を「process終了回数0（worker終端確認まで終了延期）」へ明確化。 |

両担当とも最終実ファイルを再確認し、必須指摘なし・G1〜G6 Pass・計画のみcommit可と回答。未反映の必須指摘なし。Windows未試験は上記受入Holdへ残す。

## 今回の文書検証

ローカルMarkdown参照存在、指定branch/基準、git diff --check、stage対象がこの計画3文書だけであることを確認する。製品source/設定/テスト/Windows環境は変更・実行しない。PCViewer側の停止済#248未追跡草案は変更・commit・削除しない。

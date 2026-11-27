# Gufo v0.28.1 review — 2026-11-01

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

公式リリース、PR本文・差分、v0.28.1の実ソースを確認した。実行中のGufo、通常設定、採用ソースは変更していない。ユーザーの割り込み指示に従い、Attention境界修正の実装・コミット・同時利用3回テストは保留中。

## v0.28.1の変更

- 公開: 2026-10-31 22:44 JST。リリースコミット `12effe369662ac572cb100cd20157f56a74e2da0`。
- [公式リリース](https://github.com/gufo-org/gufo/releases/tag/v0.28.1)
- [PR #875](https://github.com/gufo-org/gufo/pull/875): Prefillを短く分割した場合やcheckpoint復元後の短い末尾で、長い一括処理とHCミキサーの演算順序が変わる問題を修正。F16 projection、SSM、HC combine、W8A8の順序も固定し、経路別のビット一致試験を追加。作者の検証はgfx1151／ROCm 7.2.4。今回のAttention密／疎境界とは別の演算箇所。
- [PR #882](https://github.com/gufo-org/gufo/pull/882): MTPの予測確率が低くなった時点で先読みチェーンを打ち切る。greedy 0.85、sampled 0.95、計画済みgreedy batchの末尾破棄0.6。無駄な検証の削減を狙う。UD-Q4_K_XL＋共有Q8_0 MTPの作者実測でC1/C4約11%改善。一方、一部16-token短文は約10%低下（+35〜65 ms）。Huihui IQ4_XS／ROCm 10.1での当方の測定ではない。samplingの固定seedによる文章は方針変更により変わり得る。

## 今回のAttention問題との照合

[attention-comparison.json](../../../../evidence/local/reports/gufo-v0281-review-20261101/attention-comparison.json) に関数本文のSHA-256と比較結果を保存。

v0.27.0とv0.28.1で以下が完全一致した:
- CompactSelectionKernel
- AttentionKernel
- AttentionMergeKernel
- rocm::Attention
- Executor::Attention全体

バッチ末尾で疎Attentionを判定する条件も残っている:
- executor.cpp line 3160: `start_pos + n > c.indexer_top_k`
- batch.cpp line 847: `end > c.indexer_top_k`

したがって、今回特定した2048トークン境界の演算順差を直接直す変更はv0.28.1に入っていない、というコード上の判断。v0.28.1自体の実機再現試験は未実施。MTP方針変更で境界をまたぐ幅・頻度が変わり、不一致が見えにくくなる可能性はあるが、修正完了の根拠にはしない。

## 現行v0.27から増えるv0.28.0の変更も確認

- [PR #860](https://github.com/gufo-org/gufo/pull/860): OpenAI互換の入力トークン数照会API。生成前にテンプレート・tools・reasoning・対応画像を含む入力長を確認できる。クライアントが利用する必要があり、Hermesが自動利用すると確認したわけではない。
- EmbeddingGemma 2の動画入力、要求ごとの画像処理予算。
- Qwen-Imageのsafetensors重み読み込み修正。
- Qwen3.8-27Bの2〜8行verification GEMM改善（Flash-Next専用の改善ではない）。
- [PR #872](https://github.com/gufo-org/gufo/pull/872): Windows HIPのインライン最適化フラグ修正。今回のWSL/Linuxビルドには適用されない。

[公式v0.28.0リリース](https://github.com/gufo-org/gufo/releases/tag/v0.28.0) と [v0.27.0からの比較](https://github.com/gufo-org/gufo/compare/v0.27.0...v0.28.1) も参照。

## 再開時の注意

ユーザーは、通常版向け最小修正・既存規約準拠・独立コミット・同時利用3回・通常設定へ復帰を依頼した後、「その作業の前にv0.28.1の修正内容を確認」と指示した。現時点でv0.28.1への更新は実行していない。

更新する場合は公式の数値一致修正を先に保持した土台で、残るAttention境界対応を独立させる。Huihui／Orca／ISTAの既存拡張と高速化を不用意に落とさない。通常構成はHuihui＋BF16 PLE、ROCm 10.1＋AMD_SERIALIZE_COPY=2、2セッション各131072、thinking auto、MTP max7/min1、Docker on-failure:3。


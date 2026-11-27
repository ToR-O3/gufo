# ToRO3モデル整理完了

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

2026-11-07 JST。ボスが「今回のOrca移植版＋内蔵PLE」を常用候補に選択し、ToRO3内を必要なモデルだけに整理するよう指示。モデルの通常起動への切替は実施していない。

## 現在のモデル

`/home/user/Models/ToRO3-Qwen3.8-Flash-Next/UD-IQ4_XS/`

| ファイル | bytes | SHA-256 |
|---|---:|---|
| `Qwen3.8-Flash-Next-UD-IQ4_XS-00001-of-00004.gguf` | 10946618 | `93ff23a08e5cd7501f24fa16f5b415d5807d0175e04edd3ed6a7a41b601a4eef` |
| `Qwen3.8-Flash-Next-UD-IQ4_XS-00002-of-00004.gguf` | 49604491008 | `d9aa9be6fa33a84a52a4a33edc7d85c0d504f8d568a85286914e0ec94c3d3e38` |
| `Qwen3.8-Flash-Next-UD-IQ4_XS-00003-of-00004.gguf` | 15256783680 | `018c32020ca7b49c59a3b9a0966d29e6244c2572c568ac8cfad7b1c0bad1e2d0` |
| `Qwen3.8-Flash-Next-UD-IQ4_XS-00004-of-00004.gguf` | 28800138432 | `5de8b5dbf89409acc39468decafd4a4a95b13c29d88a14b0356107897829ec36` |

合計 93,672,359,738 bytes（87.239 GiB）。4ファイルは全てローカル実体・0444で、リンクではない。分割1のGGUFを起動時に指定する。内蔵PLEはモデルに含まれ、この構成では外部PLEの指定は不要。API名は従来の候補名 `Qwen3.8-FN-UD-Orca-IQ4_XS` に対応する。

今回残したのは本命の146テンソル移植候補であり、第10層をQ8化した診断版や旧Luni試作版ではない。

## 実施内容

- `Orca-v31-staging/UD-IQ4_XS/` を `UD-IQ4_XS/` へ同一ファイルシステム内で移動。
- それまで `UD-IQ4_XS` に存在した旧LuniのNASリンクを削除。
- `Orca-v31-diagnostic-small`、`Orca-v31-diagnostic-q8down10`、`Orca-v31-packed-q8down10` の診断用ファイル・リンクとフォルダを削除。
- 空になった `Orca-v31-staging` を削除。

診断用のローカル実ファイル3個を削除し、32,839,854 bytes（約31.32 MiB）を除去した。リンク削除は12件。リンクをたどって共有モデルやNAS実体を削除していない。NAS上の旧Luni・診断用モデルは従来の保管場所に残る。

## 確認

移動前に4ファイル全体を今回SHA-256計算し、`/home/user/workspace/LLM/orca-v31/reports/assembly.json` の `verified.output_sha256` と一致。移動後は同じdevice・inode・サイズ・mtime・モードであることを照合した。内容の再加工やコピーは行っていない。NAS参照先のメタデータも前後一致。

Dockerコンテナの構成・稼働状態・起動終了時刻と `normal-settings.json` のSHAを前後照合し、変更なし。通常Gufoの停止・再起動・追加推論は行っていない。

初回はDockerのCmdがnullのコンテナを扱う事前確認で停止した。移動・削除前の失敗で、nullを空リストとして扱う修正後の実行は全工程を完了。失敗記録は `attempts.json` に保持。

## 過去の記録との対応

過去の比較レポート・生データ・加工用スクリプトにある `Orca-v31-staging/UD-IQ4_XS` は当時の場所。現在は上記の新しい場所を使用する。過去のSHA記録や入力条件は書き換えていない。古い加工・診断・NAS移動スクリプトをそのまま再実行しない。特に同名の `UD-IQ4_XS` は旧LuniリンクからOrca実体へ変わっているため、名前だけで旧Luniと判断してはいけない。

- [今回の品質比較](../../../../../v0.30/benchmarks/benchmark/reports/huihui-orca-v31-quality-20261107/report.ja.md)
- 移動・削除前の一覧（ローカル保管：before.json）
- 完了・照合結果（ローカル保管：result.json）
- 整理スクリプト（ローカル保管：/home/user/workspace/Gufo/local/orca-v31/cleanup_model.py）

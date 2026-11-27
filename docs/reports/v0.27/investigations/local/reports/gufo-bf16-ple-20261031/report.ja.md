# Huihui IQ4_XS用 BF16 n-gram 対応記録

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

2026-10-31。実装・ダウンロード・ビルドに加え、ユーザーの許可後に最小限の実モデル確認を完了しました。**Huihui IQ4_XS＋BF16 n-gramを通常設定で起動したままです。**

## 今回の変更

Huihui UD-IQ4_XS本体のGGUFは変更せず、外部のPLE（n-gram埋め込み表）を選ぶ `--ple-model PATH` を追加しました。BF16の読み取り・展開・キャッシュ・GPUへ渡す行データには既存の `NgramTable` を使います。新しいGPUカーネルは追加していません。

- 外部GGUFと本体のアーキテクチャ、PLE設定、ハッシュ乗数、各headの範囲、特殊トークンID、テンソル寸法を照合します。
- 永続化する継続キャッシュの識別に、外部PLEのフィンガープリントを加えました。
- オプションを指定しなければ、従来の内蔵PLEを使います。
- 外部PLEを選んだ場合は、読み込みログに `event=ple_sidecar type=BF16` とファイルパスが出ます。
- ソース・コメント・プロジェクト内の説明は英語。変更ファイルに個人用の固定パスはありません。

## 取得ファイル

保存先: `/home/user/Models/PLE/Qwen3.8-Flash-Next-PLE-BF16.gguf`

配布元: [DelgonのBF16 PLE（固定リビジョン）](https://huggingface.co/Delgon/Qwen3.8-Flash-Next-PLE-GGUF/resolve/5de86c89b5b78fc2711744475b4da50ddd9acb72/Qwen3.8-Flash-Next-PLE-BF16.gguf)

サイズ: **102,400,492,480 bytes / 約95.37 GiB**。公開SHA-256との全体照合に成功しました。

```text
0ac215cf0c5767ca20c463ad467eb31341e0ce0ca6cc8eb4f934d4b5f6d1932d
```

元のHuihui分割GGUFは引き続き必要です。内蔵PLEを含む分割ファイルにも本体の重みがあるため、削除していません。この外部ファイル分が追加のディスク使用量になります。

## 確認結果

| 確認項目 | 結果 |
| --- | --- |
| 既存の書式チェック | 695 C++ファイル、成功 |
| BF16 sidecarのCPUテスト | 読み取り、重複行、マッピング解放後の読み取りに成功 |
| 不一致ファイルのCPUテスト | 各メタデータの欠落・不一致、寸法違い、未対応形式を拒否 |
| 既存のモデル設定・GGUF読み取りテスト | 成功 |
| 実ファイルのヘッダー | 必要な11項目が型・値とも一致 |
| ROCm 10.1向けビルド | 成功。追加項目による初期化警告は解消 |
| 新DockerイメージのCLI | `--help` と `serve llm --help` 成功。GPU・モデルなしで実行 |
| ダウンロードの全体ハッシュ | 公開SHA-256と一致 |
| 実モデルの最小確認 | 短文と256トークンの反復入力、計2件ともHTTP 200で完了。MTP採用あり |
| 本格的な速度・品質・長文・ピークメモリ評価 | 未実施。最小限にするという指示に従い、試験を追加していません |

ビルド全体には警告があり、ログに保存しています。ビルドエラーはありません。

## 現在起動している構成

- イメージ: `gufo:v0.27.0-ud-iq4xs-ple-bf16-rocm101-20261031`
- モデル名: `Qwen3.8-FN-UD-IQ4_XS`（維持）
- ROCm 10.1、`AMD_SERIALIZE_COPY=2`（維持）
- 2セッション、各131,072トークン、RAMキャッシュ4 GiB、MTPとmmprojは通常設定を維持。
- 追加引数: `--ple-model /models/PLE/Qwen3.8-Flash-Next-PLE-BF16.gguf`

ユーザーの許可を受けて新しいGufoコンテナへ切り替え、通常設定ファイルにも反映しました。旧イメージと旧コンテナ `Gufo-pre-ple-bf16-20261031` は停止状態で残しています。起動ログで外部PLEのBF16選択を確認済みです。

## 実ファイルと記録

- 実装したソース（ローカル保管：../../../..）
- レビュー用の全差分（ローカル保管：../../evidence/deployments/bf16-ple-v0270-20261031/checks/implementation.patch）
- 変更ファイルのSHA-256一覧（ローカル保管：../../evidence/deployments/bf16-ple-v0270-20261031/checks/source-manifest.json）
- 適用した起動引数（ローカル保管：../../evidence/deployments/bf16-ple-v0270-20261031/normal/docker-create.json）
- 検証結果JSON（ローカル保管：../../evidence/deployments/bf16-ple-v0270-20261031/checks/validation.json）
- 最終ビルドログ（ローカル保管：../../evidence/deployments/bf16-ple-v0270-20261031/logs/build-final-defaults.log）

ソースの基点は `2a6beddcca7160406694a52517d5fdbf9abbae93`、作業ブランチは `feat/qwen38-flash-next-ple-model`。変更は独立した作業ツリーに保持しています。

実行ファイルSHA-256: `5699a8c70fed2b2c36ab8733100168ae71069a057ea08ada6a87ffd8dd9d43db`

## 最小限の実モデル確認

| 確認 | 入力トークン | 生成トークン | MTP採用／提案 | 結果 |
| --- | ---: | ---: | ---: | --- |
| 短い日本語応答 | 33 | 13 | 9 / 17 | 指定した確認文を返して完了 |
| 小規模な反復入力 | 256 | 4 | 2 / 4 | 「反復確認OK」を返して完了 |

テストの2件だけ `reasoning_effort=none` とし、通常設定の `--think auto` は維持しています。確認後のhealthも正常でした。追加の推論試験は行っていません。

## メモリの記録

| 観測対象 | 値 | 範囲 |
| --- | ---: | --- |
| Windows側の空き物理メモリ・起動前 | 14.83 GiB | 起動直前の単発測定 |
| Windows側の空き物理メモリ・確認後 | 5.03 GiB | 確認後の単発測定 |
| WSL側の利用可能メモリ・観測最小 | 16.82 GiB | 起動・短文確認中に採取した9サンプルの範囲 |
| GufoプロセスRSS・起動完了時 | 4.05 GiB | GPU割り当て全体を含む値ではない |
| ROCmのデバイス使用量・起動完了時 | 77.27 GiB | 共有領域も含む。専用VRAMだけの使用量ではない |

WindowsとWSLの空き容量は観測範囲が異なるため足し合わせません。Windowsのピーク時空き容量は測っていません。今回の少量の入力では、長文利用時の最大使用量や速度差は判断できません。

[最小確認の詳細](../../../../evidence/local/evidence/deployments/bf16-ple-v0270-20261031/normal/summary.json) · サーバーログ（ローカル保管：../../evidence/deployments/bf16-ple-v0270-20261031/normal/server.log）


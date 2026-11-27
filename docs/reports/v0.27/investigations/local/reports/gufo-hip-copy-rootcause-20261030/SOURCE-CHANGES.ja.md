# 稼働中Gufoに対応する実ファイルの変更箇所

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

確認日：2026-10-30（日本時間）。原因調査は中断中。以下の確認・記録ではGufoを停止せず、本体ソースや通常設定も編集していない。

## 比較対象

- 基準：公式Gufo v0.27.0、`de15fdb91f6992f09a2514dabd6d6a8e5f8a2efc`。
- 対象：最小UD-IQ4_XS対応、`2a6beddcca7160406694a52517d5fdbf9abbae93`。
- 実際のソース：`/home/user/Apps/Gufo/deployments/ud-iq4xs-minimal-v0270-20261030-165036/source`。
- 稼働イメージ：`gufo:v0.27.0-ud-iq4xs-minimal-2a6beddc-rocm101`。Dockerの読み取りで一致を確認。
- このソースツリーの未コミット差分はなし。

**本体は8ファイル、106行追加・9行削除。テスト関連5ファイルを含めた全体は13ファイル、364行追加・9行削除＋新規バイナリfixture。**

ここで示すのは、編集用Pythonスクリプトや診断用interposerではなく、実際のビルド対象ソースと運用設定の変更箇所。行番号は上記コミット時点のファイルに対応する。

## 本体ソースの8ファイル

表のパスは `src/models/qwen38_flash_next/` からの相対表記。リンクは実ファイルの絶対パス。

| 実ファイル | 変更行の開始位置 | 変更内容 |
|---|---|---|
| weights.cpp（ローカル保管：/home/user/Apps/Gufo/deployments/ud-iq4xs-minimal-v0270-20261030-165036/source/src/models/qwen38_flash_next/weights.cpp:30） | 30・34・147行 | IQ3_S / IQ4_XSのブロック構造を登録し、IQ3_S / IQ4_NL / IQ4_XSをエキスパート重みの受け入れ対象に追加。 |
| cpu_ops.cpp（ローカル保管：/home/user/Apps/Gufo/deployments/ud-iq4xs-minimal-v0270-20261030-165036/source/src/models/qwen38_flash_next/cpu_ops.cpp:69） | 69・75行 | CPU参照側でIQ3_S / IQ4_XSを既存の逆量子化関数へ接続。IQ4_NLの接続は元から存在。 |
| kernels/rocm/mmq/qfn_mmq.h（ローカル保管：/home/user/Apps/Gufo/deployments/ud-iq4xs-minimal-v0270-20261030-165036/source/src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.h:79） | 34・79行 | 既存の命名に合わせ、3形式のprefill用エキスパート演算の呼び出し口を宣言。対応形式のコメントを更新。 |
| kernels/rocm/mmq/qfn_mmq.hip.cpp（ローカル保管：/home/user/Apps/Gufo/deployments/ud-iq4xs-minimal-v0270-20261030-165036/source/src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.hip.cpp:543） | 362・543・754行 | 3形式を受け入れ、既存qfn_mmq_moe_implへ渡す薄い関数を追加。既存行列演算のテンプレートを各形式で実体化。 |
| kernels/rocm/mmq/mmvq.hip.cpp（ローカル保管：/home/user/Apps/Gufo/deployments/ud-iq4xs-minimal-v0270-20261030-165036/source/src/models/qwen38_flash_next/kernels/rocm/mmq/mmvq.hip.cpp:842） | 235・248・842行 | 生成時などの少数トークン処理で、3形式を既存の内積関数・演算設定・エキスパート実行へ振り分け。 |
| kernels/rocm/executor.cpp（ローカル保管：/home/user/Apps/Gufo/deployments/ud-iq4xs-minimal-v0270-20261030-165036/source/src/models/qwen38_flash_next/kernels/rocm/executor.cpp:1883） | 23・1690・1883行 | エキスパート実行の分岐に3形式を追加。Q6_Kの出力投影は既存の共通Qwen GEMMへ接続し、そのヘッダーを追加。 |
| kernels/rocm/device_model.cpp（ローカル保管：/home/user/Apps/Gufo/deployments/ud-iq4xs-minimal-v0270-20261030-165036/source/src/models/qwen38_flash_next/kernels/rocm/device_model.cpp:422） | 409・422行 | Q6_K出力ヘッドのアップロード拒否を解除。Q6_Kのembedding・エキスパートは引き続き拒否。 |
| kernels/rocm/batch.cpp（ローカル保管：/home/user/Apps/Gufo/deployments/ud-iq4xs-minimal-v0270-20261030-165036/source/src/models/qwen38_flash_next/kernels/rocm/batch.cpp:682） | 682行 | Q6_Kを含むバッチ出力処理も、上記の対応済みDense経路へ流す。 |

本体8ファイルの実際のGit差分（ローカル保管：gufo-v0270-production-source.patch）

実装は、既存のIQ計算と共通Q6_K計算を呼べるように接続するもの。今回の最小対応で、IQの内積数学やゼロ初期化処理そのものを書き換えた差分はない。ただし、それによって新たなモデル・内部形式の組み合わせが実行されるため、接続の正しさとモデル全体の動作検証は別途必要。

## テスト関連の5ファイル

以下は上流向けのソースに含めた検証用ファイル。今回の停止原因調査で一時的に作った診断プログラムとは別。

| 実ファイル | 変更箇所・規模 | 内容 |
|---|---|---|
| CMakeLists.txt（ローカル保管：/home/user/Apps/Gufo/deployments/ud-iq4xs-minimal-v0270-20261030-165036/source/tests/models/qwen38_flash_next/CMakeLists.txt:18） | 18・36行 | CPUの行読み取り試験とGPUのIQエキスパート試験を登録。 |
| iq_rows_test.cpp（ローカル保管：/home/user/Apps/Gufo/deployments/ud-iq4xs-minimal-v0270-20261030-165036/source/tests/models/qwen38_flash_next/iq_rows_test.cpp:1） | 新規・68行 | CPUの逆量子化、行サイズ、エキスパート間のstrideを独立参照データと照合。 |
| iq_expert_ops_test.cpp（ローカル保管：/home/user/Apps/Gufo/deployments/ud-iq4xs-minimal-v0270-20261030-165036/source/tests/models/qwen38_flash_next/iq_expert_ops_test.cpp:1） | 新規・143行 | 生成用・prefill用のIQ演算出力を独立CPU参照に基づく内積と照合。 |
| fixtures/iq_expert.json（ローカル保管：/home/user/Apps/Gufo/deployments/ud-iq4xs-minimal-v0270-20261030-165036/source/tests/models/qwen38_flash_next/fixtures/iq_expert.json:1） | 新規・45行 | 参照元の固定リビジョン、形式、寸法、ハッシュとデータ由来を記録。 |
| [fixtures/iq_expert.bin](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/fixtures/iq_expert.bin) | 新規・155,256 bytes | 上記試験で使う量子化データと独立CPU参照値。実モデルの重みファイルではない。 |

## 停止回避フラグの実際の設定箇所

`AMD_SERIALIZE_COPY=2` は本体C++への追加ではなく、Dockerコンテナの環境変数として適用している。

| 実ファイル | 該当箇所 | 役割 |
|---|---|---|
| docker-create.json（ローカル保管：/home/user/Apps/Gufo/deployments/ud-iq4xs-minimal-v0270-20261030-165036/normal/docker-create.json:47） | 47行 | コンテナ作成引数の `--env AMD_SERIALIZE_COPY=2` |
| normal-settings.json（ローカル保管：/home/user/Apps/Gufo/normal-settings.json:106） | 106行 | 通常運用の `runtime_environment` に値 `2` を記録 |
| normal-settings-huihui.json（ローカル保管：/home/user/Apps/Gufo/normal-settings-huihui.json:106） | 106行 | Huihui通常設定の同内容の記録 |

稼働中コンテナ自体にも `AMD_SERIALIZE_COPY=2` があることを確認した。診断用の `LD_PRELOAD` は設定されていない。GPUのコピー・初期化完了を待つ回避策であり、問題の計算を省略するものではない。

## 原因調査で調べたが、この最小対応では変更していない箇所

- `mmid.hip.cpp`：エキスパートIDマップの生成と共有の補助領域。
- `qfn_ggml_stubs.hip.cpp`：ストリーム別の一時領域プール。
- `qfn_mmq.hip.cpp`の既存初期化部分：一時配列への `hipMemsetAsync`。このファイルには上記の接続差分はあるが、この初期化部分には今回の変更なし。
- `executor.cpp`の既存チェックポイント読み戻し・ストリーム同期部分。このファイルには上記の形式分岐差分はあるが、停止回避の待機を本体に固定してはいない。

根本原因調査の診断コードは別ディレクトリに保存され、本番Gufoへ組み込まれていない。

## 古いソースパスとの取り違えに注意

`normal-settings.json`の説明用 `confirmed_source_directory` には、以前の `v0270-update-20261030-151123/source` が残っている。その作業ツリーのHEADは `01497fcd` で、今回の最小対応の `2a6beddc` と異なる。

この文書は、`ud-iq4xs-minimal-v0270-20261030-165036/source` を直接確認した差分。古い説明用パスの修正は今回行っていない。

[調査全体のまとめへ戻る](SUMMARY.ja.md)

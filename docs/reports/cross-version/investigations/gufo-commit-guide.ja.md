# Gufo：速度改善・モデル対応・v0.30.0統合のコミット別解説

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

> 保存先の整理（2026-11-01）：開発用ソースは `/home/user/workspace/Gufo` に集約。このチェックアウトはASR対応 `4336a8d5` を含む。稼働中のLLMイメージは本文記載の `8f4a7e77`、ASRイメージは `4336a8d5` のまま。以下の測定値・コミット記録は変更していない。

作成日：2026-11-01（日本時間）
2026-11-02：第1.1節にプログラム改修の基本方針を追記。
2026-11-06：第12節にv0.30.0と既存カスタムの比較・適用した変更を追記。第11節以前は当時の記録として読む。
2026-11-06：第14節に未加工Unsloth v3.1用のQ5_0対応と実機試験結果を追記。試験イメージは作成済み、通常運用はc1170a4a＋Huihui・外部Q8 PLEを維持。

v0.28.1の取り込み・Attention境界修正・通常採用を同日追記。第3～9節はv0.27.0採用時の記録、第10節がLLM本体の更新。第11節に日本語ASR専用イメージの対応を追記した。

旧版で採用した速度改善6件と、Huihui・Orca・ISTA-DASLab GSQ-RCO／SC117・Qwen3-ASR-1.7B-JAの対応を、同じ4項目で記録する。

1. 意図
2. 変更したファイル
3. 変更内容
4. 確認できた結果

各項目のファイル表は主要な変更と役割を説明し、付録にはGitから取得した**各コミットの全変更ファイル**を載せる。共通部品、モデルへの接続、速度改善、検証を分けることで、後から変更の目的と範囲を追えるようにする。

## 1. 対象のソースと読み方

- 拡張の当初の基点：上流Gufo v0.27.0、`de15fdb9`。現在は公式v0.28.1、`12effe36`を統合済み。
- Huihuiの通常イメージで採用している先頭コミット：`8f4a7e77f91c466bfe5fbf0703385c9a17b6f4c2`。
- ブランチ：`fix/qwen38-attention-boundary-20261101-163558`。
- ソース：採用中ソース（ローカル保管：..）。
- 通常イメージ：`gufo:v0.28.1-attention-8f4a7e77-rocm101`。
- 対象となるローカルの履歴：LLM本体の既存14コミット＋公式統合1件＋独立修正1件。ASR別ブランチの対応1件を第11節に記載。

この文書でいうOrcaは、元のQwen系GGUFへのOrca対応を指す。ISTA-DASLab／SC117は、ISTA-DASLabのGSQ-RCO量子化への共通対応をSC117にも適用する区分として扱う。実装は配布者名で分岐せず、重みの形式・形状を基準にする。

Huihui対応も、Huihui専用の名前分岐ではない。Unsloth UD-IQ4_XS系の内部形式を接続し、Huihui v2を実際の検証モデルに使った。Unsloth側の別配布GGUFそのものを今回実行したという意味ではない。

「結果」は、設計上省ける処理、保存された演算試験、モデルの読み込み確認、モデル全体の実測を区別して書く。各コミット単体の速度差は今回測定していない。後掲の速度は全変更を組み込んだ構成の値で、各コミットの寄与率には分解できない。記載した試験は2026-10-30～2026-11-01の保存済み記録であり、この文書作成のための新たなベンチではない。

用語の目安：Prefillは入力を処理する段階、Decodeは回答を生成する段階。WMMAはGPUの行列演算機能、GDNは線形Attention内部、HCは複数の内部データを混ぜ合わせる部分を指す。

### 1.1 プログラム改修の基本方針

2026-11-02追記。これまでの依頼で取り決めた、Gufoのカスタム改修を進める際の方針をまとめる。Gufo公式の規約そのものではなく、この環境での作業方針である。実際の改修では、対象箇所の最新のソース・作業規約と、その作業に対するユーザーの指示を確認する。

基本は、**既存コードになじむ書き方で、足りない処理だけを追加し、既存の処理へ接続すること**。将来の上流更新やPRでも、変更の理由と範囲を追える差分にする。

#### 既存コードの命名と書き方に合わせる

- ファイル名、ディレクトリ構成、関数名、変数名、型名、列挙値は、同じ役割を持つ既存実装に倣う。独自の命名規則を持ち込まず、近くの対応済みモデルや量子化形式の実装を手本にする。
- 関数の分け方、引数・戻り値、エラー処理、ログ、コメント、CMakeへの登録、テストの配置も周囲に合わせる。変更と無関係な改名・整形・リファクタリングを混ぜない。追加する名前は、配布者名や作業時の仮称よりも、処理の役割や扱う形式を表すものにする。

#### 既存処理を優先し、追加を必要な範囲に絞る

- まず、既存の読み込み・変換・演算・高速化の経路で使えるものを確認する。そのまま使える処理は極力無改造で呼び出し、同じ機能を別実装として複製しない。
- 未対応部分だけを実装し、必要な変換や振り分けを経て既存経路へ接続する。GPUの作業領域、転送、演算の起動方法なども既存の仕組みを再利用し、不要なコピー、メモリ確保、重み全体の展開を増やさない。
- 将来の形式追加は、既存の型・分岐・共通入口へ処理と検証を追加できる形にする。まだ必要性が分からない機能のために、大きな抽象化や設定項目を先回りで増やさない。

#### 共通部品とモデルへの接続を分ける

- 第1段階は、未対応の内部形式を扱う共通部品。第2段階は、その部品をモデルの重み定義・読み込み条件・実行経路へ接続する処理とする。形式を扱う部品に、特定の配布モデルだけの条件を混ぜない。
- 公開モデル名の「IQ4_XS」「IQ3_S」だけで内部形式を決めつけず、実際のテンソルの形式・形状・メタデータを確認する。モデル内に複数の量子化形式が混在する場合も、それぞれを既存または共通の処理へつなぐ。
- HuihuiやSC117という配布者名・ファイル名だけで分岐せず、必要な形式と構造に基づいて対応する。モデル固有の処理が必要なら、その根拠と適用条件を明示する。ISTA対応をPRにする場合はISTA-DASLab GSQ-RCO対応を主題とし、SC117での動作確認は検証結果として区別する。

#### 本体の変更と環境設定を区別する

- ソース、コメント、ログ文言、上流提出用の説明は英語で記述する。個人のユーザー名、ホームディレクトリ、ローカルモデルの保存先を共通コードへ埋め込まず、既存の引数・設定・パス解決を使う。日本語の作業台帳や個人環境の保存先は、ローカル文書・運用設定で扱う。
- Gufo本体、量子化拡張、モデルの重み、ROCm・WSL・Docker設定のどこに原因があるかを切り分ける。停止を回避する環境変数やフラグを見つけても、根本原因を修正したとは扱わない。診断専用のコードやログ出力は通常版へ残さない。
- 上流更新時は、公式の対応・修正とカスタム部分の重複を確認する。公式経路を使える場合は重複実装を整理し、残す独自処理には必要な理由を示す。競合は双方の処理の役割を確認して統合し、対応モデルや既存の動作を不用意に失わないようにする。

#### 変更の目的ごとにコミットし、台帳へ記録する

- 共通部品、モデル対応、速度改善、不具合修正、上流取り込みなど、目的の異なる変更は独立したコミットにする。一つの意味のある改修を一単位とし、後から確認・移植・取り消しができるようにする。必要なテストは対応する変更と関連づけ、無関係な差分を混ぜない。
- コミット名も既存プロジェクトの形式に合わせ、英語で目的を示す。PRには対象の機能に必要な変更と検証をまとめ、個人設定、一時ファイル、日本語のローカル台帳を混ぜない。
- コミット完了後、この台帳へ「意図」「変更したファイル」「変更内容」「確認できた結果」の同じ項目で追記する。性能への寄与を単独で測っていない場合は、その旨を明記する。

#### 検証と作業の進め方

- 変更箇所に合った最小限の確認から始める。読み込み定義、演算の独立した参照値、形式・形状の境界、既存経路への影響、対象モデルの動作を、必要な範囲で確認する。速度改善では、速度だけでなく数値精度・メモリ使用量・既存機能への影響も確認する。
- 「形式上対応できる」「実際に読み込めた」「推論を確認した」「正式ベンチを完了した」を分けて記録する。Huihuiでの確認を、別配布のUnslothモデルも実測済みという意味にしない。正式ベンチの回数、PLEあり／なし、メモリ記録、通常設定への復帰はベンチの基本方針（ローカル保管：BENCHMARK-POLICY.md）に従い、個別に指定された検証範囲を優先する。
- 調査・方針検討だけを依頼された段階では実装へ進まない。実装が許可された後は承認済みの範囲で進め、作業の区切りごとに途中経過を報告する。稼働中のモデルや起動設定の変更も、依頼された範囲を守る。確認の失敗・未実施事項は、その時点で報告する。
- ソースの編集はworkspace内のGitリポジトリで行い、恒久的な文書は `docs`、ベンチ記録は `docs/benchmark` に保存する。Appsは実行・運用用とする。一時ファイルや重複した元フォルダは、必要な変更・記録を保存して照合した後に削除する。

## 2. 実際のコミット順

| 順番 | コミット | 分類 | 内容 |
| --- | --- | --- | --- |
| 1 | `386dec30` | Huihui対応 | 第1段階：既存IQ演算を共通入口から呼べるようにする |
| 2 | `eb4409d6` | Huihui対応 | 第2段階：UD-IQ4_XSの重み・CPU参照・実行経路を接続 |
| 3 | `2a6beddc` | Huihui対応 | 第2段階の補完：Q6_Kの出力層を既存Qwen演算へ接続 |
| 4 | `2b1143b6` | 共通部品・PLE・検証 | 外部BF16 n-gramを指定する既存実装を独立保存 |
| 5 | `59753e6a` | 共通部品・PLE・検証 | IQ2_S・IQ3_XXS・Q2_0の共通演算を復元 |
| 6 | `8e02a42a` | 速度改善 | 小さい量子化行列の計算をまとめて実行 |
| 7 | `7430e160` | 速度改善 | IQエキスパートを既存のWMMA Prefillへ接続 |
| 8 | `39ca0879` | 速度改善 | GDNのFP16出力を次の投影で再利用 |
| 9 | `86434907` | 速度改善 | HCのFP32結果と再利用用FP16を同時生成 |
| 10 | `58516f7d` | 速度改善 | 共有エキスパートのSwiGLUとFP16変換を融合（旧Aの採用版） |
| 11 | `a236cb30` | 速度改善 | BF16のHC・PLEを計算順序が一定のWMMAへ接続 |
| 12 | `5b8da407` | 共通部品・PLE・検証 | 圧縮IQ演算と高速経路を独立参照で検証 |
| 13 | `095a6266` | Orca対応 | 元Qwen系のOrca IQ4_XSを共通経路へ接続 |
| 14 | `fbf0a9f8` | ISTA-DASLab／SC117対応 | ISTA GSQ-RCOの混在形式と浮動小数点重みの差へ対応 |
| 15 | `0090201a` | 公式統合 | v0.28.1を既存のモデル拡張・速度改善へマージ |
| 16 | `8f4a7e77` | 不具合修正 | 狭いバッチのAttentionを密／疎境界で単独処理と一致させる |
| 17（ASR別ブランチ） | `4336a8d5` | 日本語ASR対応 | 単一safetensors・共有出力重みを既存ASRへ接続 |

## 3. 旧版から移植した速度改善

### `8e02a42a` — 小さい量子化行列の計算をまとめて実行

コミット名：`perf(qwen38-flash-next): batch small packed projections with exact FP32 dots`

#### 意図

HCや制御用の小さな投影でGPU起動を細かく繰り返す負担を減らし、大きな投影には既存の行列演算経路を使う。小さい行列ではFP32の積和順序を維持する。

#### 変更したファイル

| 主なファイル | 編集内容 |
| --- | --- |
| [src/models/qwen38_flash_next/kernels/rocm/native_quant.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/native_quant.cpp) | 新設。NativeIq4Dense / NativeIq4DenseF16で形式・形状・Prefillかどうかに応じて既存演算へ振り分ける。 |
| [src/models/qwen38_flash_next/kernels/rocm/native_quant.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/native_quant.hpp) | 対象形式と入力・作業領域の契約を定義する。 |
| src/models/qwen/hip/kernels/prefill_quant_gemm.hip（ローカル保管：../src/models/qwen/hip/kernels/prefill_quant_gemm.hip） | LaunchGroupedQuantGEMMFp32を追加する。 |
| src/models/qwen/hip/kernels/small_batch_gemm.hpp（ローカル保管：../src/models/qwen/hip/kernels/small_batch_gemm.hpp） | GroupedGridを追加し、既存の8トークン演算を複数グループで起動できるようにする。 |
| src/models/qwen/hip/ops/gemm.hpp（ローカル保管：../src/models/qwen/hip/ops/gemm.hpp） | 共通ランチャーを宣言する。 |
| [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) | Denseから新しい振り分けへ接続し、既存のFP16入力再利用を使う。 |
| [src/models/qwen38_flash_next/kernels/rocm/batch.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/batch.cpp) | DenseBatchでも同じ共通部品へ接続する。 |
| src/models/qwen38_flash_next/CMakeLists.txt（ローカル保管：../src/models/qwen38_flash_next/CMakeLists.txt） | native_quant.cppをビルド対象へ登録する。 |

#### 変更内容

- 対象のIQ4_XS・Q5_K小行列では8トークン単位のグループをまとめて起動する。端数は既存の処理へ渡す。
- 出力幅512以下の小さいPrefill投影はFP32の積和を維持する。より大きい対応投影は既存のFP16入力・WMMA経路へつなぐ。
- 重みの保存形式を変える処理ではなく、計算の呼び分けとまとめ方を変更する。

#### 確認できた結果

- 最終統合版のnative_iq4_opsで、IQ4_XS・IQ4_NL・Q4_K・Q5_K・Q6_Kの対象投影、端数行、割り当て境界の試験が合格した。
- 小行列のFP32精度を保つ経路と、大行列のFP16/WMMA経路を分けて利用できる状態になった。
- このコミット単体のPrefill・TTFT・Decodeの改善率は今回測定していない。

根拠：native IQ演算の検証ログ（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/logs/gpu-native_iq4_ops.log）。

### `7430e160` — IQエキスパートを既存のWMMA Prefillへ接続

コミット名：`perf(qwen38-flash-next): reuse routed WMMA prefill for IQ experts`

#### 意図

既存のQ4系で使われている入力振り分け・行列演算をIQ形式にも使い、長い入力のPrefillをまとめて処理できるようにする。

#### 変更したファイル

| 主なファイル | 編集内容 |
| --- | --- |
| [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) | WmmaExpertsの適用条件をIQ形式へ拡張し、MoeExpertsから既存の入力準備と型別WMMAへつなぐ。 |
| [src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp) | 既存RoutedF16GEMMKernelにIQ形式の重み読み出し・展開と型別呼び出しを追加する。 |
| [src/models/qwen38_flash_next/kernels/rocm/kernels.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hpp) | 必要なWeightTypeを追加する。 |
| [src/models/qwen38_flash_next/kernels/rocm/iq_prefill.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/iq_prefill.hpp) | 新設。IQ2_S・IQ3_XXS・IQ3_S・Q2_0の圧縮ブロックを必要な単位で読み出し、演算用FP16へ展開する。 |
| src/models/qwen/hip/kernels/iq4_half.hpp（ローカル保管：../src/models/qwen/hip/kernels/iq4_half.hpp） | IQ4のコード表からFP16重みを作る既存処理を共通部品として切り出す。 |
| src/models/qwen/hip/kernels/prefill_fp16.hip（ローカル保管：../src/models/qwen/hip/kernels/prefill_fp16.hip） | 切り出したIQ4展開処理を共有する。 |

#### 変更内容

- 同じエキスパートを使う入力をまとめ、必要な量子化ブロックを展開してWMMAで計算する。重み全体の事前FP16展開は行わない。
- gate/up側のIQ4_XS・IQ2_S・IQ3_XXS・IQ3_S、down側のIQ4_NL・Q2_0を既存経路へ接続する。
- 入力振り分け、GPU内の作業領域、WMMAの積和、出力処理は既存実装を共有する。形式固有の部分はコード表・スケール・重み展開に集約する。
- Prefill以外のバッチ検証は既存MMQ経路を維持する。FP16展開ではスケールを先にFP32で計算し、不要な追加丸めを避ける。

#### 確認できた結果

- 最終統合版のrouted_wmma_opsが合格。独立した参照計算、MMQとの比較、行数の端数、ルーティングを確認した。
- Huihuiにも含まれるIQ3_Sなどのエキスパートが、形式・形状の条件を満たすPrefillで既存WMMA経路を利用できるようになった。
- WMMAと従来MMQは演算精度の扱いが異なる部分があり、全出力のビット一致を一律に保証した変更ではない。独立参照に対する誤差を検証した。
- コミット単体の速度差は未測定。統合後の速度は後掲する。

根拠：routed WMMA検証ログ（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/logs/gpu-routed_wmma_ops.log）。

### `39ca0879` — GDNのFP16出力を次の投影で再利用

コミット名：`perf(qwen38-flash-next): reuse GDN half output for IQ4 projections`

#### 意図

GDN（線形Attention内部）の出力を、FP32として保存してからFP16へ変換し直す処理を省く。

#### 変更したファイル

| 主なファイル | 編集内容 |
| --- | --- |
| [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) | LinearAttentionで既存GDNのFP16出力をIQ4_XS出力投影へ直接渡す。 |
| [tests/models/qwen38_flash_next/gdn_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/gdn_ops_test.cpp) | 同じ出力領域をFP16用に使う場合の丸め、状態、未使用領域を検証する。 |

#### 変更内容

- 対象はPrefill中のIQ4_XS出力投影で、形状2560×6144、許容バッチ幅などの条件を満たす場合に限定する。
- 従来の「FP32出力→保存→再読込→FP16変換」を、既存機能による「FP16出力→次の投影」へ接続する。
- 既存のFP32作業領域をFP16出力用に利用し、GDNの計算そのものを別実装へ置き換えない。

#### 確認できた結果

- gdn_opsが合格。FP16出力のバイト一致、内部状態の維持、作業領域の未使用部分に書き込まないことを確認した。
- 対象経路では、独立したFP16変換と中間データの読み書きを省ける。
- IQ4_XSの対象投影に限定した最適化で、全モデル・全層に一律適用する処理ではない。単体の速度差は未測定。

根拠：GDN検証ログ（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/logs/gpu-gdn_ops.log）。

### `86434907` — HCのFP32結果と再利用用FP16を同時生成

コミット名：`perf(qwen38-flash-next): emit reusable half rows from HC mixing`

#### 意図

HCで4本の内部データを混ぜた結果を後続の投影がFP16で使う際、別の変換処理と読み直しを省く。

#### 変更したファイル

| 主なファイル | 編集内容 |
| --- | --- |
| [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) | HcMixで対象IQ4経路を選び、後続計算が使うFP16入力の再利用情報を更新する。 |
| [src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp) | HcMixEpilogueVec4KernelにFP32出力とFP16コピーの同時出力を追加する。 |
| [src/models/qwen38_flash_next/kernels/rocm/kernels.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hpp) | FP16コピーも受け取るオーバーロードを追加する。 |
| [tests/models/qwen38_flash_next/hc_mix_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/hc_mix_ops_test.cpp) | FP32結果、FP16コピー、inject部分の一致と独立CPU参照を検証する。 |

#### 変更内容

- Orca系の対象IQ4_XS down/inject、4ストリーム・hidden2560などの条件を満たす場合に使う。
- FP32の結果を残しながら、その場でFP16コピーも作る。
- 途中の投影呼び出しが入力再利用情報を無効化するため、正しいFP16コピーが残っていることを呼び出し後に登録する。

#### 確認できた結果

- hc_mix_opsが合格。1・8・9・33・2049トークンでFP32出力とFP16コピーが従来処理とビット一致した。
- 独立CPU計算との確認も通過し、対象経路の後続変換を省けるようになった。
- この変更単体の速度差は未測定。

根拠：HC検証ログ（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/logs/gpu-hc_mix_ops.log）。

### `58516f7d` — 共有エキスパートのSwiGLUとFP16変換を融合（旧Aの採用版）

コミット名：`perf(qwen38-flash-next): fuse IQ4 shared expert half staging`

#### 意図

共有エキスパートの活性化処理と次の投影用のFP16変換をまとめ、中間FP32配列の保存・再読込を減らす。

#### 変更したファイル

| 主なファイル | 編集内容 |
| --- | --- |
| [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) | GatedDenseとMoeでIQ4_NL共有エキスパートのFP16作業バッファを直接利用する。 |
| [src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp) | SwigluValueとSwigluHalfに、従来のFP32丸め境界を保つ経路を追加する。 |
| [src/models/qwen38_flash_next/kernels/rocm/kernels.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hpp) | 丸め境界を維持するSwigluHalfのオーバーロードを追加する。 |
| [tests/models/qwen38_flash_next/swiglu_half_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/swiglu_half_test.cpp) | 新設。丸め境界、符号付きゼロ、微小値、端数を含む一致検証を行う。 |
| [tests/models/qwen38_flash_next/CMakeLists.txt](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/CMakeLists.txt) | swiglu_half試験を登録する。 |

#### 変更内容

- SwiGLU計算後のFP32丸めを維持しつつ、保存する最終出力はFP16にする。
- 単純な融合でコンパイラーが積を組み替え、FP16の丸め結果が変わることを防ぐ。
- 共有エキスパート用の専用FP16バッファを使い、ルーティングされた他のエキスパートが使う入力を上書きしない。
- 従来からの呼び出しは元のオーバーロードを使い、対象IQ4経路に必要な処理だけ追加する。

#### 確認できた結果

- 1,556,811点の試験で、既存の単純融合経路は14点で従来の二段階処理と差が出た。採用したFP32境界維持経路は差が0点だった。
- native_iq4_opsでは、共有エキスパートのFP16入力だけでなく、その後の投影出力も従来処理と一致した。
- これは旧版で採用したAの内容に当たる。旧実験のB1・B2・Cは今回の移植対象に含めていない。
- このコミット単体の速度差は未測定。

根拠：SwiGLU一致検証（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/logs/gpu-swiglu_half.log）、後続投影の検証（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/logs/gpu-native_iq4_ops.log）。

### `a236cb30` — BF16のHC・PLEを計算順序が一定のWMMAへ接続

コミット名：`perf(qwen38-flash-next): reuse fixed-order WMMA for BF16 HC and PLE`

#### 意図

入力をまとめて処理した場合と、キャッシュ境界などで分割した場合の丸め差を避けながら、既存のWMMAを使う。

#### 変更したファイル

| 主なファイル | 編集内容 |
| --- | --- |
| [src/models/qwen38_flash_next/kernels/rocm/blaslt.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/blaslt.cpp) | HC・PLEの対象形状についてUnquantizedBf16Gemmへ振り分ける。 |
| [src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp) | BF16の重み・入力を保ったまま、既存の行列カーネルを固定順序で使う入口を追加する。 |
| [src/models/qwen38_flash_next/kernels/rocm/kernels.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hpp) | UnquantizedBf16Gemmを宣言する。 |
| [tests/models/qwen38_flash_next/blaslt_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/blaslt_ops_test.cpp) | 5種類の形状、7種類の分割条件、FP64参照を検証する。 |

#### 変更内容

- 足し合わせるK方向の順序を一定にし、同じトークンの結果がバッチ内の位置で変わらないようにする。
- 対象形状は出力×入力で320×10240、4×10240、10240×320、10240×2560、2560×2560。
- 対象はモデル内部のBF16行列演算。外部BF16 n-gramファイルの選択機能は2b1143b6に分離している。

#### 確認できた結果

- 5形状それぞれで7つの分割条件を通過し、一括実行と分割実行の結果が一致した。
- 独立FP64参照との差は試験の許容値0.002未満。記録された最大絶対誤差の最大は0.001661だった。
- 個別の速度寄与や、モデル全体の回答一致をこの試験だけで測ったものではない。

根拠：BF16行列検証ログ（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/logs/gpu-blaslt_ops.log）。

## 4. Huihui対応：Unsloth UD-IQ4_XS系の最小接続

### `386dec30` — 第1段階：既存IQ演算を共通入口から呼べるようにする

コミット名：`feat(qwen38-flash-next): expose existing IQ expert kernels`

#### 意図

Huihuiで使われているUnsloth UD-IQ4_XSの混在形式を処理するため、既存のIQ演算をFlash-Nextから利用できる共通部品として公開する。

#### 変更したファイル

| 主なファイル | 編集内容 |
| --- | --- |
| [src/models/qwen38_flash_next/kernels/rocm/mmq/mmvq.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/mmvq.hip.cpp) | IQ3_S・IQ4_NL・IQ4_XSのベクトル内積とMoEベクトル処理の型分岐を追加する。 |
| [src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.h](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.h) | 3形式のqfn_mmq_*_moe_raw APIを宣言する。 |
| [src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.hip.cpp) | 既存qfn_mmq_moe_implへ渡す薄い型別入口と必要なテンプレート実体化を追加する。 |
| [tests/models/qwen38_flash_next/iq_expert_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/iq_expert_ops_test.cpp) | 新設。CPU参照用の保存データとGPU演算を比較する。 |
| [tests/models/qwen38_flash_next/fixtures/iq_expert.json](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/fixtures/iq_expert.json) | 参照データの形式・由来を記録する。 |

#### 変更内容

- 既に含まれるIQの内積・量子化カーネルを再利用し、型の選択と入口を補う。
- 配布者名で判定せず、IQ3_S・IQ4_NL・IQ4_XSという内部形式を指定して使える構造にする。
- この段階は共通演算の接続。モデルの重みを受け入れる条件は次のコミットで接続する。

#### 確認できた結果

- 最小対応3コミットの構成が保存されており、最終統合版でもiq_expert_opsが合格した。
- 他のモデルが同じ内部形式を使う場合も再利用できる共通入口になった。
- このコミット単独でモデル全体の読み込み・推論完了と扱わない。

根拠：最小対応のコミット対応表（ローカル保管：local/reports/gufo-ud-iq4xs-minimal-20261030-165036/evidence/revisions.json）、最終統合版のIQ演算検証（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/logs/gpu-iq_expert_ops.log）。

### `eb4409d6` — 第2段階：UD-IQ4_XSの重み・CPU参照・実行経路を接続

コミット名：`feat(qwen38-flash-next): connect UD-IQ4_XS expert formats`

#### 意図

共通演算を用意しただけでは読み込めないため、GGUF内の形式をモデル側が認識し、対応する実行経路へ渡せるようにする。

#### 変更したファイル

| 主なファイル | 編集内容 |
| --- | --- |
| [src/models/qwen38_flash_next/weights.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/weights.cpp) | IQ3_S・IQ4_XSのブロック寸法を登録し、IQ3_S・IQ4_NL・IQ4_XSをエキスパートの許可形式へ追加する。 |
| [src/models/qwen38_flash_next/cpu_ops.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/cpu_ops.cpp) | 既存DequantizeIQ3_S / DequantizeIQ4_XSをCPU参照の行読み出しへ接続する。 |
| [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) | Expertsの形式分岐から第1段階の型別APIを呼び出す。 |
| [tests/models/qwen38_flash_next/iq_rows_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/iq_rows_test.cpp) | 新設。行の復号結果を保存済みの参照データと比較する。 |
| [tests/models/qwen38_flash_next/CMakeLists.txt](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/CMakeLists.txt) | iq_rows試験を登録する。 |

#### 変更内容

- HuihuiのUD-IQ4_XSは内部が単一のIQ4_XSではなく、IQ3_S・IQ4_NLなどが混在する。その実際の内部形式を受け入れる。
- 重みのサイズ計算、CPU参照、GPU実行の3箇所を同じ形式定義でそろえる。
- Unsloth UD-IQ4_XS系の形式対応として実装し、検証モデルにHuihuiを使用する。Huihui専用の名前判定は追加しない。

#### 確認できた結果

- モデル側から共通IQエキスパート演算へ接続できるようになった。
- ただしHuihuiのQ6_K出力層は次の2a6beddcで補完しており、このコミットだけを全対応完了とはしない。
- 最終統合版のiq_rowsとiq_expert_opsを含む検証が合格した。Unsloth配布元の別GGUFそのものは今回実行していない。

根拠：最小対応の検証記録（ローカル保管：local/reports/gufo-ud-iq4xs-minimal-20261030-165036/evidence/validation.json）、最小対応の変更記録（ローカル保管：local/reports/gufo-ud-iq4xs-minimal-20261030-165036/evidence/revisions.json）。

### `2a6beddc` — 第2段階の補完：Q6_Kの出力層を既存Qwen演算へ接続

コミット名：`fix(qwen38-flash-next): reuse shared Q6_K output kernels`

#### 意図

IQエキスパートに対応しても残るQ6_K出力層の問題を、共通Qwenカーネルの再利用で解消する。

#### 変更したファイル

| 主なファイル | 編集内容 |
| --- | --- |
| [src/models/qwen38_flash_next/kernels/rocm/device_model.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/device_model.cpp) | Q6_K出力層を許可する。未対応のQ6_K埋め込み・routed expertに対する事前拒否は残す。 |
| [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) | Q6_KのDense投影をLaunchBatchedQuantGEMMFp32へ接続する。 |
| [src/models/qwen38_flash_next/kernels/rocm/batch.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/batch.cpp) | Q6_Kを未対応の小行列経路へ流さず、同じDenseへ渡す。 |
| [tests/models/qwen38_flash_next/iq_expert_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/iq_expert_ops_test.cpp) | 必要な標準ヘッダーを追加する。 |
| [tests/models/qwen38_flash_next/iq_rows_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/iq_rows_test.cpp) | 必要な標準ヘッダーを追加する。 |

#### 変更内容

- Q6_K出力のための別カーネルを新設せず、既存の共通Qwen演算を呼ぶ。
- 第1・第2段階と合わせ、Huihui UD-IQ4_XSの混在エキスパートと出力層を最小限の変更で接続する。
- 後の8e02a42aでは、このQ6_K分岐を他の量子化Denseと共通化したNativeIq4Denseへ整理している。現在のHEADはその後続変更を含む。

#### 確認できた結果

- 2026-10-30の3コミットを合わせた最小構成で、反復入力、キャッシュ再利用と同じ応答、Pythonコード、thinking、同時2要求を確認した。
- その時点のROCm 10.1動作にはDocker環境変数AMD_SERIALIZE_COPY=2を併用した。量子化対応コミットだけで停止の根本原因を直したとは扱わない。
- 最終統合版ではHuihuiの128K MTP文章を外部BF16あり・なし各1回確認し、2026-11-01に通常構成へ採用済み。

根拠：2026-10-30の6要求と結果（ローカル保管：local/reports/gufo-ud-iq4xs-minimal-20261030-165036/evidence/validation.json）、2026-10-30の通常起動確認（ローカル保管：local/reports/gufo-ud-iq4xs-minimal-20261030-165036/evidence/final-state.json）、2026-11-01の採用確認（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/normal/final-verification.json）。

## 5. Orca対応：元Qwen系のIQ4_XS

### `095a6266` — 元Qwen系のOrca IQ4_XSを共通経路へ接続

コミット名：`feat(qwen38-flash-next): restore Orca packed IQ4_XS model support`

#### 意図

Orca配布のGGUFが持つIQ4形式の各投影を、復元した共通演算・高速化経路へ接続する。モデル全体を事前変換する構成を避け、元の保存形式を利用する。

#### 変更したファイル

| 主なファイル | 編集内容 |
| --- | --- |
| [src/models/qwen38_flash_next/weights.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/weights.cpp) | HC down/injectのIQ4_XS、HC up・共有expert downのIQ4_NL、通常投影のIQ4_XS/Q4_K/Q5_K/Q6_Kなどを受け入れる。 |
| [src/models/qwen38_flash_next/kernels/rocm/device_model.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/device_model.cpp) | IQ4_XSを含むstacked uploadを圧縮形式のまま扱い、必要なFP16作業領域の寸法を計上する。 |
| [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) | 小さな投影の型変換とHC CombineのPrefill指定を接続する。 |
| [src/models/qwen38_flash_next/kernels/rocm/batch.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/batch.cpp) | IQ4_XSの型をバッチ側へ伝える。 |
| [src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp) | IQ4_XSの行要素・行サイズを扱う。短いPrefillの最後でもHC Combineのベクトル計算順序を維持する。 |
| tests/models/qwen38_flash_next/native_iq4_ops_test.cpp（ローカル保管：../tests/models/qwen38_flash_next/native_iq4_ops_test.cpp） | 新設。圧縮投影、埋め込み、共有expert、作業領域境界を検証する。 |
| [tests/models/qwen38_flash_next/hc_mix_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/hc_mix_ops_test.cpp) | 1・8・9・15・16トークンに分割したPrefillのHC結果を確認する。 |

#### 変更内容

- 共通部品をモデルの各重みへ接続し、形式が対応する箇所で速度改善6件を利用できるようにする。
- 複数のIQ4_XS行列をまとめて配置する際も、元の圧縮ブロックを維持する。
- Prefillの末尾が短くなったことでHC正規化の計算方法が変わらないよう、Prefillかどうかを明示して呼び分ける。
- このコミットで追加したiq4xs_bind_probe.cppは、後のISTA対応で汎用のweights_probe.cppへ整理した。

#### 確認できた結果

- 最終統合版の実Orca GGUFで1224テンソルの読み込み定義とMTP構成の一致を確認した。
- native_iq4_ops、HC、GDN、共有expertなどの関連演算試験が合格した。
- この移植後のOrca本体をGPUへ載せたモデル全体の会話試験・正式ベンチは今回実施していない。旧版で動いた記録と、今回の検証範囲を区別する。

根拠：実GGUFとMTPの読み込み定義確認（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/checks/model-bindings.json）、native IQ4検証ログ（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/logs/gpu-native_iq4_ops.log）、HC検証ログ（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/logs/gpu-hc_mix_ops.log）。

## 6. ISTA-DASLab GSQ-RCO対応とSC117への適用

### `fbf0a9f8` — ISTA GSQ-RCOの混在形式と浮動小数点重みの差へ対応

コミット名：`feat(qwen38-flash-next): restore ISTA GSQ-RCO mixed quantization support`

#### 意図

ISTA-DASLab GSQ-RCO系モデルの形式・配置に対応し、その共通実装で派生モデルのSC117も扱えるようにする。SC117専用の名前判定や別実装を作らない。

#### 変更したファイル

| 主なファイル | 編集内容 |
| --- | --- |
| [src/models/qwen38_flash_next/weights.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/weights.cpp) | IQ2_S・IQ3_XXS・Q2_0のexpert、F16のPLE畳み込み、BF16のrouterなどを受け入れる。 |
| [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) | 追加expert形式を共通MMQ入口へ接続する。 |
| [src/models/qwen38_flash_next/kernels/rocm/device_model.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/device_model.cpp) | 必要な小行列だけ実行時形式へ変換して配置する。 |
| [src/models/qwen38_flash_next/float_weights.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/float_weights.cpp) | 新設。F32/F16/BF16の行読み出しと、丸め規則を固定したFP16変換を実装する。 |
| [src/models/qwen38_flash_next/float_weights.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/float_weights.hpp) | 変換関数の共通インターフェースを宣言する。 |
| [src/models/qwen38_flash_next/kernels/rocm/float_upload.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/float_upload.hpp) | 新設。分割読み込み・小さな作業バッファ・ピン留め出力を使って必要な行だけ変換して転送する。 |
| [src/models/qwen38_flash_next/reference.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/reference.cpp) | PLE畳み込みとalpha/betaのCPU参照も、実際の形式と実行時変換に合わせる。 |
| [tests/models/qwen38_flash_next/float_weights_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/float_weights_test.cpp) | 新設。浮動小数点変換を検証する。 |
| [tests/models/qwen38_flash_next/float_upload_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/float_upload_test.cpp) | 新設。分割変換・転送を検証する。 |
| [tests/models/qwen38_flash_next/weights_probe.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/weights_probe.cpp) | 既存のOrca向けprobeを整理し、各モデルの重みとMTPを確認する汎用probeにする。 |

#### 変更内容

- ISTA-DASLab公開モデルで使われるIQ3_S・IQ2_S・IQ3_XXS・Q2_0などの混在形式を共通部品で扱う。IQ3_Sという別の配布者があるという意味ではない。
- F16のPLE畳み込みは既存計算が必要とするF32へ変換する。BF16/F16を含む小さなstacked行列は対象を限定してF16へそろえる。元から対応しているF32・Q8_0の経路は維持する。
- 変換はロード時の実行用データに対して行い、元のGGUFを書き換えない。大きな全体コピーを作らず、行単位・分割バッファで処理する。
- FP16で表せない値や非有限値は明示的に拒否する。CPU参照も対応させ、検証側だけ異なる形式を計算しないようにする。

#### 確認できた結果

- SC117 IQ3_Sの実GGUFで1224テンソルと共有MTPの読み込み定義を確認した。形式の共通部品、浮動小数点変換、GPU転送の試験が合格した。
- ISTA自身のGGUFは現在のモデル置き場にないため、この移植では実行していない。SC117についても移植後のモデル全体のGPU推論・正式ベンチは未実施。
- 目標はISTA-DASLab GSQ-RCO対応を共通実装とし、そのままSC117も扱える構成にすること。ISTAとSC117の回答全文一致を合格条件にはしない。

根拠：実SC117 GGUFの読み込み確認（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/checks/model-bindings.json）、浮動小数点転送の検証（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/logs/gpu-float_upload.log）、共通expert演算の検証（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/logs/gpu-native_expert_ops.log）。

## 7. 共通部品・外部PLE・検証の独立コミット

### `59753e6a` — IQ2_S・IQ3_XXS・Q2_0の共通演算を復元

コミット名：`feat(quant): restore shared IQ2_S IQ3_XXS and Q2_0 operators`

#### 意図

ISTA GSQ-RCOなどの混在形式を、モデル別の場当たり的な処理ではなく、再利用可能な読み取り・復号・演算の部品として扱う。

#### 変更したファイル

| 主なファイル | 編集内容 |
| --- | --- |
| [src/core/gguf_reader.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/core/gguf_reader.hpp) | ブロック構造の定義を追加する。 |
| [src/core/quant/ggml_dequant.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/core/quant/ggml_dequant.cpp) | IQ2_S・IQ3_XXS・Q2_0のCPU復号を実装する。 |
| [src/core/quant/ggml_dequant.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/core/quant/ggml_dequant.hpp) | 復号関数を宣言する。 |
| src/core/quant/iq_codebooks.hpp（ローカル保管：../src/core/quant/iq_codebooks.hpp） | 必要なコード表を追加する。 |
| src/models/qwen/gemm_route.hpp（ローカル保管：../src/models/qwen/gemm_route.hpp） | 共通の形式認識を追加する。 |
| [src/models/qwen38_flash_next/cpu_ops.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/cpu_ops.cpp) | モデル側のCPU参照から共通復号を利用する。 |
| [src/models/qwen38_flash_next/weights.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/weights.cpp) | ブロックサイズ・行サイズ計算を追加する。 |
| [src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.hip.cpp) | 形式別の共通入口と演算テンプレートを接続する。 |
| [src/models/qwen38_flash_next/kernels/rocm/mmq/mmvq.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/mmvq.hip.cpp) | ベクトル演算の形式分岐を追加する。 |
| [src/models/qwen38_flash_next/kernels/rocm/mmq/vecdotq.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/vecdotq.hpp) | 対象IQ形式の内積処理を調整する。 |
| [src/models/qwen38_flash_next/kernels/rocm/mmq/VENDOR.md](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/VENDOR.md) | 参照したコードの由来とローカル変更を記録する。 |
| [tests/models/qwen38_flash_next/native_quant_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/native_quant_test.cpp) | 新設。独立した参照データによる復号試験を追加する。 |

#### 変更内容

- IQ2_S・IQ3_XXS・Q2_0を認識して読み出し、CPU参照とGPUのMMQ/MVQへ渡せるようにする。
- 小数を含むIQサブスケールが整数演算で切り捨てられないようにし、独立した参照値で確認する。
- モデルの各重みに形式を許可して接続する部分は、後段のISTA対応fbf0a9f8に分離する。

#### 確認できた結果

- 最終統合版のCPU native_quant、GPU native_expert_ops・iq_expert_ops・routed_wmma_opsが合格した。
- 同じ内部形式を使う他のモデルにも利用できる共通部品となった。モデルの名前で処理を選ぶ設計ではない。
- 共通形式対応だけで任意のモデル全体が動作することを保証するものではなく、重み配置・形状の接続は個別に確認する。

根拠：[CPU検証記録](../evidence/local/reports/gufo-iq-performance-port-20261031/summary.json)、独立参照によるGPU検証（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/logs/gpu-native_expert_ops.log）。

### `2b1143b6` — 外部BF16 n-gramを指定する既存実装を独立保存

コミット名：`feat(qwen38-flash-next): load PLE embeddings from a separate GGUF`

#### 意図

Huihui IQ4_XS本体を保持しながら、PLE表だけを外部GGUFから読む機能を、速度改善やモデル形式対応とは別のコミットにまとめる。

#### 変更したファイル

| 主なファイル | 編集内容 |
| --- | --- |
| [src/cli/serve/serve.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/cli/serve/serve.cpp) | --ple-modelを受け付ける。 |
| [src/cli/serve/inference_backend.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/cli/serve/inference_backend.cpp) | 外部PLEの指定をモデル起動へ伝える。 |
| [src/models/qwen38_flash_next/engine.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/engine.cpp) | 外部PLE用の読み取りオブジェクトを保持し、モデルへ渡す。 |
| [src/models/qwen38_flash_next/ngram.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/ngram.cpp) | 外部GGUFの対象テンソルと必要なメタデータを確認し、既存の行読み取りへ接続する。 |
| [src/models/qwen38_flash_next/ngram.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/ngram.hpp) | 外部PLEを受け取るインターフェースを追加する。 |
| [tests/models/qwen38_flash_next/ngram_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/ngram_test.cpp) | 外部表の読み取り、形式・メタデータなどを検証する。 |
| docs/SERVER.md（ローカル保管：SERVER.md） | 起動オプションを記載する。 |
| docs/models/qwen3.8-flash-next/README.md（ローカル保管：models/qwen3.8-flash-next/README.md） | 使い方を記載する。 |

#### 変更内容

- このコミットは、移植前から稼働していた未コミットの外部PLE対応を履歴に保存したもの。高速化6件の一つではない。
- 外部表の形状とハッシュ計算などに必要なメタデータを照合してから使用する。
- 既存のIQ4_NL/BF16の行読み取り・展開経路を再利用し、元モデルのGGUFを書き換えない。

#### 確認できた結果

- 導入時のモデル不要試験と短い起動確認を経て、後のHuihui 128K MTP文章試験で外部BF16あり・なしの両条件を確認した。
- 現在はHuihui IQ4_XSとDelgon配布のBF16 PLEを組み合わせて通常運用している。
- n-gramの参照表とMTPのドラフトモデルは別物であり、このコミットはMTPモデルの差し替えではない。

根拠：外部PLE導入記録（ローカル保管：local/evidence/deployments/bf16-ple-v0270-20261031/report.ja.md）、[外部BF16あり／なしの実測](../../v0.27/evidence/benchmark/reports/iq-performance-ple-mtp128-20261101-012027/summary.json)。

### `5b8da407` — 圧縮IQ演算と高速経路を独立参照で検証

コミット名：`test(qwen38-flash-next): qualify packed IQ expert projection paths`

#### 意図

実装同士が同じ誤りを共有していても見逃さないよう、独立した参照値と境界条件で追加した演算経路を確認する。

#### 変更したファイル

| 主なファイル | 編集内容 |
| --- | --- |
| [tests/models/qwen38_flash_next/native_expert_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/native_expert_ops_test.cpp) | 新設。圧縮重みの演算を独立参照と比較し、512エキスパートの端や不使用IDを検証する。 |
| [tests/models/qwen38_flash_next/iq_expert_test.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/iq_expert_test.hpp) | 追加形式の呼び分けと検証補助を整理する。 |
| [tests/models/qwen38_flash_next/routed_wmma_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/routed_wmma_ops_test.cpp) | 追加IQ形式のWMMA、端数行、参照計算との比較を拡張する。 |
| [tests/models/qwen38_flash_next/CMakeLists.txt](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/CMakeLists.txt) | 試験を登録する。 |

#### 変更内容

- 入力・符号・スケールを制御したデータを使い、量子化重み側の計算を切り分ける。
- IQ2_S・IQ3_XXSの小数スケール、エキスパートID 0/511、不使用ID -1、1・8・9・32・33行などを検証する。
- FP64の参照、保存済みの独立復号結果、既存MMQと新しいWMMAの比較を使い分ける。

#### 確認できた結果

- 最終統合版でnative_expert_opsとrouted_wmma_opsが合格した。今回のGPU演算試験は全9項目が合格している。
- このコミットはテストの追加で、通常推論へ新しい高速化処理を追加するものではない。
- 演算単位の検証は、モデル全体の正答率や全会話パターンの動作保証とは分けて扱う。

根拠：GPU演算9項目の結果（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/checks/gpu-operators.json）、独立参照検証ログ（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/logs/gpu-native_expert_ops.log）、WMMA検証ログ（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/logs/gpu-routed_wmma_ops.log）。

## 8. 統合後の速度と検証範囲

### Huihuiの128K MTP文章

| n-gram | 構成 | 回数 | Prefill (tok/s) | TTFT (秒) | Decode (tok/s) |
| --- | --- | --- | --- | --- | --- |
| 外部BF16あり | 移植前 | 3回平均 | 695.66 | 2.968 | 32.16 |
| 外部BF16あり | 移植後 | 1回 | 943.94 | 2.192 | 32.35 |
| 内蔵n-gram | 移植前 | 1回 | 581.56 | 3.546 | 32.15 |
| 内蔵n-gram | 移植後 | 1回 | 1004.70 | 2.057 | 31.01 |

保存済みの移植前実測に対して、Prefillは外部BF16ありで **+35.7%**、内蔵で **+72.8%**。今回確認できた改善は主にPrefillと、それに伴うTTFTの短縮だった。DecodeはBF16ありでほぼ同程度、内蔵では今回値が低くなっており、生成速度まで一律に改善したとはしない。

- 同じHuihui UD-IQ4_XS、ROCm 10.1、AMD_SERIALIZE_COPY=2。Gufo公式single-mtp/prose、1セッション、context133760、thinking off、greedyで測定。
- 128Kのキャッシュ済み履歴を用意した後、追加約2KをPrefillし128トークン生成。今回の実数は両条件とも履歴131052・追加2054・生成128トークン。
- TTFTは追加要求に対する時間。128K全体の履歴準備時間ではない。
- 移植後はユーザー指定で各1回。移植前のBF16は3回平均、内蔵は1回。測定日・背景負荷・ファイルキャッシュの差は残る。
- 高速化6件は形式・形状の条件を満たす経路で使われる。6件すべてがHuihuiの全層で使われた、または同じ割合で効いたという意味ではない。

詳細：[BF16あり／なし比較レポート](../../v0.27/benchmarks/benchmark/reports/iq-performance-ple-mtp128-20261101-012027/report.ja.md)、[集計JSON](../../v0.27/evidence/benchmark/reports/iq-performance-ple-mtp128-20261101-012027/summary.json)。

### 確認済み範囲

| 対象 | 保存済み記録で確認できたこと | 今回の範囲に含まれないこと |
| --- | --- | --- |
| 共通演算・読み取り | CPU6件、GPU演算9件が合格。ビット一致を確認した箇所と、独立参照との誤差で検証した箇所がある。 | 全モデルの正答率・回答品質の評価。 |
| Huihui最小対応 | 2026-10-30に反復入力、キャッシュ再利用、コード生成、thinking、同時2要求を確認。 | Unsloth配布元の別GGUFの実行。 |
| Huihui最終統合版 | 1224テンソルとMTPの読み込み確認、128K MTP文章を2条件各1回、通常2セッション設定の起動と短い応答。 | 全項目3回の正式ベンチ、全会話パターンの安定性保証。 |
| Orca最終統合版 | 実GGUFの1224テンソルとMTP構成、関連演算試験。 | 移植後のOrcaモデル全体のGPU推論・正式ベンチ。 |
| ISTA-DASLab／SC117最終統合版 | SC117実GGUFの1224テンソルと共有MTP、混在形式と浮動小数点変換の試験。 | ISTA自身のGGUFの実行、移植後のSC117モデル全体のGPU推論・正式ベンチ。 |

GPU演算試験：`swiglu_half`、`hc_mix_ops`、`gdn_ops`、`float_upload`、`native_expert_ops`、`iq_expert_ops`、`native_iq4_ops`、`blaslt_ops`、`routed_wmma_ops`。CPU試験：`gguf_reader_test`、`config`、`ngram`、`iq_rows`、`native_quant`、`float_weights`。テストプログラムの実行時間はモデルのPrefill・Decode速度とは別の値として扱う。

## 9. v0.27.0採用時の通常運用と、コミットに含まれない設定

以下は初回採用時の記録。現在のバージョンと起動確認は第10節を参照。

2026-11-01に、速度改善を移植した最終コミット`fbf0a9f8`のイメージを通常版として採用した。Huihui IQ4_XS＋外部BF16 n-gram、2セッション・各131072トークン、thinking auto、共有MTP、RAMキャッシュ4 GiB、prefill chunk512、ROCm 10.1で稼働している。

Dockerコンテナ名は`Gufo`、再起動設定は`on-failure:3`。通常設定と再作成用コマンドを更新済み。次回のDocker起動も同じ構成を使う。採用時にready応答、モデル登録名、実バイナリ、実行ライブラリ、通常引数、短い応答「OK」を確認した。

`AMD_SERIALIZE_COPY=2`はDocker環境変数によるコピー完了待機の設定で、上記14コミットの高速化コードとは分離している。ROCm 10.1で発生した停止の根本原因を、このモデル対応・高速化コミットで完全に修正したとは扱わない。

根拠：通常採用の検証（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/normal/final-verification.json）、通常設定（ローカル保管：/home/user/Apps/Gufo/normal-settings.json）、再作成用コマンド（ローカル保管：local/evidence/deployments/iq-perf-port-20261031-230741/normal/docker-create.json）。

## 10. v0.28.1の取り込みとAttention境界修正（2026-11-01追記）

既存14コミットを維持して公式v0.28.1をマージし、その後にAttentionの修正を独立コミットにした。今回のユーザー指示により、正式な3回ベンチは後日に延期。演算試験と短い同時利用確認を行い、BF16 n-gramありのMTP文章128Kを1回だけ取得する。

### `0090201a` — 公式v0.28.1を既存の拡張へ統合

コミット名：`chore: merge Gufo v0.28.1 with local model extensions`

#### 意図

公式v0.28.1のHC演算・MTP制御などの修正を取り込み、Huihui／Unsloth・Orca・ISTA-DASLab／SC117対応と速度改善6件を保持する。公式の修正と、今回こちらで追加するAttention修正を履歴上で分ける。

#### 変更したファイル

| ファイル | 主な変更 |
| --- | --- |
| [tests/models/qwen38_flash_next/CMakeLists.txt](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/CMakeLists.txt) | 公式が追加したHC経路テストと既存拡張のテストを同じ一覧へ登録。 |
| [tests/models/qwen38_flash_next/hc_mix_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/hc_mix_ops_test.cpp) | こちらのFP32／FP16出力確認と公式のHC inject部分和確認を両方実行。 |
| [src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp) | 公式のHC・SSM等の数値一致修正を、既存の拡張を保ったまま統合。 |
| [src/models/qwen38_flash_next/mtp_policy.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/mtp_policy.hpp) | 公式のドラフト信頼度によるMTP制御を取り込み。 |
| [src/models/qwen38_flash_next/engine.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/engine.cpp) / [src/models/qwen38_flash_next/kernels/rocm/batch.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/batch.cpp) | 公式のMTP処理変更を取り込み。 |

この表は主要ファイル。v0.28.0を経てv0.28.1までの公式差分全体も含むため、全変更ファイルは付録Aに別記する。

#### 変更内容

- 親コミットは、既存拡張の`fbf0a9f8`と公式v0.28.1の`12effe36`。既存のコミットIDは書き換えていない。
- 手動解消した競合はテスト登録一覧とHCテスト。同じ場所に別目的のテストが追加されていたため、それぞれの関数と登録を保持して結合した。本体に競合する2つの実装を並べて残したものではない。
- 公式HC修正は、処理する行数・経路による部分和などの数値差を抑えるもの。今回追跡したAttentionの密／疎境界は公式v0.28.1でも残っていたため、次の独立コミットで対応する。

#### 確認できた結果

- 公式の書式検証721ファイル、ROCm 10.1での本体ビルドに成功。
- 統合後の`hc_mix_ops`と`hc_route_ops`に合格。こちらのFP16出力確認と公式HC確認の両方を実行した。
- Huihuiを使った確認は次項のAttention修正も含む構成。統合コミット単体の速度差や回答品質は測定していない。
- Orca・SC117の拡張コードは維持。今回は両モデルの再ロードや正式ベンチを実施していない。

### `8f4a7e77` — 密／疎境界をまたいでもAttentionの計算順序を揃える

コミット名：`fix(qwen38-flash-next): preserve dense attention across narrow batch boundaries`

#### 意図

MTPの検証バッチが2,048トークンの境界をまたいだとき、境界前の行まで疎Attentionへ回り、単独ARと丸め順序が変わる問題を修正する。同時利用時の回答不一致に関係した箇所であり、量子化拡張やモデル名による分岐ではない。

#### 変更したファイル

| ファイル | 主な変更 |
| --- | --- |
| [src/models/qwen38_flash_next/kernels/rocm/kernels.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hpp) | 内部Attention APIに`dense_tokens`を追加し、全キーが見える範囲でのみ使う契約を明記。既定値0。 |
| [src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp) | GPU上の各行の位置を見て密／疎経路を選ぶ。GPUグラフ再利用時も現在位置で判定。 |
| [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) | モデル設定の既存`indexer_top_k`を境界として渡す。 |
| [tests/models/qwen38_flash_next/attention_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/attention_ops_test.cpp) | 境界前後、2/4/8行、分割あり／なし、マスク走査／選択リスト、GPUグラフ再利用の回帰確認を追加。 |
| docs/models/qwen3.8-flash-next/QUALITY.md（ローカル保管：models/qwen3.8-flash-next/QUALITY.md） | 数値契約、原因、検証条件と結果を英語で記録。 |

#### 変更内容

- 密経路と疎経路では、不完全な4キー単位の末尾をどの部分和に含めるかが違い、同じQ/K/Vと可視キーでもFP32の丸め差が生じていた。
- マスクの有無だけで一括判定せず、各行のGPU上の位置が既存の密区間内なら単独ARと同じ密経路を使う。
- 境界後のマスクは引き続き有効。一般的なマスク呼び出しは引数の既定値0で従来どおり動く。
- GPU起動、バッファ、モデル別フラグは追加しない。新しいソース・コメントは英語で、個人のパスは入れていない。
- `AMD_SERIALIZE_COPY=2`による転送待機は継続。このAttention修正とは別件であり、以前のROCm／WSL転送停止を解消したという扱いにはしない。

#### 確認できた結果

- 境界の36条件すべてで単独処理とビット一致。独立FP64参照との最大誤差は`8.23761e-8`（既存の上限`1e-6`以内）。GPUグラフで位置を変更した再実行も一致。
- 同じテストで`dense_tokens=0`にした比較用プログラムは不一致を検出した。修正を無効にした挙動を回帰テストが検出できることを確認。
- 既存Attentionテスト全体にも合格。
- Huihui IQ4_XS＋BF16 n-gram、キャッシュ済み入力2,040トークン、6要求同時、MTP上限7／下限1の1回確認で、6件の128トークン回答すべてが単独ARと完全一致。
- 試験初回は私が指定した`--min-draft-tokens 3`がサーバーの許容値ではなく、読み込み前に終了した。通常Gufoへ復帰後、対応している上限7／下限1に直して同時MTP確認だけを再実行した。合格済みの演算試験とAR結果は同一バイナリの記録を再利用。初回失敗の記録も保持している。
- 正式な3回ベンチ、全同時数・全履歴の再確認、外部n-gramなしの再測定は今回実施していない。

通常採用・128K測定：2026-11-01 17:08 JSTに新版を通常版へ反映。Huihui IQ4_XS＋BF16 n-gram、2セッション・各128K、ROCm 10.1、thinking auto、MTP上限7／下限1、RAMキャッシュ4 GiB、prefill chunk512、再起動`on-failure:3`を維持。ready、モデル名、バイナリとライブラリ、短い応答「OK」を確認し、次回Docker起動用設定も更新済み。

| 今回の公式MTP文章128K | 回数 | Prefill (tok/s) | TTFT (秒) | Decode (tok/s) |
| --- | ---: | ---: | ---: | ---: |
| Huihui IQ4_XS＋BF16 n-gram、v0.28.1＋境界修正 | 1 | 961.10 | 2.153 | 33.66 |

ベンチは1セッション・context133760・thinking off。キャッシュ済み履歴131052に追加2054トークン、生成128トークン。正式3回の測定は後日。[実測とメモリのレポート](../../v0.28/benchmarks/benchmark/reports/v0281-attention-boundary-20261101-163558/report.ja.html)に、前回の3回平均との比較と今回の観測範囲を記載。

記録：演算試験（ローカル保管：local/evidence/deployments/v0281-attention-boundary-20261101-163558/checks/operators.json）、[同時回答照合](../evidence/local/evidence/deployments/v0281-attention-boundary-20261101-163558/checks/comparison.json)、[今回の出力先](../README.md)。

## 11. Qwen3-ASR-1.7B-JA対応（2026-11-01追記）

### `4336a8d5` — 単一safetensorsと共有出力重みの読み込み

`feat(asr): support single-file checkpoints with tied output weights`

完全なコミットID：`4336a8d5cac97a3d4ce4d21d31147d12fbce6870`。ASR専用ブランチ `feat/qwen3-asr-single-file-tied-weights` に採用。

#### 意図

`neosophie/Qwen3-ASR-1.7B-JA` を配布された形のままGufoで使う。元Qwenの重みを混ぜず、Gufoの既存ASR計算経路へ接続する。Huihuiの通常イメージは `8f4a7e77` のまま維持し、ASR専用の別イメージに採用した。

#### 変更したファイル

| ファイル | 役割 |
| --- | --- |
| `src/models/qwen3_asr/config.hpp` / `config.cpp` | `tie_word_embeddings` の設定を保持・読み取り |
| `src/models/qwen3_asr/loader.cpp` | 単一ファイルの読み込みと、共有出力重みの参照を追加 |
| `src/models/qwen3_asr/tests/config_test.cpp` | 共有設定の明示・省略を検証 |
| `src/models/qwen3_asr/tests/checkpoint_test.cpp` | 単一・2分割、共有・非共有、明示重み優先、不足データを検証 |
| `src/models/qwen3_asr/tests/loader_test.cpp` | 単一と従来2分割の実モデル検証 |
| `src/models/qwen3_asr/CMakeLists.txt` | モデル専用CPU回帰テストを登録 |
| `docs/models/qwen3-asr/README.md` | 対応する保存形式と共有重みの条件を説明 |

#### 変更内容

`model.safetensors` がある場合に読み込み、なければ従来の2分割を読む。出力重みが省略され、モデルが共有を明示した場合だけ、同じモデルの埋め込みへの参照を登録する。出力重みが明示されていれば優先する。重みの複製・元Qwenからの補充・カーネル変更は行っていない。

#### 確認できた結果

整形722ファイル、CPUテスト2件、実モデルのローダー検査が合格。日本語合成音声7.59秒は3回とも全文一致し、処理0.890 / 0.554 / 0.571秒。公式の英語音声も参照文と一致した。Huihuiが文字出力中にASRを実行して両方完了。これは限定した動作確認で、一般的な認識精度や正式性能ベンチの結果ではない。

初回起動はGPUへ重みを配置できず失敗した。診断付き1回と、その後の診断なし通常イメージ2回は起動成功したが、初回の原因は未特定。診断コードは採用していない。現在のHuihuiは停止・再起動しておらず、Hermesの設定も未変更。

ソース：/home/user/Apps/Gufo/deployments/qwen3-asr-ja-20261101-211213/source（ローカル保管：..）。設定：asr-settings.json（ローカル保管：/home/user/Apps/Gufo/asr-settings.json）。[日本語検証記録](../../v0.28/investigations/local/reports/qwen3-asr-ja-20261101-211213/report.ja.md)。

## 12. v0.30.0の取り込みとカスタムの再接続（2026-11-05追記）

以前の実装・公式の変更・判断理由・適用済みの修正・未確認範囲を、[カスタム全件の点検・変更記録](../../v0.30/investigations/local/updates/v0300-20261105/custom-audit.ja.md)に記載した。

### `a8e0e441` — 公式v0.30.0と既存拡張の統合

#### 意図

公式v0.29以降のモデル拡張構造・量子化共通処理・HC精度・routed prefill・キャッシュの変更を取り込み、既存のHuihui／Orca／ISTA-DASLab・SC117／外部PLE／日本語ASR対応を接続し直す。競合しない箇所も確認し、重複した関数や旧経路の残存を整理する。

#### 変更したファイル

下表はカスタムとの接続に関係する主要ファイル。上流側の更新を含む全変更はGitから取得したファイル一覧（ローカル保管：local/updates/v0300-20261105/commit-files.json）を参照。

| ファイル | 役割 |
| --- | --- |
| `src/core/gguf_reader.hpp`、`src/core/quant/ggml_dequant.{hpp,cpp}` | 公式IQ展開を利用し、追加で必要なQ2_0の形式・展開を保持 |
| `src/families/qwen/hip_common/gemm_route.hpp`、`kernels/`配下 | 新しい共通ライブラリ配置へIQ演算と小行列処理を移植 |
| `src/models/qwen38_flash_next/extension.{cmake,json}` | 拡張方式で追加ソースとPLEオプションを登録 |
| `src/ext/text.hpp`、`src/cli/serve/inference_backend.{hpp,cpp}`、`serve.cpp` | 外部PLEの指定を新しいモデル生成入口へ渡す |
| `src/models/qwen38_flash_next/runner.{hpp,cpp}`、`continuation_identity.{hpp,cpp}` | PLE読み込みと、異なるPLEの永続キャッシュの分離 |
| `src/models/qwen38_flash_next/kernels/rocm/{device_model.cpp,executor.cpp,kernels.hip.cpp}` | 公式HC精度修正・128トークンrouted配置とIQ経路を接続 |
| `CMakeLists.txt`、`tests/models/qwen38_flash_next/` | 新ターゲットへの試験の接続、追加形式・PLE識別・境界形状の確認 |
| `include/gufo/ext/changes/0.1/mixed-quantization-ple-runner-options.md`、`SURFACE` | 公開契約の追加を公式の方式で記録 |

#### 変更内容

- IQ2_S／IQ3_XXSのCPU展開は公式実装へ寄せ、重複した関数・switch分岐を削除した。ISTA対応に必要なQ2_0と、モデルへの量子化・浮動小数点重みの接続は残した。
- 旧モデル別CMakeを復活させず、公式の`extension.cmake`へ追加ソースを登録した。ASRの単一チェックポイント確認はルートCMakeの新しい登録位置へ移した。
- 外部PLEは`TextRunnerOptions`からFlash-Nextのrunnerへ渡す。外部PLEを使う永続キャッシュではPLE自体の識別情報を含め、内蔵・外部および異なる外部PLEの状態を区別する。外部指定がない場合の既存の識別形式は維持する。
- HC inject重みをFP32へ変換する上流の修正を保持した。旧コードの再利用によってBF16／FP16へ戻さない。OrcaのIQ経路で用いる補助の半精度出力は適用条件を分けて残した。
- 上流の128トークンrouted配置へIQ形式を接続した。Q4／Q5のpaired projectionは上流の専用カーネルを使い、IQ用に共有カーネルを拡張する。旧Q4／Q5用のレジスター分岐を共有カーネル内に重複して残さない。
- 以前の独自Attention境界修正は、上流のdense extent判定へ置き換えた。新しいprefill・キャッシュ制御は上流の処理を使う。
- モデルからcoreへの直接includeを公開ヘッダー経由へ移した。ソースの英語・個人パス非埋め込み方針を確認した。

#### 確認できた結果

- 18ファイルのマージ競合と、競合表示が出なかったCPU量子化の重複を整理した。
- 公式CPUチェックは170件中159成功・9スキップ・2失敗。2失敗はASR試験の所有情報不足が共通原因。`644e43c3`で修正後、関連unit 28件と所有・依存・境界検査は成功。全170件の再実行成功とは区別する。
- GPUビルドで見つかった公開ヘッダー漏れとrouted経路の接続漏れを後続コミットで修正。最終版 `9c6ab4b6` の本体・対象演算試験のビルドは成功。
- 書式検査892ファイル、拡張API 0.1.21、モデル／core・CLI／拡張境界、所有関係、family graphの検査は成功。
- 最終版の演算13項目とASR checkpoint fixtureが成功。Orca・SC117・Huihui内蔵／外部PLEの4条件、計24要求の短い動作確認は成功。再実行・キャッシュ・同時2件の出力一致と反復入力の完了を確認した。
- ユーザーの明示許可後、Huihui＋外部BF16 PLEを通常設定で起動。旧起動引数と完全一致し、2セッション各128K・ROCm 10.1・コピー待機設定を維持。旧版は停止退避。正式ベンチはユーザー指示で実施していない。詳細は[点検記録](../../v0.30/investigations/local/updates/v0300-20261105/custom-audit.ja.md)を参照。

### `644e43c3` — ASR checkpoint試験の所有情報を補う

#### 意図

新版の拡張境界検査へ、移動したASR試験の所属を明示する。

#### 変更したファイル

`src/models/qwen3_asr/library.json`。

#### 変更内容

ルートCMakeに登録したcheckpoint試験をASRライブラリのchecksへ追加。単一safetensors・tied outputの読み込み機能は維持する。

#### 確認できた結果

不足を検出した検査を再実行し、関連unit 28件とtarget ownership／family graph／境界のgateが通過した。

### `dae0eba7` — 公式の浮動小数点展開・BF16演算へ共通化

#### 意図

新版に同じ役割の部品がある箇所は独自の重複を削り、必要な型・形状の接続と検査を残す。

#### 変更したファイル

`src/models/qwen38_flash_next/float_weights.cpp`、`kernels/rocm/{float_upload.hpp,blaslt.cpp,kernels.hip.cpp,kernels.hpp}`。

#### 変更内容

浮動小数点展開を公式`DequantizeRow`へ変更。独自BF16 wrapperを削除し、既存`OwnBf16`と`DenseBf16Gemm`へHC／PLE形状を接続した。浮動小数点の型制限・非有限値拒否、FP16への明示丸め、有界uploadは残す。`ReadAt`の公開宣言ヘッダーを補った。

#### 確認できた結果

書式・拡張API・依存境界の検査は通過。浮動小数点展開・uploadの演算試験は通過。BF16の分割境界・独立参照を含む後続の全13項目も成功した。独自wrapper単独の速度差は測定していない。

### `3f01d157` — 新しいrouted配置へIQを接続し直す

#### 意図

旧Q4／Q5のpaired分岐を除去した後に残った参照と、新しい128トークン配置の呼び出し不足を解消する。

#### 変更したファイル

`src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp`。

#### 変更内容

旧affine register-cacheへの参照を除去。非paired引数の既定値を補い、IQ pairedの幅を128に明示。128トークンのIQ downを既存の型別カーネルへ渡す。Q4／Q5 pairedと通常downの公式専用カーネルは保持する。

#### 確認できた結果

GPU本体と対象演算試験のビルドは成功。後続のrouted試験で、64幅IQのdispatch漏れを検出したため通常版には採用せず、次のコミットで修正した。

### `9c6ab4b6` — 64トークンIQ経路の接続を保持

#### 意図

旧IQの64幅と、新版の128幅の両方を有効にし、既存の境界検証を維持する。

#### 変更したファイル

`src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp`。

#### 変更内容

64幅でIQ型を`LaunchRoutedF16<64>`へ渡す。128幅は同じIQ型判定を利用し、通常形式は公式の専用downを使う。参照値・許容誤差・試験の対象形状は緩めていない。

#### 確認できた結果

書式検査とGPUビルドは成功。候補イメージの版表示、最終ソースrevisionとELFのSHA-256一致を確認。演算13項目と4条件の短いモデル確認が成功し、通常版へ反映した。条件と未測定範囲は[カスタム点検記録](../../v0.30/investigations/local/updates/v0300-20261105/custom-audit.ja.md)へ記載した。

## 13. v0.30.0カスタムの再点検・整理（2026-11-06追記）

項目1〜10の合意に基づく整理。7・9・10は現状維持。8は共有する公式カーネル本体と独自起動条件を区別して点検し、演算の変更を増やさず契約説明を整理した。公開APIに含まれる未参照の型宣言は互換性のため保持した。

詳細：[整理・追試記録](../../v0.30/investigations/local/updates/v0300-review-20261106/report.ja.md)。実装・コミット完了後に、5構成の公式ベンチマークを各3回実施した。[v0.28.1・今回・公式参考値の比較レポート](../../v0.30/benchmarks/benchmark/reports/all-models-v0300-20261106-050505/report.ja.html)。

### `944bb32d` — CPU展開の共通化

#### 意図

モデル側の重複した展開処理を公式入口へ集約する。

#### 変更したファイル

- [src/models/qwen38_flash_next/cpu_ops.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/cpu_ops.cpp)
- [tests/models/qwen38_flash_next/native_quant_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/native_quant_test.cpp)

#### 変更内容

対応型と範囲検査を保ち、共通DequantizeRowへ委譲。独自Q5_1展開を削除。公開型宣言は互換性維持。

#### 確認できた結果

独立した9形式のfixtureでモデル側の展開結果も照合し、native_quant合格。

### `cee020c2` — Attention独自境界試験の整理

#### 意図

公式へ統一した演算に付随する独自試験を削除する。

#### 変更したファイル

- [tests/models/qwen38_flash_next/attention_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/attention_ops_test.cpp)

#### 変更内容

CheckDenseWindowとその呼び出しを削除し、公式の境界・budget検査を維持。

#### 確認できた結果

attention_ops合格。

### `81687a7e` — BF16 HC／PLE経路の追試と採用

#### 意図

現行hipBLASLtとの差を測り、必要な固定順序経路を選ぶ。

#### 変更したファイル

- [src/models/qwen38_flash_next/kernels/rocm/blaslt.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/blaslt.cpp)
- [tests/models/qwen38_flash_next/blaslt_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/blaslt_ops_test.cpp)

#### 変更内容

旧inject形状だけ除外。既存のGemm／LibraryGemmへ同一データを渡す明示実行用--compareを演算試験へ追加。

#### 確認できた結果

有効HC／PLE4形状は固定順序を採用。分割不一致0、512行で約2.0〜4.3倍短時間。単体演算の比較であり全体速度ではない。

### `fa136df5` — 外部Q8 PLEの検証

#### 意図

外部ファイルの内部形式によってQ8_0／BF16を選ぶ既存入口を確認する。

#### 変更したファイル

- [tests/models/qwen38_flash_next/ngram_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/ngram_test.cpp)

#### 変更内容

小型GGUFによる外部Q8_0とBF16の読み込み、mapping解放後の読み出し、重複行のgatherを検証。

#### 確認できた結果

ngram試験合格。実配布Q8ファイルは正式ベンチ準備で別途取得・検証する。

### `3dc7bd00` — 不要なIQ64幅の整理

#### 意図

本番未使用の処理と古い説明を削減する。

#### 変更したファイル

- [src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp)
- [tests/models/qwen38_flash_next/routed_wmma_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/routed_wmma_ops_test.cpp)

#### 変更内容

64トークン幅のdispatchを削除。試験は16／48／128を使い、端数・guard・参照を維持。旧paired説明を修正。

#### 確認できた結果

routed_wmma_ops合格。通常Prefillは既に128幅であり、削除による高速化は主張しない。

### `0632426b` — HC専用試験の復元

#### 意図

統合で抜けたFP32／FP16同時出力試験を実際に実行する。

#### 変更したファイル

- [tests/models/qwen38_flash_next/hc_mix_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/hc_mix_ops_test.cpp)

#### 変更内容

既存CheckF32HalfOutputを1／8／9／33／2049トークンで呼び出す。

#### 確認できた結果

hc_mix_ops合格。FP32結果、FP16コピー、inject部分和の一致を確認。

### `c1170a4a` — 小行列ランチャーの契約説明整理

#### 意図

公式の説明と独自ランチャーの説明を正しく対応させる。

#### 変更したファイル

- [src/families/qwen/kernels/ops/gemm.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/families/qwen/kernels/ops/gemm.hpp)

#### 変更内容

Grouped用の説明を専用宣言へ、既存Batchedの説明を公式宣言へ戻す。計算本体は変更しない。

#### 確認できた結果

native_iq4_opsとswiglu_half合格。既存カーネルの共有・精度維持は継続。

### 実装完了後の公式ベンチマーク（2026-11-06）

5構成すべてで、単独72測定・同時利用189応答・読み込み3回・固定負荷メモリ2種類×3回を取得。入力・出力長、ログのハッシュ、測定回数、HIP監視記録を確認した。Huihuiの3構成は指定PLE以外の起動設定が一致している。

| 構成 | 単独AR／MTP一致：v0.28.1 → 今回 | 同時利用一致：v0.28.1 → 今回 |
| --- | --- | --- |
| SC117 IQ3_S・内蔵 | 6/24 → 6/24 | 15/15 → 14/15 |
| Huihui UD-IQ4_XS・外部BF16 | 4/24 → 3/24 | 12/15 → 14/15 |
| Huihui UD-IQ4_XS・外部Q8 | 未測定 → 5/24 | 未測定 → 15/15 |
| Huihui UD-IQ4_XS・内蔵 | 3/24 → 3/24 | 12/15 → 12/15 |
| Orca IQ4_XS・内蔵 | 7/24 → 5/24 | 15/15 → 15/15 |

回答一致には未達が残る。これは同一入力の出力一致であり、一般的な正答率ではない。不一致を含む速度値と全個別値を保持し、比較レポートで明示した。旧版との差には公式本体・カスタム整理・測定日・稼働状況の差が含まれ、個々のコミットの効果へ分解していない。短い測定区間でWindows監視サンプルが存在しない場合は、その区間のWindowsピークを未測定としている。

通常版は整理済みc1170a4aのイメージを採用し、Huihui＋外部BF16・2セッション×131072・従来のDocker設定へ復帰。ASR・Strataは停止を維持。通常起動の確認記録（ローカル保管：local/updates/v0300-review-20261106/normal/verification.json）。

### 通常PLEを外部Q8へ変更（2026-11-06、設定のみ）

ボスの指定で、Huihuiの通常PLEを外部BF16から外部Q8_0へ切り替えた。ソース・イメージはc1170a4aのままで、コードの追加コミットはない。Appsの通常設定2ファイルとDocker作成引数の`--ple-model`参照先だけを変更し、その他の通常引数・環境・マウント・再起動設定を維持した。

API・モデル名・実際のQ8ファイル参照・短文応答を確認。ASRは同じコンテナを停止したまま。追加ベンチは実施していない。[切り替え記録](../../v0.30/investigations/local/updates/huihui-q8-adoption-20261106-111452/README.ja.md)。

## 14. Unsloth v3.1のQ5_0対応（2026-11-06）

作業ブランチは `codex/qwen38-q5-0`。通常採用中のc1170a4aを基点に、読み込み修正と形式対応を分けて追加した。2コミットともローカルに保存し、pushはしていない。

### `1f2fe101` — メタデータだけの先頭GGUFを読み込む

#### 意図

Unsloth v3.1の第1ファイルはテンソル数0で、メタデータの直後に終わる。データ領域が存在しないのに6バイトの末尾paddingを要求し、`Malformed or truncated GGUF` となっていた。

#### 変更したファイル

- [src/core/gguf_reader.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/core/gguf_reader.cpp)
- [tests/core/gguf_reader_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/core/gguf_reader_test.cpp)

#### 変更内容

メタデータ・テンソル定義を読み終えた後、テンソル数が0ならデータ領域のalignment検査を省略する。テンソルを含むファイルの範囲検査は維持する。テストは未整列の正常ヘッダー、splitの先頭／後続ファイルからの読み込み、途中で切れたメタデータの拒否を確認する。

#### 確認できた結果

新しい回帰テストは修正前に失敗、修正後に成功。GGUF reader・identity試験が合格し、実ファイル4本を1,224テンソルとして読み込めた。速度改善は測定していない。

### `fe85127b` — Q5_0 expertを既存MMQへ接続する

#### 意図

未加工Unsloth v3.1のexpert down 33本に使われるQ5_0を、既存の量子化演算で処理できるようにする。許可一覧だけの追加ではCPU・GPUの呼び出しが不足するため、各入口を揃える。

#### 変更したファイル

| ファイル | 役割 |
|---|---|
| [weights.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/weights.cpp) | expert形式の許可一覧 |
| [cpu_ops.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/cpu_ops.cpp) | 既存CPU展開処理への振り分け |
| [executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) | prefill時のMMQ呼び出し |
| [qfn_mmq.h](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.h) | 既存規則に沿った宣言 |
| [qfn_mmq.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.hip.cpp) | 既存MMQテンプレートとvector入口への接続 |
| [mmvq.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/mmvq.hip.cpp) | 既存Q5_0内積とvector起動への振り分け |
| [iq_expert_test.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/iq_expert_test.hpp) | 既存演算テストからの呼び出し |
| [native_quant_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/native_quant_test.cpp) | CPU展開の独立した期待値との照合 |
| [native_expert_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/native_expert_ops_test.cpp) | GPUの独立した期待値・実形状・境界の照合 |

#### 変更内容

近隣のQ5_1と同じ規則で `qfn_mmq_q5_0_moe_raw` を追加し、既存の `vec_dot_q5_0_q8_1` と `mul_mat_q_case<GGML_TYPE_Q5_0>` を呼ぶ。型に応じた通常のMMQ経路を使い、新しい専用カーネル・設定・配布者名の分岐は追加していない。

#### 確認できた結果

CPU展開、GPUのQ5_0 40条件、既存native／IQ形式の演算試験、892ファイルの共有整形チェックが合格。Q5_0の40条件では独立した期待値との最大絶対誤差が0だった。

未加工Unsloth v3.1を内蔵PLE・8K contextで起動し、通常生成／共通Q8 MTPのAPI確認、日本語生成、再実行、2同時要求、算術が合格。pp約2048／tg128を各モードでウォームアップ1回＋正式3回測定し、正式3回とも通常生成とMTPで回答ハッシュが一致した。生成速度平均は通常29.39 tok/s、MTP32.53 tok/s。旧版は同モデルを読めず、改修前後の速度比較は未実施。長文context・一般的な回答品質・全モデルの合格を意味しない。

試験イメージは `gufo:v0.30.0-q5-0-fe85127b-rocm101`。ユーザー指定どおり稼働中の処理の完了を待って試験し、終了後は同じ通常Gufoコンテナ（c1170a4a・Huihui＋外部Q8 PLE）へ復旧した。構成・通常設定の一致と応答を確認し、ASR・Strataも維持。Luni差分の取得・GGUF加工は未実施。

[実装・検証記録](../../v0.30/investigations/local/updates/q5-0-20261106/README.ja.md)、[ベンチの個別値・メモリ](../../v0.30/benchmarks/benchmark/runs/q5-0-20261106/README.ja.md)。

## 15. Gufo v0.31.0取り込みとカスタム整理（2026-11-08）

更新前は`fe85127b`、上流の正式版は`v0.31.0`（`be7bd3be`）。ブランチは`codex/update-v0310-20261108`。通常モデルは2026-11-07に採用したToRO3＋内蔵PLEであり、過去のHuihui＋外部Q8へ戻さない。詳細・分類表・検証ログは[今回の記録](../../v0.31/investigations/local/updates/v0310-20261108/README.ja.md)へ保存する。

### `5bc37d69` — 上流更新と既存拡張の接続

#### 意図

v0.31.0の公式修正を採用し、量子化・外部PLE・ASRの既存対応を維持する。上流が解消した問題の独自実装は残さない。

#### 変更したファイル

上流取り込み一式。競合を解消したファイルは`src/core/gguf_reader.cpp`と`src/models/qwen38_flash_next/runner.cpp`。拡張APIの追加記録は`include/gufo/ext/changes/0.2/mixed-quantization-ple-runner-options.md`。

#### 変更内容

テンソルなし先頭GGUFへの対応を公式実装へ統一。廃止されたPersistenceDescriptorの代わりに、外部PLEの指紋をcomponent-cacheの互換性識別へ直接渡す。API 0.2系列へ既存の公開追加契約を記録し、旧0.1記録は保持した。Q5_0、IQ・Q2_0、浮動小数点upload、GDN・HC・SwiGLU、固定順序WMMA、日本語ASRの実装は維持した。

#### 確認できた結果

既存カスタム84ファイルを比較し、上流と重なる10ファイルを個別確認、残る74ファイルは更新前と一致。共有整形検査893ファイル、拡張API検査（0.2.8、SURFACE 122パス）は合格。ビルド・テストの最終結果は今回の記録を参照。演算カーネルは変更しておらず、このコミット単独の性能改善は測定していない。

### `d1bc7691` — GGUF回帰テストの重複整理

#### 意図

上流が追加した同じfixtureへ検証を集約し、独自テストの維持負担を減らす。

#### 変更したファイル

`tests/core/gguf_reader_test.cpp`、`docs/models/qwen3.8-flash-next/README.md`。

#### 変更内容

独自TestMetadataOnlyShardを削除し、公式TestTensorlessFirstShardへ切れたメタデータの拒否・後続shardから開く確認を追加。外部PLEの説明には実装済みQ8_0形式を追記した。

#### 確認できた結果

共有整形検査、本体ビルド、共有CPU回帰102件、追加CPU fixture 4件が合格。GGUF読み込みとPLEキャッシュ識別も合格した。候補イメージは`gufo:v0.31.0-reviewed-d1bc7691-rocm101`で、版表示と同梱バイナリのSHA-256一致を確認。GPU推論・ベンチ・通常版切り替えは、この追記時点では未実施。通常設定と3コンテナの構成・起動時刻は開始前と一致した。

## 付録A. コミットごとの全変更ファイル

Gitの差分から取得した一覧。追加・変更・削除を当該コミット時点で表示する。リンクは現在の採用中ソースを指すため、その後のコミットによる変更も含む。現在存在しないファイルはリンクにせず、履歴上のファイルとして明示する。特定コミット時点の内容は、ソースディレクトリで`git show <コミット>:<パス>`により確認できる。

<details>
<summary>386dec30 — 第1段階：既存IQ演算を共通入口から呼べるようにする（7ファイル）</summary>

完全なコミットID：`386dec309da9368d3bf8cb0f74f3888627d29b04`

| 操作 | ファイル |
| --- | --- |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/mmq/mmvq.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/mmvq.hip.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.h](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.h) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.hip.cpp) |
| 変更 | [tests/models/qwen38_flash_next/CMakeLists.txt](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/CMakeLists.txt) |
| 追加 | [tests/models/qwen38_flash_next/fixtures/iq_expert.bin](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/fixtures/iq_expert.bin) |
| 追加 | [tests/models/qwen38_flash_next/fixtures/iq_expert.json](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/fixtures/iq_expert.json) |
| 追加 | [tests/models/qwen38_flash_next/iq_expert_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/iq_expert_ops_test.cpp) |

</details>

<details>
<summary>eb4409d6 — 第2段階：UD-IQ4_XSの重み・CPU参照・実行経路を接続（5ファイル）</summary>

完全なコミットID：`eb4409d6ba64e3fe18a090780a481f55c6f3f436`

| 操作 | ファイル |
| --- | --- |
| 変更 | [src/models/qwen38_flash_next/cpu_ops.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/cpu_ops.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) |
| 変更 | [src/models/qwen38_flash_next/weights.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/weights.cpp) |
| 変更 | [tests/models/qwen38_flash_next/CMakeLists.txt](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/CMakeLists.txt) |
| 追加 | [tests/models/qwen38_flash_next/iq_rows_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/iq_rows_test.cpp) |

</details>

<details>
<summary>2a6beddc — 第2段階の補完：Q6_Kの出力層を既存Qwen演算へ接続（5ファイル）</summary>

完全なコミットID：`2a6beddcca7160406694a52517d5fdbf9abbae93`

| 操作 | ファイル |
| --- | --- |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/batch.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/batch.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/device_model.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/device_model.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) |
| 変更 | [tests/models/qwen38_flash_next/iq_expert_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/iq_expert_ops_test.cpp) |
| 変更 | [tests/models/qwen38_flash_next/iq_rows_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/iq_rows_test.cpp) |

</details>

<details>
<summary>2b1143b6 — 外部BF16 n-gramを指定する既存実装を独立保存（10ファイル）</summary>

完全なコミットID：`2b1143b69c27f01542680d22e9885858d02a9acd`

| 操作 | ファイル |
| --- | --- |
| 変更 | docs/SERVER.md（ローカル保管：SERVER.md） |
| 変更 | docs/models/qwen3.8-flash-next/README.md（ローカル保管：models/qwen3.8-flash-next/README.md） |
| 変更 | [src/cli/serve/inference_backend.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/cli/serve/inference_backend.cpp) |
| 変更 | [src/cli/serve/inference_backend.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/cli/serve/inference_backend.hpp) |
| 変更 | [src/cli/serve/serve.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/cli/serve/serve.cpp) |
| 変更 | [src/models/qwen38_flash_next/engine.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/engine.cpp) |
| 変更 | [src/models/qwen38_flash_next/engine.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/engine.hpp) |
| 変更 | [src/models/qwen38_flash_next/ngram.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/ngram.cpp) |
| 変更 | [src/models/qwen38_flash_next/ngram.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/ngram.hpp) |
| 変更 | [tests/models/qwen38_flash_next/ngram_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/ngram_test.cpp) |

</details>

<details>
<summary>59753e6a — IQ2_S・IQ3_XXS・Q2_0の共通演算を復元（20ファイル）</summary>

完全なコミットID：`59753e6a4750edd7a8e081b1ebc5f30aabc25991`

| 操作 | ファイル |
| --- | --- |
| 変更 | [src/core/gguf_reader.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/core/gguf_reader.hpp) |
| 変更 | [src/core/quant/ggml_dequant.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/core/quant/ggml_dequant.cpp) |
| 変更 | [src/core/quant/ggml_dequant.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/core/quant/ggml_dequant.hpp) |
| 追加 | src/core/quant/iq_codebooks.hpp（ローカル保管：../src/core/quant/iq_codebooks.hpp） |
| 変更 | src/models/qwen/gemm_route.hpp（ローカル保管：../src/models/qwen/gemm_route.hpp） |
| 変更 | [src/models/qwen38_flash_next/cpu_ops.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/cpu_ops.cpp) |
| 変更 | [src/models/qwen38_flash_next/cpu_ops.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/cpu_ops.hpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/mmq/VENDOR.md](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/VENDOR.md) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/mmq/common.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/common.hpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/mmq/mmvq.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/mmvq.hip.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_ggml_stubs.h](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_ggml_stubs.h) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.h](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.h) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.hip.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/mmq/vecdotq.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/vecdotq.hpp) |
| 変更 | [src/models/qwen38_flash_next/weights.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/weights.cpp) |
| 変更 | [tests/models/qwen38_flash_next/CMakeLists.txt](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/CMakeLists.txt) |
| 追加 | [tests/models/qwen38_flash_next/fixtures/generate_native_quant.py](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/fixtures/generate_native_quant.py) |
| 追加 | [tests/models/qwen38_flash_next/fixtures/native_quant.bin](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/fixtures/native_quant.bin) |
| 追加 | [tests/models/qwen38_flash_next/fixtures/native_quant.json](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/fixtures/native_quant.json) |
| 追加 | [tests/models/qwen38_flash_next/native_quant_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/native_quant_test.cpp) |

</details>

<details>
<summary>8e02a42a — 小さい量子化行列の計算をまとめて実行（8ファイル）</summary>

完全なコミットID：`8e02a42ac08655d60726f10a02db11a746564836`

| 操作 | ファイル |
| --- | --- |
| 変更 | src/models/qwen/hip/kernels/prefill_quant_gemm.hip（ローカル保管：../src/models/qwen/hip/kernels/prefill_quant_gemm.hip） |
| 変更 | src/models/qwen/hip/kernels/small_batch_gemm.hpp（ローカル保管：../src/models/qwen/hip/kernels/small_batch_gemm.hpp） |
| 変更 | src/models/qwen/hip/ops/gemm.hpp（ローカル保管：../src/models/qwen/hip/ops/gemm.hpp） |
| 変更 | src/models/qwen38_flash_next/CMakeLists.txt（ローカル保管：../src/models/qwen38_flash_next/CMakeLists.txt） |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/batch.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/batch.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) |
| 追加 | [src/models/qwen38_flash_next/kernels/rocm/native_quant.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/native_quant.cpp) |
| 追加 | [src/models/qwen38_flash_next/kernels/rocm/native_quant.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/native_quant.hpp) |

</details>

<details>
<summary>7430e160 — IQエキスパートを既存のWMMA Prefillへ接続（6ファイル）</summary>

完全なコミットID：`7430e16009c89f88e30b7581799f825c91e5d4b8`

| 操作 | ファイル |
| --- | --- |
| 追加 | src/models/qwen/hip/kernels/iq4_half.hpp（ローカル保管：../src/models/qwen/hip/kernels/iq4_half.hpp） |
| 変更 | src/models/qwen/hip/kernels/prefill_fp16.hip（ローカル保管：../src/models/qwen/hip/kernels/prefill_fp16.hip） |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) |
| 追加 | [src/models/qwen38_flash_next/kernels/rocm/iq_prefill.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/iq_prefill.hpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/kernels.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hpp) |

</details>

<details>
<summary>39ca0879 — GDNのFP16出力を次の投影で再利用（2ファイル）</summary>

完全なコミットID：`39ca0879e33eedee0a7044902bcf41c48a13307d`

| 操作 | ファイル |
| --- | --- |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) |
| 変更 | [tests/models/qwen38_flash_next/gdn_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/gdn_ops_test.cpp) |

</details>

<details>
<summary>86434907 — HCのFP32結果と再利用用FP16を同時生成（4ファイル）</summary>

完全なコミットID：`864349073764822a733ef544e78096934c3d3e14`

| 操作 | ファイル |
| --- | --- |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/kernels.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hpp) |
| 変更 | [tests/models/qwen38_flash_next/hc_mix_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/hc_mix_ops_test.cpp) |

</details>

<details>
<summary>58516f7d — 共有エキスパートのSwiGLUとFP16変換を融合（旧Aの採用版）（5ファイル）</summary>

完全なコミットID：`58516f7da2aba6ff288e2c9d0f667d3fca938b4b`

| 操作 | ファイル |
| --- | --- |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/kernels.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hpp) |
| 変更 | [tests/models/qwen38_flash_next/CMakeLists.txt](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/CMakeLists.txt) |
| 追加 | [tests/models/qwen38_flash_next/swiglu_half_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/swiglu_half_test.cpp) |

</details>

<details>
<summary>a236cb30 — BF16のHC・PLEを計算順序が一定のWMMAへ接続（4ファイル）</summary>

完全なコミットID：`a236cb3084fd6e5da1050eb8b8d7d5f208f51fbe`

| 操作 | ファイル |
| --- | --- |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/blaslt.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/blaslt.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/kernels.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hpp) |
| 変更 | [tests/models/qwen38_flash_next/blaslt_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/blaslt_ops_test.cpp) |

</details>

<details>
<summary>5b8da407 — 圧縮IQ演算と高速経路を独立参照で検証（4ファイル）</summary>

完全なコミットID：`5b8da407d2bdf83a93c18126525ce0a69fdb7b58`

| 操作 | ファイル |
| --- | --- |
| 変更 | [tests/models/qwen38_flash_next/CMakeLists.txt](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/CMakeLists.txt) |
| 追加 | [tests/models/qwen38_flash_next/iq_expert_test.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/iq_expert_test.hpp) |
| 追加 | [tests/models/qwen38_flash_next/native_expert_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/native_expert_ops_test.cpp) |
| 変更 | [tests/models/qwen38_flash_next/routed_wmma_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/routed_wmma_ops_test.cpp) |

</details>

<details>
<summary>095a6266 — 元Qwen系のOrca IQ4_XSを共通経路へ接続（10ファイル）</summary>

完全なコミットID：`095a6266b4414da13b58bf51f424c0fd4b66521d`

| 操作 | ファイル |
| --- | --- |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/batch.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/batch.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/device_model.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/device_model.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/kernels.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hpp) |
| 変更 | [src/models/qwen38_flash_next/weights.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/weights.cpp) |
| 変更 | [tests/models/qwen38_flash_next/CMakeLists.txt](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/CMakeLists.txt) |
| 変更 | [tests/models/qwen38_flash_next/hc_mix_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/hc_mix_ops_test.cpp) |
| 追加 | `tests/models/qwen38_flash_next/iq4xs_bind_probe.cpp`（現HEADには存在しない履歴上のファイル） |
| 追加 | tests/models/qwen38_flash_next/native_iq4_ops_test.cpp（ローカル保管：../tests/models/qwen38_flash_next/native_iq4_ops_test.cpp） |

</details>

<details>
<summary>fbf0a9f8 — ISTA GSQ-RCOの混在形式と浮動小数点重みの差へ対応（14ファイル）</summary>

完全なコミットID：`fbf0a9f889464dace805e446219bb8e34969dca9`

| 操作 | ファイル |
| --- | --- |
| 変更 | src/models/qwen38_flash_next/CMakeLists.txt（ローカル保管：../src/models/qwen38_flash_next/CMakeLists.txt） |
| 追加 | [src/models/qwen38_flash_next/float_weights.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/float_weights.cpp) |
| 追加 | [src/models/qwen38_flash_next/float_weights.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/float_weights.hpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/device_model.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/device_model.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) |
| 追加 | [src/models/qwen38_flash_next/kernels/rocm/float_upload.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/float_upload.hpp) |
| 変更 | [src/models/qwen38_flash_next/reference.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/reference.cpp) |
| 変更 | [src/models/qwen38_flash_next/weights.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/weights.cpp) |
| 変更 | [src/models/qwen38_flash_next/weights.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/weights.hpp) |
| 変更 | [tests/models/qwen38_flash_next/CMakeLists.txt](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/CMakeLists.txt) |
| 追加 | [tests/models/qwen38_flash_next/float_upload_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/float_upload_test.cpp) |
| 追加 | [tests/models/qwen38_flash_next/float_weights_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/float_weights_test.cpp) |
| 削除 | `tests/models/qwen38_flash_next/iq4xs_bind_probe.cpp`（現HEADには存在しない履歴上のファイル） |
| 追加 | [tests/models/qwen38_flash_next/weights_probe.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/weights_probe.cpp) |

</details>

<details>
<summary>0090201a — 公式v0.28.1の統合（110ファイル）</summary>

完全なコミットID：`0090201a84eaa5a8b685730f51af43bf3636df0b`。第1親との比較。

| 操作 | ファイル |
| --- | --- |
| M | .gitattributes（ローカル保管：../.gitattributes） |
| M | .github/workflows/windows-build.yml（ローカル保管：../.github/workflows/windows-build.yml） |
| M | .release-please-manifest.json（ローカル保管：../.release-please-manifest.json） |
| M | CHANGELOG.md（ローカル保管：../CHANGELOG.md） |
| M | CMakeLists.txt（ローカル保管：../CMakeLists.txt） |
| M | README.md（ローカル保管：../README.md） |
| M | THIRD_PARTY_NOTICES.md（ローカル保管：../THIRD_PARTY_NOTICES.md） |
| M | cmake/Checks.cmake（ローカル保管：../cmake/Checks.cmake） |
| M | docs/SERVER.md（ローカル保管：SERVER.md） |
| M | docs/models/embeddinggemma-2/BENCHMARKS.md（ローカル保管：models/embeddinggemma-2/BENCHMARKS.md） |
| M | docs/models/embeddinggemma-2/QUALITY.md（ローカル保管：models/embeddinggemma-2/QUALITY.md） |
| M | docs/models/embeddinggemma-2/README.md（ローカル保管：models/embeddinggemma-2/README.md） |
| A | docs/models/embeddinggemma-2/artifacts/gfx1151-2026-10-29-video-ci.json（ローカル保管：models/embeddinggemma-2/artifacts/gfx1151-2026-10-29-video-ci.json） |
| A | docs/models/embeddinggemma-2/artifacts/gfx1151-2026-10-29-video.json（ローカル保管：models/embeddinggemma-2/artifacts/gfx1151-2026-10-29-video.json） |
| M | docs/models/qwen3.8-flash-next/EXPERIMENTS.md（ローカル保管：models/qwen3.8-flash-next/EXPERIMENTS.md） |
| M | flake.nix（ローカル保管：../flake.nix） |
| M | [src/cli/serve/embedding_service.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/cli/serve/embedding_service.cpp) |
| M | [src/cli/serve/embedding_service.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/cli/serve/embedding_service.hpp) |
| M | [src/cli/serve/embeddings_api.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/cli/serve/embeddings_api.cpp) |
| M | [src/cli/serve/http_server.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/cli/serve/http_server.cpp) |
| M | [src/cli/serve/openai_chat.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/cli/serve/openai_chat.cpp) |
| M | [src/cli/serve/openai_chat.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/cli/serve/openai_chat.hpp) |
| M | [src/core/image.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/core/image.cpp) |
| M | [src/core/image.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/core/image.hpp) |
| M | [src/core/platform/CMakeLists.txt](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/core/platform/CMakeLists.txt) |
| A | [src/core/platform/library.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/core/platform/library.cpp) |
| A | [src/core/platform/library.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/core/platform/library.hpp) |
| M | src/models/embedding_gemma2/CMakeLists.txt（ローカル保管：../src/models/embedding_gemma2/CMakeLists.txt） |
| M | [src/models/embedding_gemma2/batch_scheduler.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/embedding_gemma2/batch_scheduler.hpp) |
| M | [src/models/embedding_gemma2/config.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/embedding_gemma2/config.cpp) |
| M | [src/models/embedding_gemma2/config.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/embedding_gemma2/config.hpp) |
| M | [src/models/embedding_gemma2/hip/encoder_runtime.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/embedding_gemma2/hip/encoder_runtime.cpp) |
| M | [src/models/embedding_gemma2/image_processor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/embedding_gemma2/image_processor.cpp) |
| M | [src/models/embedding_gemma2/image_processor.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/embedding_gemma2/image_processor.hpp) |
| A | [src/models/embedding_gemma2/video_processor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/embedding_gemma2/video_processor.cpp) |
| A | [src/models/embedding_gemma2/video_processor.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/embedding_gemma2/video_processor.hpp) |
| M | src/models/qwen/CMakeLists.txt（ローカル保管：../src/models/qwen/CMakeLists.txt） |
| M | [src/models/qwen/hip/batched_decode.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen/hip/batched_decode.cpp) |
| M | src/models/qwen/hip/dflash/executor.cpp（ローカル保管：../src/models/qwen/hip/dflash/executor.cpp） |
| M | src/models/qwen/hip/kernels/prefill_quant_gemm.hpp（ローカル保管：../src/models/qwen/hip/kernels/prefill_quant_gemm.hpp） |
| A | src/models/qwen/hip/kernels/verify_gemm_27b.hip（ローカル保管：../src/models/qwen/hip/kernels/verify_gemm_27b.hip） |
| A | src/models/qwen/hip/kernels/verify_gemm_27b.hpp（ローカル保管：../src/models/qwen/hip/kernels/verify_gemm_27b.hpp） |
| M | [src/models/qwen38_flash_next/engine.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/engine.cpp) |
| M | [src/models/qwen38_flash_next/engine.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/engine.hpp) |
| M | [src/models/qwen38_flash_next/kernels/rocm/batch.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/batch.cpp) |
| M | [src/models/qwen38_flash_next/kernels/rocm/blaslt.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/blaslt.cpp) |
| M | [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) |
| M | [src/models/qwen38_flash_next/kernels/rocm/executor.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.hpp) |
| M | [src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp) |
| M | [src/models/qwen38_flash_next/kernels/rocm/kernels.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hpp) |
| M | [src/models/qwen38_flash_next/kernels/rocm/w8a8_wave64.hip](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/w8a8_wave64.hip) |
| M | [src/models/qwen38_flash_next/mtp_policy.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/mtp_policy.hpp) |
| M | src/models/qwen_image_21/CMakeLists.txt（ローカル保管：../src/models/qwen_image_21/CMakeLists.txt） |
| M | [src/models/qwen_image_21/hip/kernels.hip](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen_image_21/hip/kernels.hip) |
| M | [src/models/qwen_image_21/hip/runtime.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen_image_21/hip/runtime.hpp) |
| M | [src/models/qwen_image_21/weights.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen_image_21/weights.cpp) |
| M | [src/models/qwen_image_21/weights.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen_image_21/weights.hpp) |
| M | [tests/cli/http_server_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/cli/http_server_test.cpp) |
| M | [tests/cli/http_server_test_support.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/cli/http_server_test_support.hpp) |
| A | [tests/core/platform_library_fixture.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/core/platform_library_fixture.cpp) |
| A | [tests/core/platform_library_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/core/platform_library_test.cpp) |
| A | [tests/fixtures/embedding_gemma2/image_boundaries.json](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/fixtures/embedding_gemma2/image_boundaries.json) |
| M | [tests/fixtures/embedding_gemma2/image_golden.json](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/fixtures/embedding_gemma2/image_golden.json) |
| A | [tests/fixtures/embedding_gemma2/video/boundary.mp4](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/fixtures/embedding_gemma2/video/boundary.mp4) |
| A | [tests/fixtures/embedding_gemma2/video/fractional.mp4](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/fixtures/embedding_gemma2/video/fractional.mp4) |
| A | [tests/fixtures/embedding_gemma2/video/overduration.mp4](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/fixtures/embedding_gemma2/video/overduration.mp4) |
| A | [tests/fixtures/embedding_gemma2/video/overflow.mp4](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/fixtures/embedding_gemma2/video/overflow.mp4) |
| A | [tests/fixtures/embedding_gemma2/video/overframes.mp4](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/fixtures/embedding_gemma2/video/overframes.mp4) |
| A | [tests/fixtures/embedding_gemma2/video/oversized.mp4](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/fixtures/embedding_gemma2/video/oversized.mp4) |
| A | [tests/fixtures/embedding_gemma2/video/portrait.webm](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/fixtures/embedding_gemma2/video/portrait.webm) |
| A | [tests/fixtures/embedding_gemma2/video/short.mp4](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/fixtures/embedding_gemma2/video/short.mp4) |
| A | [tests/fixtures/embedding_gemma2/video/with_audio.mp4](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/fixtures/embedding_gemma2/video/with_audio.mp4) |
| A | [tests/fixtures/embedding_gemma2/video_golden.json](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/fixtures/embedding_gemma2/video_golden.json) |
| M | [tests/functional/README.md](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/functional/README.md) |
| M | [tests/functional/devlab_plan.py](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/functional/devlab_plan.py) |
| M | [tests/functional/devlab_run.py](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/functional/devlab_run.py) |
| M | [tests/functional/embedding_images.py](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/functional/embedding_images.py) |
| A | [tests/functional/embedding_video.py](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/functional/embedding_video.py) |
| M | [tests/functional/impact.json](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/functional/impact.json) |
| M | [tests/functional/metrics.py](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/functional/metrics.py) |
| M | [tests/functional/model_services.py](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/functional/model_services.py) |
| A | [tests/functional/openai_count_tokens.py](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/functional/openai_count_tokens.py) |
| M | [tests/functional/openai_sdk.py](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/functional/openai_sdk.py) |
| M | [tests/functional/run.py](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/functional/run.py) |
| M | [tests/functional/test_devlab.py](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/functional/test_devlab.py) |
| M | [tests/functional/test_runner.py](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/functional/test_runner.py) |
| M | [tests/models/embedding_gemma2/CMakeLists.txt](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/embedding_gemma2/CMakeLists.txt) |
| M | [tests/models/embedding_gemma2/config_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/embedding_gemma2/config_test.cpp) |
| M | [tests/models/embedding_gemma2/embeddings_api_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/embedding_gemma2/embeddings_api_test.cpp) |
| M | [tests/models/embedding_gemma2/image_golden_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/embedding_gemma2/image_golden_test.cpp) |
| A | [tests/models/embedding_gemma2/video_processor_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/embedding_gemma2/video_processor_test.cpp) |
| M | [tests/models/qwen38_flash_next/CMakeLists.txt](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/CMakeLists.txt) |
| M | [tests/models/qwen38_flash_next/blaslt_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/blaslt_ops_test.cpp) |
| M | [tests/models/qwen38_flash_next/gdn_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/gdn_ops_test.cpp) |
| M | [tests/models/qwen38_flash_next/hc_mix_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/hc_mix_ops_test.cpp) |
| A | [tests/models/qwen38_flash_next/hc_route_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/hc_route_ops_test.cpp) |
| M | [tests/models/qwen38_flash_next/projection_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/projection_ops_test.cpp) |
| M | [tests/models/qwen38_flash_next/routed_wmma_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/routed_wmma_ops_test.cpp) |
| A | [tests/models/qwen_image_21/weights_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen_image_21/weights_test.cpp) |
| M | [tests/tools/windows_bundle_test.py](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/tools/windows_bundle_test.py) |
| M | [tools/README.md](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tools/README.md) |
| A | [tools/models/embedding_gemma2/reference_image_boundaries.py](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tools/models/embedding_gemma2/reference_image_boundaries.py) |
| M | [tools/models/embedding_gemma2/reference_images.py](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tools/models/embedding_gemma2/reference_images.py) |
| A | [tools/models/embedding_gemma2/reference_video.py](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tools/models/embedding_gemma2/reference_video.py) |
| A | [tools/qwen27b/verify_sweep/README.md](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tools/qwen27b/verify_sweep/README.md) |
| A | [tools/qwen27b/verify_sweep/sweep.hip](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tools/qwen27b/verify_sweep/sweep.hip) |
| A | [tools/qwen27b/verify_sweep/sweep.py](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tools/qwen27b/verify_sweep/sweep.py) |
| M | [tools/windows/check_bundle.py](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tools/windows/check_bundle.py) |
| M | vcpkg.json（ローカル保管：../vcpkg.json） |
| M | version.txt（ローカル保管：../version.txt） |

</details>

<details>
<summary>8f4a7e77 — Attention密／疎境界修正（5ファイル）</summary>

完全なコミットID：`8f4a7e77f91c466bfe5fbf0703385c9a17b6f4c2`。第1親との比較。

| 操作 | ファイル |
| --- | --- |
| M | docs/models/qwen3.8-flash-next/QUALITY.md（ローカル保管：models/qwen3.8-flash-next/QUALITY.md） |
| M | [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) |
| M | [src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp) |
| M | [src/models/qwen38_flash_next/kernels/rocm/kernels.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hpp) |
| M | [tests/models/qwen38_flash_next/attention_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/attention_ops_test.cpp) |

</details>

<details>
<summary>4336a8d5 — Qwen3-ASR-1.7B-JA対応（ASR別ブランチ・8ファイル）</summary>

完全なコミットID：`4336a8d5cac97a3d4ce4d21d31147d12fbce6870`。

| 操作 | ファイル |
| --- | --- |
| 変更 | docs/models/qwen3-asr/README.md（ローカル保管：models/qwen3-asr/README.md） |
| 変更 | src/models/qwen3_asr/CMakeLists.txt（ローカル保管：../src/models/qwen3_asr/CMakeLists.txt） |
| 変更 | [src/models/qwen3_asr/config.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen3_asr/config.cpp) |
| 変更 | [src/models/qwen3_asr/config.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen3_asr/config.hpp) |
| 変更 | [src/models/qwen3_asr/loader.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen3_asr/loader.cpp) |
| 追加 | [src/models/qwen3_asr/tests/checkpoint_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen3_asr/tests/checkpoint_test.cpp) |
| 変更 | [src/models/qwen3_asr/tests/config_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen3_asr/tests/config_test.cpp) |
| 変更 | [src/models/qwen3_asr/tests/loader_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen3_asr/tests/loader_test.cpp) |

</details>

## 付録B. 記録の区切り

- 2026-11-01：Qwen3-ASR-1.7B-JAを取得し、別Gufoサーバーへ対応。Huihuiと同時動作を確認。Hermesの接続設定は未変更。

- 2026-10-30：Huihui最小対応3コミットとROCm 10.1の通常動作確認。
- 2026-10-31：既存外部PLE対応のコミット化、旧採用済み高速化の移植、共通形式・Orca・ISTA対応の整理、CPU/GPU演算と読み込み定義の検証。
- 2026-11-01：Huihuiの128K MTP文章を外部BF16あり／なし各1回測定し、その後、速度改善版を通常運用へ採用。

旧版で不採用だったB1・B2・Cの実験や診断用TrunkTraceは、今回の高速化移植に含めていない。新規モデルのダウンロードは、この高速化移植と今回の文書作成では行っていない。

この文書は日本語の検証・説明資料として、開発用リポジトリの `docs/gufo-commit-guide.ja.md` に保存する。ベンチ記録は `docs/benchmark` に置く。上流提出用ソースの英語方針は維持する。変更内容の根拠は上記Gitコミットとリンク先の保存済みログ・JSON。

<details>
<summary>1f2fe101 — メタデータだけのGGUFの読み込み（2ファイル）</summary>

完全なコミットID：`1f2fe1019a3694d3c5c23cca7173685c44f050f1`

| 操作 | ファイル |
| --- | --- |
| 変更 | [src/core/gguf_reader.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/core/gguf_reader.cpp) |
| 変更 | [tests/core/gguf_reader_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/core/gguf_reader_test.cpp) |

</details>

<details>
<summary>fe85127b — Q5_0 expertの既存MMQへの接続（9ファイル）</summary>

完全なコミットID：`fe85127b5012b43760900ced7f5fca0bfaa353f9`

| 操作 | ファイル |
| --- | --- |
| 変更 | [src/models/qwen38_flash_next/cpu_ops.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/cpu_ops.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/executor.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/mmq/mmvq.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/mmvq.hip.cpp) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.h](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.h) |
| 変更 | [src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.hip.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.hip.cpp) |
| 変更 | [src/models/qwen38_flash_next/weights.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/weights.cpp) |
| 変更 | [tests/models/qwen38_flash_next/iq_expert_test.hpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/iq_expert_test.hpp) |
| 変更 | [tests/models/qwen38_flash_next/native_expert_ops_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/native_expert_ops_test.cpp) |
| 変更 | [tests/models/qwen38_flash_next/native_quant_test.cpp](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/native_quant_test.cpp) |

</details>

## 2026-11-08：v0.31.0候補の一括整理

以下は候補への変更であり、通常版への反映や全体ベンチの完了を意味しない。

### `330b9359` — GGUF独自試験の廃止

- 意図：公式へ統一したtensorless shard処理に対応し、残っていた独自試験も廃止する。
- 変更したファイル：`tests/core/gguf_reader_test.cpp`。
- 変更内容：独自の切り詰めメタデータと途中shard開始の追加試験を除去。reader本体と試験は公式v0.31.0と差分なし。
- 確認できた結果：公式gguf_reader_testのビルド・実行合格（0.81秒）、全C++書式検査合格。記録は`docs/local/updates/v0310-20261108/implementation/gguf-check.*`。

### `c4fa163c` — 量子化Denseの名称整理

- 意図：複数形式を扱う共通入口をIQ4専用と誤解しない名前にする。
- 変更したファイル：Flash-Next ROCmの`native_quant.hpp`、`native_quant.cpp`、`device_model.cpp`、`executor.cpp`、`batch.cpp`と、同モデル試験の`CMakeLists.txt`、`native_quant_ops_test.cpp`（旧名から移動）。
- 変更内容：`NativeQuantDense`、`NativeQuantDenseF16`、`IsNativeQuantDense`へ統一。試験名・現行コメントも整合。対応形式・数値演算・精度選択は変更しない。
- 確認できた結果：本体・対象GPU試験ビルド合格、既存GPU演算試験合格（10.62秒）、全C++書式検査合格。速度改善としては計上しない。記録は同実施フォルダの`rename-*`。

### `c8047364` — 未使用IQコード表の削除

- 意図：共通形式の重複を除く。
- 変更したファイル：`src/core/quant/iq_codebooks.hpp`（削除）。
- 変更内容：参照も公開SURFACE登録もない旧コード表を除去。現用の公式復号とGPU用固定コード表は維持。
- 確認できた結果：本体と量子化試験のビルド、CPU量子化試験、拡張API検査合格。実行経路の変更はなく、速度改善として計上しない。

### `b2513b6c` — 型対応とブロック寸法の共通化

- 意図：同じ形式の扱いが接続箇所ごとに分かれないようにする。
- 変更したファイル：Flash-Next ROCmの`batch.cpp`、`executor.cpp`、`native_quant.hpp`、`kernels.hip.cpp`。
- 変更内容：小行列／埋め込みの重複型対応を共通入口へ集約。routed GEMMの行サイズを既存ブロック型と要素数定義へ接続。演算と既存dispatch条件は維持。
- 確認できた結果：本体ビルド、CPU量子化、Dense GPU試験（10.36秒）、routed WMMA GPU試験（191.52秒）、拡張API・全C++書式検査合格。記録は実施フォルダの`quant-shared-*`。この整理だけの速度改善は主張しない。

### `5fb5f3ce` — 浮動小数点転送の直接復号

- 意図：FP32出力時に一行バッファを経由するコピーを省く。
- 変更したファイル：Flash-Next ROCmの`float_upload.hpp`。
- 変更内容：公式復号を転送用pinnedバッファへ直接出力。FP16向け一行作業領域と明示丸め、有限値検査、分割ReadAt・同期転送・ダイジェストを維持。公式WeightUploadは生バイトの非同期転送で契約が異なるため置換しない。実利用先のない他モデル向け抽象化は追加しない。
- 確認できた結果：CPU浮動小数点試験とGPU転送試験（0.57秒）、本体ビルド・書式検査合格。符号付きゼロ、8MiB境界をまたぐ端数転送、非有限値拒否、scratch上限を既存試験で確認。ロード全体の時間短縮は未測定。記録は`float-*`。

### `8f38940f` — Q5_0 expertのWMMA接続

- 意図：Q5_0 downを持つ層でも既存のexpert WMMAを使えるようにする。
- 変更したファイル：Flash-Next ROCmの`executor.cpp`、`kernels.hip.cpp`、`kernels.hpp`、`routed_wmma_ops_test.cpp`。
- 変更内容：既存packedコード・high bits・scale領域とQ5の復号・WMMAを再利用。22バイトブロックを境界超過せず読み、符号をスケール前に処理。IQ系gate/up＋Q5_0 downをprefillで選択。128行も既存汎用タイルへ接続。
- 確認できた結果：独立CPU復号・FP64参照、端数、16/48/128タイル、IQ4_XS／IQ3_XXSとの実形状と分割幅・短い末尾のbit一致に合格（3.59秒）。初期の専用128行版は丸め境界で不一致となり不採用。失敗ログと差分を保持。実モデルの最終採用評価は後段。

### `84261f8f` — MMQ gate/up入力準備の共有

- 意図：WMMA以外で同一形状のgate/upが残る場合に、routingと量子化を一度にする。
- 変更したファイル：Flash-Next ROCmの`executor.cpp`、`mmq/qfn_mmq.h`、`mmq/qfn_mmq.hip.cpp`、`routed_wmma_ops_test.cpp`。
- 変更内容：Q4専用入口を形式引数のある`qfn_mmq_moe_pair`へ整理し、既存2重みテンプレートをQ5_K・Q8_0・IQ2_S・IQ3_XXS・IQ3_S・IQ4_XSでも利用。重み別の演算を維持。
- 確認できた結果：7形式で別々のMMQ呼出しと出力一致、既存native expert独立試験合格。通常IQ4_XSのprefillは先のWMMA対応を使うので、同じ効果を重複計上しない。

### 共有expert融合の評価結果（不採用）

既存Qwenの`TryLaunchBatchedDualQuantGEMMSwiGLUFp16`を640×2560のQ6_K／IQ4_XSで試験。入力を有限範囲へ合わせた試験で、512行時のFP16出力差は20／21要素。単体の繰り返し測定も既存分離処理0.160／0.146msに対し融合0.214／0.203msで、1〜512行の全ケースで遅かった。これは診断用GPU演算時間であり、モデル全体の速度ではない。丸め・性能双方の条件を満たさず、候補を通常処理へ接続しない。試験パッチ・実行バイナリ・失敗を含む全ログをローカルへ保存し、実験用のコードは配布ソースから除去した。

### 9ae20555 — 参照ツールのQwen共通ライブラリ接続

意図：独立CPU参照を使うツールの既存リンク不足を修正。`extension.cmake`で参照ライブラリにQwen vision（textを含む）を接続した。推論演算は変更せず、本体・CPU/GPU probeのビルド成功を確認。

### 8aa11092 — 互換性のある投影間で入力を再利用

意図：型・形状・処理段階・配置が一致する投影で、既存のF16/BF16入力バッファを再利用する。GDNの既存F16出力を対象量子化形式へ接続し、HCのFP32/Q8出力省略は後続全体と共有expertの専用バッファ利用を確認できる場合に限定。半精度バッファの形式・寿命情報を保持し、未知の後段は従来のFP32再構築を維持する。変更：executor / native_quantの宣言・実装4ファイル。対象native dense、HC、GDN、BF16演算試験、CPU・API・書式合格。全体数値・性能比較は継続中。

### f8538932 — 量子化経路の説明を現行実装へ合わせる

`mmq/VENDOR.md`のQ5_0漏れ、paired対応形式、独立試験の内訳を修正。説明中のモデル名による形状の限定も削除した。演算コードの変更なし。

### 7492ef24 — 不採用の量子化演算実験を記録

共有expert融合は遅延と丸め差、Q5_0専用128行は分割時のFP16丸め差により不採用。既存の数値条件を緩和せず、前者は従来処理、後者は独立試験に合格した汎用経路を保持。測定値と不採用理由をEXPERIMENTS.mdへ記録した。

### d91b1dad — 少量expert down処理の共通起動と入力順序

Q5_0・IQ4_NL・Q2_0のK=640、2〜640slotに、既存expert順Q8_1準備と4出力行の共通カーネルを接続。Q5_1/Q8_0と範囲外fallbackは維持。独立native expert試験、130条件の全出力ハッシュ一致、同じ生成文・処理量・draft採用数によるMTP HTTP4条件を確認。各処理段階の5%かつ10ms退行フラグなし。反復文Decodeは2条件で約6.25%/3.61%短縮。小演算単体の短い条件はcontrolも揺れており、単体速度の一律改善とはしない。共有書式検査合格。

- `d9ead92d` `perf(flash-next): skip unused Q5_0 token tiles`：既存Q5_1等と同じタイル末尾・レジスタ保持条件をQ5_0へ適用。独立演算・159位置の全logit完全一致。
- `0d984776` `perf(qwen): group small Q6_K projections without changing row arithmetic`：512×2560のQ6_Kを既存8行カーネルのまとめ起動へ接続。異なる行と大きな端数でも旧処理に完全一致。4 HTTP条件で出力一致、各phaseの退行フラグなし。16K反復Prefill 3278→2347ms。個人固有の名前・環境情報をコミットメッセージに含めず、実験記録も同じ機能コミットへ含めた。

### `96f1aa8c` — 量子化経路の検証範囲と速度差を記録

- 意図：機能合格、数値一致、独立参照、速度比較の限界を分けて残す。
- 変更ファイル：`docs/models/qwen3.8-flash-next/QUALITY.md`、`BENCHMARKS.md`。
- 変更内容：Q5_0の数値評価、再利用・Q6_Kの完全一致、同じ生成文での4条件比較、生成文が変わった文章Decodeの遅延を併記。
- 確認結果：AR／外部PLE MTP各11試験合格。内蔵PLE MTP全機能試験合格と最終対象履歴を確認。長文JSON停止は193→194msで前回の遅延が再現せず、出力・draft数一致。比較全体には中断後の出力差2件と速度判定不能7件が残る。Git差分検査合格。

### v0.31.0 最終反映 — `96f1aa8c`

目的別15コミットの一括作業を完了。外部Q8_0 PLE／内蔵PLEとも正式全項目を各3回測定、件数・HTTP原記録・集計値を照合。通常Gufoを内蔵PLE・従来パラメータで起動しready・応答4・MTP draft28/採用20・reasoning24を確認した。回答一致は全合格ではなく、単独AR/MTP35組と外部PLE C4の初回1応答を未合格として保持。C4の3種対象再測定では再現せず原因未確定。詳細は[最終実施記録](../../v0.31/investigations/local/updates/v0310-20261108/implementation/README.ja.md)と[正式レポート](../../v0.31/benchmarks/benchmark/reports/quant-refactor-20261108/report.ja.html)。

## GitHub Actions修正（2026-11-08）

- **意図**：CIで相対化されたソースパスからfixtureを探す不備と、Windows依存取得の一時的HTTP504を修正。
- **変更ファイル**：tests/models/qwen38_flash_next/CMakeLists.txt、native_quant_test.cpp、native_expert_ops_test.cpp、iq_expert_ops_test.cpp、.github/workflows/windows-build.yml、devlab-windows.yml。
- **変更内容**：f859c321（公開6e84429b）でCMakeからfixtureパスを明示。bbd27170（公開df9e702f）で依存取得を最大3回に限定して再試行し、継続失敗は維持。目的別・指定の英語メッセージでコミット。
- **確認結果**：書式893ファイル、3テストの対象コンパイル・リンク、CPUの3作業ディレクトリ実行、再試行9例、YAML読取りが合格。Nix開発環境全体の準備失敗は残し、完全なローカルビルド成功とは扱わない。GPU実行・速度測定は不要の範囲として未実施。Actions結果と公開記録は修正記録（ローカル保管：local/github/tor-o3/ci-fixes-20261108/README.ja.md）。

### Windows ccache取得経路の修正（2026-11-08）

- **意図**：Actionsでccacheの同じ配布URLが3回ともHTTP504となり、単純再試行だけでは解消しないことを確認。
- **変更ファイル**：.github/workflows/windows-build.yml。
- **変更内容**：ccf2539e（公開6951b576）。ccache 4.14.2をGitHub Release API経由で取得し、公式SHA-256を固定して展開前に検証。Ninjaと分離し、キャッシュ利用・必要なビルドを維持。
- **確認結果**：公式Release APIでの実ダウンロードとdigest一致、展開・成功/失敗の再試行・不一致検出の4例が合格。Windows拡張契約は最初のPRで成功。本体の速度測定は対象外。

### Windows vcpkg archive取得経路の修正（2026-11-08）

- **意図**：zlibのGitHub archive URLがvcpkgの再試行3回ともHTTP504となるため、同一内容を取得する別経路へ接続。
- **変更ファイル**：.github/workflows/windows-build.yml。
- **変更内容**：82ed253f（公開3082db80）。vcpkg標準のx-scriptでGitHub archiveをcodeloadへ変換。対象外の通常経路・固定SHA-512検証は維持。
- **確認結果**：Windows実フックによるzlib取得、baselineのSHA-512一致、対象外URLのcache missを確認。新しい独自依存管理やライブラリ版変更は行わない。

### GitHub Actions修正の完了

PR #1をToR-O3専用認証でmain（3082db80、本体82ed253fとtree一致）へfast-forward反映。mainのRepository checks 42477507129とWindows validation 42477507100が成功。CPUは162件合格・9件既定スキップ、Windowsは82件＋4件のCTestと7件のPythonテストが合格。Windows本体ジョブは1時間4分51秒。追加4件を含む独自52コミットの指定名義を照合済み。通常Gufoは稼働継続、検証用builderは停止。詳細はCI修正記録（ローカル保管：local/github/tor-o3/ci-fixes-20261108/README.ja.md）。

### READMEのfork説明追加

571c6b54（公開785c47b5）: `docs: describe fork variants and validation scope`。ユーザー指定のAbout this fork全文をタイトル直下へ追加。README.mdのみ13行追加、既存本文維持、差分検査合格。専用ToR-O3認証・指定メールでmainへ反映し、GitHubのREADMEと手元の本文一致を確認。

## 2026-11-10 Flash-NextのDFlash2対応（公開fork PR #2）

- **意図**。既存のQwen 27B用DFlash2を共用し、Flash-Nextで対応するGGUFを選択できるようにする。
- **変更したファイル**。Qwen共通の `src/families/qwen/dflash/`、Flash-Nextのモデル・ROCm・キャッシュ実装、および関連テストと機能テストの対象選択設定。
- **変更内容**。共通化を `7d204ec0`、Flash-Nextの接続を `136f56e4` に分離。採用した特徴量をGPU上で渡し、キャッシュ復元時は本体とドラフトの位置を合わせる。同時要求はGPU上で直列に処理する。
- **確認できた結果**。整形、拡張API、依存関係、機能テスト選択13件、キャッシュ構造とサンプリングの確認が通過。実機で指定のQ8_0と内蔵PLEの組み合わせを確認し、単独48測定、同時126応答が完了。英語の起動例と検証範囲は[PR #2](https://github.com/ToR-O3/gufo/pull/2)、公開記録はこちら（ローカル保管：local/github/tor-o3/dflash2-pr-20261110.json）。

## 2026-11-12 Gufo v0.33.0取り込み

- **意図**。本家v0.33.0を取り込み、重複するカスタムを本家経路へ寄せる。
- **変更したファイル**。Flash-Nextのモデル登録、ロード、プリフィルと推論。外部PLEのCLI設定と拡張API。関連する差分一覧は[統合記録](../../v0.33/investigations/local/updates/v0330-20261112/README.ja.md)。
- **変更内容**。`2ffecf82` で本家の層分割、プロバイダー設定、GGUF・WAV処理を採用。混合量子化、外部PLE、Flash-NextのDFlash2接続と採用済み最適化を維持。`e55f3b29` で旧拡張APIのPLE指定も受け付けるようにした。
- **確認できた結果**。初回統合はNixリリースビルド、CPU182件、実モデル3構成の起動・生成・ドラフト動作が合格。互換性差分は書式・拡張API・リリースビルドと3構成の実機起動が合格。通常設定で単独生成後の2セッションがv0.31.0の49.24 tok/sからv0.33.0の7.83 tok/sへ低下したため、旧版を通常起動した。正式測定と切り分けの詳細は[統合記録](../../v0.33/investigations/local/updates/v0330-20261112/README.ja.md)。

## 2026-11-25 Gufo v0.36.0取り込み（main反映・通常起動済み）

通常設定・モデルを保存し、別作業コピー `codex/v0360-custom-review` に公式v0.36.0を統合。元の未コミット作業は保持。PR・push・main更新は実施しない。

|コミット|変更と判定|
|---|---|
|a31be512|公式v0.36統合。状態メモリ・checkpoint・DFlash接続を新契約に合わせる。既存の追加量子化・混在HC・外部PLE・日本語ASR等は維持|
|cc5c90e2|拡張API 0.3系列に互換性記録を追加。旧PLE指定フィールドの既存契約を保持|
|deff7892|IQ paired expertを公式の新routing配置へ接続。既存groupingを再利用し、出力演算順を維持|
|759ca788|DFlash target特徴量を公式の一時prefill領域へ接続。幅の切替後も消費まで保持し、解放後は参照拒否|
|f6b37aef|新Q4_K短行経路は行数間の厳密一致に失敗したため不採用。draft prefixは従来MMVQを維持|
|ed389dc5|一時領域のDFlash特徴量寿命のGPU試験をCTestへ登録|
|5c8fbc08|Q6_K draft・IQ routing・PLE・DFlashの今回の数値検証と採否を品質文書に反映|
|28fd573f|復元中に保持台帳から外れても、有効なpayloadを持つ正常な復元を利用記録更新の例外で破棄しない。修正前に失敗するCPU回帰試験を追加|
|53a217cb|メモリ断片化だけで再利用可能なcheckpointを解放しない。host floor未満の実不足時は従来の貸し出しを保持。回帰試験と実モデル54要求で確認|

初回の対象CPU/GPU試験は合格（画像項目は対象外）。キャッシュ退行を追加修正した製品53a217cbで、54要求の再利用・出力一致とHTTP長文・キャッシュ11項目×3構成を再確認。定例69ジョブ×3回は全て完了、2時間23分18秒。通常DFlash2起動・実応答を確認済み。速度全項目の改善ではなく、公式の最終トークン反映に伴う準備済み入力TTFT増加と一部長文Prefill低下をレポートに残した。既存のGPUページ貸し出し・返却試験には変更前後共通の失敗があり、別に記録して合格扱いにしない。完全に置換できる独立カスタム機能はなく、一括廃止はしていない。詳細と未検証範囲は[取り込み記録](../../v0.36/investigations/local/updates/v0360-20261125/README.ja.md)。

2026-11-25の明示指示でmainへ53a217cbをfast-forward反映し、同じリビジョンのforkイメージを再起動。指定モデル・内蔵PLE・DFlash2の実応答確認済み。[main反映記録](../../v0.36/investigations/local/updates/v0360-main-20261125/README.ja.md)。

## 2026-11-26 Gufo v0.37.0取り込み（レビュー候補）
- **意図**。公式v0.37.0のWeightSource/Stages・HTTP更新に追従し、既存量子化・外部PLE・DFlash2・中間結果再利用を維持する。main反映・PR・通常版切替はレビュー後。
- **変更したファイル**。Flash-Nextのengine/runner/weight_source、ROCm executor/device_model/float_upload、Qwen/DFlash family接続、継続identityと対応試験。文書は拡張API説明・モデル品質記録。
- **変更内容**。

|コミット|内容|
|---|---|
|421e4342|公式v0.37統合。外部PLEをGGUF WeightSourceへ移し、Source/Stagesの所有関係を整合。公式Q4_K短行の丸め修正を採用し、decode幅の旧MMVQ固定を廃止。追加量子化・HC・GDN・DFlash2・ASR・キャッシュ修正は維持|
|1b21f08c|外部WeightSourceとDFlash2の直接接続をロード前に拒否。GGUFポインタ前提のbackendへ別形式の重みを渡さない|
|b0e2750b|外部Stagesへ渡すFP32中間値を省略しない。通常GGUFの省略最適化は維持。修正前に失敗する実モデル回帰確認を追加|

- **確認できた結果**。製品ビルド成功。対象CPU16件、初回GPU14件、b0e2750bの対象GPU4件が合格。外部StagesのFP32入力問題は修正前の失敗を保存し、修正後に同じ試験の成功と通常経路とのbitwise一致を確認。HTTP4構成は各17項目成功。新旧の対象履歴は回答・処理量一致。AR初回Prefillは2組で遅延、追加同条件確認では再現せず原因未確定として記録。全体の速度合格とは扱わない。正式3回ベンチとEXL3モデル実行は未実施。
- EXL3は別拡張0.1.3、専用safetensorsとMTPを使用。宣言API範囲はforkと適合するが、今回重みをダウンロード・実行していない。DFlash2対応と混同しない。
- 詳細・採否・未確認範囲は取り込み記録（ローカル保管：local/updates/v0370-20261126/README.ja.md）。

- 文書コミットbb1c3e7cまで保存。通常v0.36＋内蔵PLE＋DFlash2へ復帰し、image・binary・引数・設定の一致と実応答・draftを確認済み。候補の通常パラメータ起動も成功。PR・push・main反映は未実施。

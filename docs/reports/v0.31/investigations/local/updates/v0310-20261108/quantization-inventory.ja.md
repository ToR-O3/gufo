# Q系・IQ系・Q2_0対応の棚卸し

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

2026-11-08 JST。候補ソース`d1bc7691`と公式v0.31.0の差分を基準に、Unsloth v3.1系 IQ4_XS対応まで含めて整理した。今回の作業は読み取り調査と文書化のみ。コード変更・ビルド・試験・ベンチ・通常反映は実施していない。

既存のモデル名別・コミット別分類は導入の経緯を示す。現在の実装は、同じ形式を共通の入口へ渡しており、モデルごとの別カーネルではない。本表はFlash-Nextの役割別対応を対象とし、coreで復号できる全形式がモデルの全箇所で使えるという意味ではない。

## 1. 機能を分けて数える

| 層 | 役割 | 主な実装 |
|---|---|---|
| 共通形式 | 型番号、ブロック寸法、行サイズ、復号規則・CPU参照。GPUでも形式固有の解釈を揃える | `src/core/gguf_reader.hpp`、`src/core/quant/ggml_dequant.{hpp,cpp}`、GPUの形式別復号部 |
| モデルへの接続 | 各重みの役割・形状・形式を許可し、転送と実行へつなぐ | `weights.cpp`、`cpu_ops.cpp`、`device_model.cpp`、`executor.cpp` |
| 高速化 | 基本演算を土台に、ベクトル/MMQ/WMMAの選択、融合、入力・中間結果の再利用を行う | `kernels/rocm/mmq/`、`native_quant.*`、`iq_prefill.hpp`、`kernels.hip.cpp` |

検証と公開契約は3区分を横断する。CPU参照・GPU演算・モデル接続ごとの検証を区別し、`tests/models/qwen38_flash_next/`と拡張APIの既存契約を維持する。共通化のためにCPUとGPUで同じ関数を強制するのではなく、形式の意味と寸法を揃え、GPU固有の配置や演算順序は必要な場所に残す。

「Huihui対応」「Orca対応」「ISTA/SC117対応」「Unsloth v3.1対応」は上記を利用したモデル接続の履歴であり、これらの名称だけで機能を重複計上しない。浮動小数点upload・外部PLE・GGUF分割読み込みは隣接する機能として区別する。

## 2. 現在の形式別対応

| 形式 | 共通定義・CPU | 現在残るFlash-Nextの主なカスタム | routed WMMAでの位置づけ |
|---|---|---|---|
| Q4_K・Q5_K | 公式 | expertの基本経路は公式。通常Denseで利用する接続を追加 | 公式のgate/up経路を維持。downとの組み合わせ条件あり |
| Q5_1 | 公式。旧モデル独自CPU展開は共通化済み | GPU基本経路・4/8行設定は公式側の既存処理 | Q4_K/Q5_K gate/upに対するdownとして公式対応 |
| Q8_0 | 公式 | 基本演算は公式。外部PLEなどの用途側接続は別項目 | Q4_K/Q5_K gate/upに対するdownとして公式対応 |
| Q5_0 | 公式に復号・内積・MMQ部品あり | expertの許可、CPU参照入口、GPUの型別dispatch・MMQ入口を追加 | 現在は対象外。4/8行設定とWMMA接続は後日作業として合意済み |
| Q6_K | 公式にCPU復号・共通Qwen Dense演算あり | 許可した通常Dense・出力層から共通演算へ接続 | routed expertはGPU未対応。Q6_K埋め込みもGPU読み込み前に拒否 |
| IQ3_S | 公式CPU復号を使用 | expertの許可・既存GPUベクトル/MMQ演算への接続 | IQ側のprefill gate/upとして対応 |
| IQ4_XS | 公式CPU復号を使用 | expert接続、許可したDense・埋め込み・HC等の圧縮形式対応 | IQ側のprefill gate/upとして対応 |
| IQ4_NL | 公式CPU復号を使用 | expert接続、HC up・共有expert down・PLE等の役割別接続 | IQ側のprefill downとして対応 |
| IQ2_S・IQ3_XXS | 前回v0.30統合で公式CPU復号へ移行済み | expert接続、GPU内積の小数スケールを保持する補正 | IQ側のprefill gate/upとして対応 |
| Q2_0 | 公式Gufoにない型・寸法・CPU復号を追加 | 64要素/18バイトのブロック、専用の復号・ベクトル内積を既存基盤へ接続 | IQ側のprefill downとして対応。条件外のMMQ入口はベクトル演算へ委譲 |

対応の根拠は[役割別の許可](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/weights.cpp)、[CPU参照の共通化](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/cpu_ops.cpp)、[expertの実行選択](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp)、[GPU入口](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.hip.cpp)。

### Q2_0とGPU経路の注意

- Q2_0はQ2_Kとは別形式。Q2_0は64要素/18バイトで、FP16スケールと2ビット符号を使う。公式にQ2_KがあることではQ2_0対応を代替できない。
- `qfn_mmq_q2_0_moe_raw`は名前にMMQを含むが、中では`qfn_mmq_moe_vec`を呼ぶ。Q2_0専用のタイル型MMQを実装済みとは記載しない。
- routed WMMAのIQ側は、prefill、gate/upが同形式でIQ4_XS・IQ2_S・IQ3_XXS・IQ3_Sのいずれか、downがIQ4_NLまたはQ2_0、hiddenが256の倍数、expert_ffが64の倍数という条件を持つ。形式単独で「いつでもWMMAを使う」とは扱わない。
- Q5_0 downは現在この条件から外れる。これを既存WMMAへ接続する作業と、Q2_0の既存対応は分ける。

根拠：[Q2_0のCPU復号](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/core/quant/ggml_dequant.cpp)、[ベクトル内積](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/mmvq.hip.cpp)、[IQ用WMMA復号](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/iq_prefill.hpp)。

### IQ2_S・IQ3_XXSに残る数値補正

CPU復号の独自実装は既に公式へ共通化した。一方、取り込まれたGPUベクトル内積には、小数スケールを整数除算で切り捨てる箇所があり、カスタムで分子を整数のまま保持し、最後に浮動小数点の1/8または1/4を掛ける補正が残る。

これは高速化だけの変更ではない。既存試験にはIQ2_Sの±5.375、IQ3_XXSの±15.5を検査する独立した例がある。公式CPUへ移行済みという理由で、GPU補正やその検証まで除去しない。

根拠：[GPU補正](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/vecdotq.hpp)、[検証コード](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/native_expert_ops_test.cpp)、[由来とローカル変更](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/VENDOR.md)。

## 3. Unsloth v3.1対応との関係

Unsloth v3.1対応で新たに必要になった接続は、主にQ5_0 expertとテンソルなし先頭GGUFの読み込みだった。既存のIQ・Q2_0・Q6_K等の形式対応を再利用しており、Unsloth向けに全形式を別実装したものではない。GGUF読み込みはv0.31で公式実装へ移行済みで、残る独自テスト差分の除去が後日作業に入っている。

現在のUnsloth v3.1系 IQ4_XS（Orca重み移植版）の保存マニフェストを集計すると、移植対象146テンソルは次の構成だった。

| 形式 | 移植対象数 | 役割 |
|---|---:|---|
| IQ4_XS | 1 | token embedding |
| Q8_0 | 85 | shared expert down 48、SSM出力36、PLE value 1 |
| Q6_K | 12 | Attention出力。最終語彙出力層という意味ではない |
| Q5_0 | 33 | routed expert down |
| IQ4_NL | 13 | routed expert down |
| Q2_0 | 2 | routed expert down。層8・10 |

これは**移植対象146テンソルだけの集計**であり、モデル全体の形式数ではない。expert down 48本はすべてこの対象に入り、Q5_0が33本、IQ4_NLが13本、Q2_0が2本。したがってQ2_0は過去のISTA/SC117専用の残骸ではなく、このモデルでも必要な形式である。

今回確認したのは保存済みマニフェストと集計記録。モデル実体の再ハッシュや推論は行っていない。参照：`/home/user/workspace/LLM/orca-v31/manifests/inputs.json`、同`reports/quantization-summary.json`、同`reports/assembly.json`。Unsloth v3.1追加接続の履歴は[Q5_0対応記録](../../../../../v0.30/investigations/local/updates/q5-0-20261106/README.ja.md)。

## 4. 整理済み・維持・後日作業・未決候補

| 区分 | 内容 |
|---|---|
| 既に整理済み | IQ2_S/IQ3_XXSのCPU復号を公式へ移行。モデル独自Q5_1展開を共通入口へ集約。重みの寸法計算も共通定義を使用。旧IQ64トークン幅を削除し、現行WMMAの16/48/128に追従 |
| 維持 | Q2_0の共通形式・CPU/GPU対応、IQのGPU接続と小数スケール補正、役割別許可・配置、現行の数値・境界試験 |
| 合意済み・未着手 | Q5_0の4/8行設定とWMMA対応、量子化Dense入口の改名、既存経路・精度選択を確認して必要箇所だけ改修。詳細は[一括作業リスト](pending-work.ja.md) |
| 合意済み・後日整理 | 参照されていない旧CPU用コード表`src/core/quant/iq_codebooks.hpp`の除去。リポジトリ内のinclude・利用・ビルド定義・公開SURFACEに参照を確認できず、現行CPU復号は公式の別の表を使用している。実際の除去・ビルド確認は未実施 |
| 合意済み・後日説明修正 | `mmq/VENDOR.md`の対応形式一覧と試験の形式数がQ5_0追加に追従していない。Q2_0のMMQ名と実際のベクトル経路は既に本文説明があるため、棚卸しでも区別する |

`block_iq2_s`・`block_iq3_xxs`の公開宣言は、未使用の内部コード表とは別。宣言を持つ`ggml_dequant.hpp`は拡張APIのSURFACEに含まれ、前回はソース互換性のため維持した。内部未使用だけを理由に自動削除しない。

新しいQ2_0専用MMQや、全形式向けの大規模な再設計を今回の棚卸しだけで作業対象に追加しない。性能効果は未測定。棚卸し後のユーザー合意により、整理・改修と高速化の実装評価を一括作業リスト第6項へ追加した。作業開始は引き続き保留する。

## 5. 検証の扱い

CPU復号、モデル側行読み出し、GPUベクトル/MMQ、routed WMMA、モデル全体の試験は対象が異なる。公式CPU復号を使うことだけを理由に、カスタムGPU接続・数値補正・役割別許可の検証を廃止しない。

Unsloth v3.1のQ5_0対応には2026-11-06のGPU・8Kモデル試験の保存記録がある。候補v0.31.0にはCPU106件合格の保存記録があるが、GPU・モデル・ベンチ・通常反映は未実施。今回それらを再実行しておらず、過去の結果を今回の実測として扱わない。

## 6. 3区分に沿ったリファクタリング案

2026-11-08の追加調査。ユーザーの方針は「共通形式」「モデルへの接続」「高速化」に分け、重複と対応の偏りを確認して改善点を割り出すこと。調査後、以下の整理・改修と高速化の実装評価を後日の作業へ追加することで合意し、一括作業リスト第6項へ記録した。公式・既存カスタムの類似処理を先に参照し、公式で満たせるものは公式へ統一、使える部品は再利用する。実装・ビルド・試験・ベンチは未着手。以下の「候補」は適用先や効果を評価する対象を示し、未合意の提案という意味ではない。

優先順位の補足：ユーザーが現在使用するFlash-Nextの対応・効率を最優先とする。他のモデル構造へも使えることは望ましいが、そのためにFlash-Nextの速度やメモリ効率を損なわない。浮動小数点重みについても同じ優先順位で整理する方針を、一括作業リスト第7項へ追加した。

### 共通形式：意味・寸法・型変換を揃える

- CPU復号は既に大半を公式へ統一しており、ここを再実装しない。Q2_0追加とIQのGPU補正は維持する。未使用の内部`iq_codebooks.hpp`は除去候補だが、拡張APIに公開済みの構造体宣言とは分ける。
- `executor.cpp`の`SmallType`と`batch.cpp`の`EmbeddingType`は、BF16/F16/Q8_0/IQ4_XS/F32の同じ型変換を重複して持つ。型変換は共有し、埋め込み等の役割別の許可・エラーは接続側に残す候補。
- CPUの共通寸法定義に対し、GPUの`RoutedF16RowBytes`等はブロック寸法を別記している。既存の定数・構造体を利用し、共有可能な寸法の重複を減らす。GPU内で使えないホスト関数の呼び出しや、不要なヘッダー依存は持ち込まない。
- 保存形式の制約とカーネルの制約を分ける。例えばIQ4_NLの保存ブロックは32要素だが、routed WMMAには別の64要素単位の制約がある。行サイズが計算できることだけでWMMA使用可とはしない。
- 現行の形式一覧・`mmq/VENDOR.md`のQ5_0説明漏れを整理する。モデル名を新しい演算名・許可条件へ持ち込まない。

### モデルへの接続：役割と実行条件を明示する

- 役割は埋め込み、通常Dense、routed expertのgate/up/down、shared expert、HC、PLE等で区別する。CPUで読めることと、その役割をGPUで実行できることは別。例えばQ6_Kの通常Dense接続を整理しても、未対応のQ6_K埋め込み・routed expertまで許可しない。
- 現在の実行入口を保ち、重み形式・寸法・処理段階・必要な入力配置を確認する小さな共通判定に整理する。形式だけの巨大な「対応済み」フラグや、モデルごとの別パイプラインは作らない。
- `WmmaExperts`は現在、Q4_K/Q5_K系の組み合わせとIQ系の組み合わせを別に列挙する。下位演算にはIQ gate/upとQ8_0/Q5_1 downなどの部品もあるが、上位の組み合わせ条件では通らない。部品が存在することと、その組み合わせが検証済みであることを区別し、必要な実モデルの組み合わせから接続を揃える。
- `NativeQuantDense`への改名、生成・MTP検証・prefillの経路確認は合意済み作業を引き継ぐ。公式の既存選択を再利用し、必要な入力・出力精度とバッファ寿命が保証される接続にする。単純な行数判定の禁止や全FP32化を目的にしない。

### 高速化：同じ最適化の適用範囲を揃える

| 優先 | 改修ポイント | 現在の差と再利用する部品 | 状態・条件 |
|---|---|---|---|
| 高 | Q5_0 downのWMMA接続 | Q5_0を含む層はgate/upも含めてWMMA経路から外れる。既存routed WMMAに形式別復号と必要な接続を追加 | 個別作業として合意済み。16/48/128タイル、端数・分割一致を含めて確認 |
| 高 | shared expertのgate/upとSwiGLUの融合 | Flash-Nextのprefillはgate/upのDenseを別に実行。共通QwenにはQ6_K・IQ4_XS等の`TryLaunchBatchedDualQuantGEMMSwiGLUFp16`が存在し、別のQwen実行経路で使用済み | 実装・評価の方針合意。Flash-Nextの640×2560形状、丸め、FP16出力の消費先と作業領域を検証してから採用 |
| 中 | MMQのgate/up入力準備を共有 | wide fallbackの対処はQ4_Kだけがpaired入口を使用。Q5_K/IQ等は2回の`Experts`へ進む。共通テンプレート`qfn_mmq_moe_impl`自体は2重みを受け取り入力準備を共有できる | 改修・評価の方針合意。同形式・同形状から対象を広げる。Q5_0 WMMA接続後に残る実経路の寄与で優先度を判断 |
| 中 | 中間結果の再利用条件を整理 | `PrepareHalfInput`、`ReadyQ8`、shared expert、GDN/HCで入力・出力再利用が存在する一方、形式固有の分岐が分散 | 確認・必要箇所の改修を合意。既に再利用済みの入力を「毎回変換」と誤認しない。FP16/Q8の配置・丸め・寿命を揃え、適用漏れだけを補う |
| 中 | 少量計算の行まとめ・expert順処理 | Q5_1の4/8行設定、Q8_0の4行設定に対し、Q5_0/IQ4_NL/Q2_0は既定2行。expert順処理と`dst_slots`にも形式制約あり | Q5_0の4/8行に加え、IQ4_NL/Q2_0や並べ替えの確認・評価も合意。主に生成・MTP側で、prefillのWMMA改善とは別に評価 |
| 対象確認後 | 組み合わせ条件と小Denseの適用範囲 | 下位WMMA部品がある混在形式、小DenseのFP32まとめ処理が対応する形式・形状に偏りがある | 確認・必要箇所の改修を合意。実際に使う重みと形状を先に確認し、単なる全形式対応のためには増やさない |

shared expertの融合は、gate/upの入力を共用し、2本のFP32中間結果と独立したSwiGLU呼び出しを減らす候補。融合部品はFP16で最終中間結果を出すため、既存`SwigluHalf`の丸め方との一致が必要。既存の入力FP16化は`PrepareHalfInput`で再利用されており、その削減分を二重に見積もらない。

Q5_0 WMMA化とMMQ入力準備共通化は同じprefill fallbackに効く可能性がある。両者の想定効果を足し合わせず、WMMA化後にも残るMMQ経路を調べる。Q2_0は現在の実モデルの2層で既にWMMA対象なので、新しいQ2_0専用MMQを優先する根拠は今回の調査では得ていない。

主な根拠：[Flash-Nextの実行選択](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp)、[重複する型変換](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/batch.cpp)、[GPUの寸法・routed演算](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp)、[MMQ共通テンプレート](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.hip.cpp)、[Qwen共通の融合演算](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/families/qwen/kernels/prefill_fp16.hip)、[既存の融合演算利用箇所](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen/hip/prefill_chunk.cpp)。

## 7. 保存済みの全テンソル一覧から見える適用差

第3節の移植対象146テンソルに加え、保存済みの全テンソル索引を今回再集計した。各モデル1224テンソル、routed expertは48層。gate/upの形状は全層`[2560,640,512]`、downは`[640,2560,512]`で、現行WMMAの形状条件を満たす。

| 保存モデル | routed expertのprefill WMMA条件を満たす層 | 条件から外れる層 | shared expertのgate/up |
|---|---:|---:|---|
| Orca IQ4_XS | 48/48 | 0 | IQ4_XSが48層 |
| Unsloth v3.1系 IQ4_XS（Orca重み移植版） | 15/48 | Q5_0 downを持つ33層 | Q6_Kが47層、Q8_0が1層 |

Unsloth側のgate/up/downの組み合わせは、IQ3_XXS/IQ3_XXS/Q5_0が20層、IQ4_XS/IQ4_XS/Q5_0が13層。残る15層はIQ3_XXS/IQ4_NLが8層、IQ4_XS/IQ4_NLが4層、IQ2_S/IQ4_NLが1層、IQ2_S/Q2_0が2層（後半はgate/upが同形式のため1つに略記）。

したがってQ5_0の未接続はdownだけの差ではなく、33層でpaired gate/up・FP16中間結果・後段の融合を含む経路選択の差になる。shared expertも、同じ640×2560の投影に既存のQ6_K/IQ4_XS融合部品を利用できる可能性がある。一方、Unsloth側のQ8_0 HC・SSM出力等には公式の最適化経路があるため、Orca向けIQ4専用分岐を通らないことだけで未高速化とはしない。

これは**保存済みメタデータと候補ソースによる静的な照合**。今回の実行トレース・モデル実体の再ハッシュ・速度計測ではない。33/48という比率から速度倍率を推定せず、prefillの伸び悩みの原因候補と位置づける。

索引：`/home/user/workspace/LLM/orca-v31/reports/diagnosis/tensor-index.json`。今回の索引SHA-256：`08a5da8c3e57ebe433a2100ba0084e6e69c71a6e074d07f21d2c04834dfb8440`。索引内`orca`と`candidate`を集計し、候補ソース`d1bc769157a8ff46d7cd08139dfbe5a2d4bd993c`の`WmmaExperts`と照合した。

## 8. 後日の作業順と採用判断

1. 形式・役割・生成/MTP/prefillごとに、現在の入口と実モデルの使用箇所を対応づける。挙動を変えない型変換・寸法・名称・未使用物の整理を、演算変更と分ける。
2. Q5_0接続とshared expert融合を独立した変更として扱う。共通部品を再利用し、形式ごとの複製パイプラインを増やさない。
3. WMMA化後にも残るMMQ入力準備、生成/MTPの行まとめ、GDN/HC等の再利用漏れを、影響範囲と実測寄与に応じて進める。
4. 実行再開後、影響する数値・演算試験で確認する。特にprefillの短い末尾、分割、キャッシュ復元で同じ行の結果が変わらない契約を守る。既存許容誤差やgoldenを変更して成立させない。decodeとprefillを一律に同じ演算へ揃える、という意味ではない。
5. 同じモデル・プロンプト・設定・キャッシュ履歴で改修前後を比較し、expert gate/up/down、shared expert、HC/GDN等の処理時間と、利用者が待つ全体時間の両方を見る。異なるモデル同士のtok/s差だけで採否を決めず、対応が揃っても高速とは限らない形状は従来経路を維持する。

既存の公式実験記録（ローカル保管：../../../models/qwen3.8-flash-next/EXPERIMENTS.md）では、過大なタイル・先読み・prefill pass拡大が不採用または撤回された条件も確認した。単に全形式を最大タイルへ統一したり、prefill-chunkを増やして補う方針にはしない。今回は文書と共有メモのみ更新し、実装等の保留を継続する。

## 9. GDN・HCの中間結果再利用：説明用の追加確認

2026-11-08、候補`d1bc7691`と公式v0.31.0の実行コードを読み取り比較。説明後、ユーザーと①入力要件と③バッファ寿命・再利用情報を一緒に整理してから②不要な出力を省く順で進めることを合意し、一括作業リスト第6項へ反映した。既存の仕組みで保証される箇所は維持し、必要箇所だけを改修する。実装・試験・計測は未実施で、性能上の採用範囲は後日評価する。

- GDNのカスタムは、後続のIQ4_XS出力投影が既にFP16入力を使うprefillで、GDNの最終出力を直接FP16として書く接続。従来のFP32出力保存・再読込・FP16変換を省く。形状は出力2560×入力6144、通常prefill、出力投影を行う場合、最大バッチ以内に限定し、GDN内部の状態計算は変更しない。既存の出力作業領域を再利用する。
- 公式にもQ8_0投影向けのGDN直接FP16出力があり、Q8の別経路には後続用のQ8入力を直接作る仕組みもある。IQ4_XS接続はこの既存の出力機能を使う。Q8向けには1024トークン以上で行間に32要素分の余白を置く配置があり、連続配置を読むIQカーネルへそのまま渡せない。
- HCのカスタムは、FP32の混合結果を書きながら同じ結果のFP16版も生成する。適用条件はprefill、4ストリーム・hidden2560、IQ4_XSのdownと小さなIQ4_XS inject、作業領域内等。後続の通常投影が必要なFP32を保持し、FP16入力を使う投影は同時生成した結果を再利用する。
- HCでは間にinjectの投影が入る。現在の対象injectは小さいFP32演算経路なので共有FP16バッファを上書きしないが、`Dense`が再利用情報を保守的に無効化するため、その呼び出し後に有効なFP16版を再登録する。F16/BF16等の別injectは同じバッファへ自身の入力を書き得る。この違いを消して無条件に再登録しない。
- 公式Q8 HCにもFP16・Q8入力の同時生成、条件が合う後続に対するFP32出力省略、短い生成側の融合がある。保存済み全テンソル索引では、Orca側はGDN出力投影36層がIQ4_XS、HC down/injectがIQ4_XS、upがIQ4_NL。Unsloth v3.1系 IQ4_XS（Orca重み移植版）はGDN出力投影36層とHC down/upがQ8_0、injectがF32で、公式側の再利用条件に合う構成。IQ専用分岐を通らないことだけでは最適化不足と判定しない。実機での経路計測は未実施。

改修候補は次の3点。

1. **後続が必要とする形式・配置を基準に接続を整理する。** GDNの直接FP16出力はIQ4_XSとQ8_0の個別接続になっている。他の対応Dense形式でも既存演算がFP16入力を使うなら、実モデルの用途・形状・丸め・行間隔を確認して同じ機構を利用する。BF16をFP16と混同せず、使われない形式の一律追加は優先しない。
2. **HCで作る出力を必要なものへ絞る。** 公式の`half_only`・`no_f32`を再利用する候補。FFN側の現行`no_f32`はrouterがF16、routed expertがWMMA、shared gate/upがQ8_0等の条件を持つ。Q6_K共有expertやQ5_0 WMMA接続の改修に合わせ、すべての後続処理がFP16等の用意済み入力を使う場面では不要なFP32やQ8の生成を省けるか確認する。現行Q5_0のMMQ fallback等がFP32を読む場合は保持する。融合カーネルと小さい末尾の経路の双方を確認する。
3. **再利用情報の登録・無効化を既存の入力準備と揃える。** どの入力に対応したFP16/BF16/Q8が、どの寸法・配置で、どの処理まで有効かを明確にし、共通ヘルパー等で更新の重複を減らす。不要な無効化や余分な再変換があれば必要箇所だけ改修する。現行のinject後の再登録は成立条件があるため、新しい不具合と決めつけない。専用バッファ追加・全演算融合を先に決めない。

GDN・HCの既存演算試験には、FP32結果、FP16への丸め、GDN状態、出力領域の共用・境界、独立参照の確認がある。改修時もこれらの契約とprefillの分割・短い末尾での一致を維持する。速度効果は、対象形式・層での変換回数と書き出し量の削減を確認してから、同一条件で計測する。

根拠：[実行選択・再利用情報](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp)、[HC同時出力とGDN入口](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp)、[HCの既存試験](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/hc_mix_ops_test.cpp)、[GDNの既存試験](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/gdn_ops_test.cpp)。重み構成の出典は第7節と同じ保存索引。

## 10. BF16 HC／PLEの固定順序WMMA：説明用の追加確認

2026-11-08、候補`d1bc7691`と公式v0.31.0を読み取り比較した。説明後、ユーザーと①適用条件と②入力再利用は既存の共通化で必要部分だけ対応し、③タイル調整は現状維持とすることで合意した。速度を落とさない方針で、一括作業リスト第6項へ反映。実装・試験・計測は未実施。

現行カスタムは、公式のBF16行列演算を使う判定`OwnBf16`へ次の4形状を追加するもの。公式にもindexer向けの固定順序BF16経路があり、その`DenseBf16Gemm`と共有カーネルの演算本体は公式と一致する。旧専用ラッパーを残した二重実装ではない。`Prepare`・`Gemm`・`UsesOwnKernel`は同じ判定を利用している。

|用途|入力要素数→出力要素数|
|---|---:|
|HC down|10240→320|
|HC up|320→10240|
|PLE key|2560→10240|
|PLE value|2560→2560|

- 入力と重みをBF16として読み、FP32で積和・出力する。内部の型表記を理由にBF16からFP16への変換と解釈しない。
- 同じ行の積和順序を、まとめるトークン数や分割位置に依存させない。ライブラリ側は行列形状・トークン数で実行計画が変わり、同じ入力でも分割すると丸め結果が変わり得る。短いprefill末尾やprefix-cache復元時の計算の一貫性を保つ目的がある。
- 2026-11-06の保存済み比較では、公式indexerの対照形状を含む5形状×7分割で固定順序側の分割不一致は0。追加4形状の512行ではhipBLASLt比で約2.0〜4.3倍の単体速度だった。一部の128行ではライブラリ側が速いが分割差があり採用していない。FP64参照との差は形状で優劣が異なり、固定順序が常に高精度という意味ではない。これは以前の単体演算結果であり、今回の候補の実機測定やモデル全体の速度結果ではない。
- 保存済み索引にある通常モデル、Unsloth v3.1系 IQ4_XS（Orca重み移植版）のHC down/upとPLE key/valueはQ8_0のため、この追加4形状のBF16分岐は直接使わない。indexerのBF16には既に公式側の適用形状がある。これは保存索引とソースの静的照合であり、稼働中モデルの計測ではない。外部PLEテーブルの保存形式と、PLE key/valueの重み形式も別である。

公式演算を再利用した現在の固定順序演算・タイル設定は維持する。独自の管理機構や新しい演算を追加する必要性は確認できていない。ユーザーと合意した範囲は次のとおりで、共通化による速度低下を受け入れない。実モデルの検証範囲と他モデルへの慎重な変更方針は、一括作業リスト冒頭の基本方針に従う。

1. **適用条件と数値条件を維持する。** BF16・FP16の違いと実形状を守り、未確認の全BF16形状へ一律適用したり、トークン数だけで分割差のある経路へ切り替えたりしない。現行判定は既に共通なので、別の判定表を新設することを前提にしない。
2. **既存の入力再利用に揃える。** `executor.cpp`には入力元・寸法・BF16/FP16の区別を持つ再利用情報がある。第9節の整理時にBF16についても上書き・寿命・丸め条件を確認し、不要な変換が実際に残る場合だけ削減する。現時点で余分なBF16変換を発見したという意味ではない。
3. **タイル設定は現状維持。** 現行BF16入口の共通64×128設定を保持し、今回の一括作業では形状別の調整を行わない。調整には形状・トークン数により遅くなる可能性があり、改善効果は未測定。①②の共通化へ紛れて演算や起動設定を変更しない。

根拠：[形状判定と呼び出し](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/blaslt.cpp)、[BF16演算本体](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp)、[入力再利用](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp)、[既存の演算比較試験](https://github.com/ToR-O3/gufo/blob/53a217cbd8fc9a1a01d5081e91942fd6ecd764bc/tests/models/qwen38_flash_next/blaslt_ops_test.cpp)、[2026-11-06の保存済み比較](../../../../../v0.30/investigations/local/updates/v0300-review-20261106/report.ja.md#bf16経路の比較)。モデル構成の出典は第7節と同じ保存索引。

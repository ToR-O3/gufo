# Gufo 同時利用時の数値差：カーネルまでの切り分け

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

2026-11-01実測。対象はHuihui v2 UD-IQ4_XS＋外部BF16 PLE、Gufo v0.27.0の採用済みカスタム版、ROCm 10.1／WSL／gfx1151。

## 結論

今回追跡した数値差の発生源は、**Gufo公式側の `AttentionKernel<2>` / `AttentionKernel<4>` にある密Attentionと疎Attentionの演算順序の違い**だった。独自のIQ量子化拡張を実行しない、公式ソースからビルドしたAttention単体でも、採取した差をビット単位で再現した。

2048トークンの境界をMTPの検証バッチがまたぐと、境界より手前のトークンにも疎Attention経路が使われる。この経路では末尾の未完成ブロックの加算先が変わり、同じQ・K・V／同じ参照範囲でもFP32の丸め結果が異なる。

診断用コピーで各行の経路を揃えると、固定draft数3・7、各6セッション×128生成トークンで回答が一致し、**各MTPサイクル終了時の全248,320語彙のlogitsも、計660比較すべて完全一致**した。

**元の通常版へ復帰済み。診断用の境界処理は通常版には適用していない。**

## どの入力で経路が変わるか

位置は0始まり。今回の再現プロンプトは2040トークン。下表は生成途中の位置2045を比較している。

| 条件 | 単独の逐次生成 | MTPのまとめ検証 |
|---|---|---|
| 開始位置 | 2045 | 2045 |
| 今回の評価行数 | 1 | 4（anchor＋draft 3） |
| バッチ末尾位置 `start + rows` | 2046 | 2049 |
| 2048超過のバッチ判定 | なし | あり |
| 比較対象の先頭行が参照するK/V | 2046個 | 同じ2046個 |
| 実際に選ばれた経路 | 密Attention | 疎Attention |
| 末尾2個の処理 | 最後の256-keyタイル内、split 7 | 別タイルとしてsplit 0 |

疎経路のマスクには、先頭行の完成済み511ブロック（2044個）がすべて含まれていた。残り2個も可視なので、情報の欠落や未来トークンの混入を示す差ではない。違うのはsoftmaxの部分和・再スケーリング・加算順序である。

公式コードでは、バッチ末尾で疎／密を決める処理が[executor.cpp](https://github.com/gufo-org/gufo/blob/de15fdb91f6992f09a2514dabd6d6a8e5f8a2efc/src/models/qwen38_flash_next/kernels/rocm/executor.cpp#L3158)と[batch.cpp](https://github.com/gufo-org/gufo/blob/de15fdb91f6992f09a2514dabd6d6a8e5f8a2efc/src/models/qwen38_flash_next/kernels/rocm/batch.cpp#L836)にある。広いprefill側には境界手前の行を密経路へ分ける処理があるが、今回通った狭いdecode／MTP側では同じ扱いになっていない。

## 最初の数値差

| 観測位置 | 結果 |
|---|---|
| 第4層（index 3）のQ・gate | 完全一致 |
| 同じ層の参照対象K・V | バイト単位で完全一致 |
| 同じ層のAttention直後 | 6,144要素中3,928要素が異なる |
| その最大絶対差 | 8.3446503 × 10⁻⁷ |
| 第4層の出力投影後 | 完全一致に戻る |
| 第32層（index 31）の出力投影後 | 最初に差が残る。最大約5.05745 × 10⁻⁵ |
| 7トークン受理後のlogits | 248,320語彙中248,319が異なる。最大差0.449867 |

「第32層が最初」と見えた粗い記録を、Attention内部まで細分化した結果、第4層ですでに微小差が生じていることが分かった。後続の量子化・投影などを経て差が残り、さらに拡大する。**独自量子化カーネルが最初の差を作っているわけではない。**

この時点では受理した7トークンの文章自体は一致している。スコア差が後で近接候補の順位を変えると文章が分岐し得る。logits差は正答率の低下率を表す数値ではない。

## 公式カーネルだけで再現

公式コミット `de15fdb91f6992f09a2514dabd6d6a8e5f8a2efc` の `kernels.hip.cpp` を別ターゲットでビルドした。補助リンク用のW8A8ソースも公式と同一。独自のIQ量子化、モデルローダー、Executorはこの実行ファイルにリンクしていない。実行するのは採取済みQ/K/Vを使うAttentionとCPUのFP64参照計算だけ。

| 単体再実行の条件 | 単独生成の採取値との比較 | MTPの採取値との比較 |
|---|---|---|
| 密経路、1・2・4行 | すべて完全一致 | 元の微小差を再現 |
| 疎経路＋選択リスト、1・2・4行 | 元の微小差を再現 | すべて完全一致 |
| 疎経路＋直接マスク、1・2・4行 | 元の微小差を再現 | すべて完全一致 |

これはプレーンGufoでHuihui全体をロードした試験ではなく、公式Attention単体を独立実行した試験である。Huihui全体の混在量子化を読み込む部分には既存の拡張を使っている。

このため、`AttentionKernel<2>` と `<4>` のグループ数の違い自体や、選択リスト作成の有無は今回の差の原因ではない。`AttentionMergeKernel` より前の部分結果では、末尾2個が移動するsplit 0と7が異なり、split 1～6は一致した。差の起点は **`AttentionKernel` 内の末尾ブロック処理** と特定できる。

該当する[AttentionKernel](https://github.com/gufo-org/gufo/blob/de15fdb91f6992f09a2514dabd6d6a8e5f8a2efc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp#L2986)、[末尾処理](https://github.com/gufo-org/gufo/blob/de15fdb91f6992f09a2514dabd6d6a8e5f8a2efc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp#L3342)、[AttentionMergeKernel](https://github.com/gufo-org/gufo/blob/de15fdb91f6992f09a2514dabd6d6a8e5f8a2efc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp#L3385)、[起動ラッパー](https://github.com/gufo-org/gufo/blob/de15fdb91f6992f09a2514dabd6d6a8e5f8a2efc/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp#L7814)とCompactSelectionKernelは、公式と通常版で関数本文が完全一致していた。

独立したFP64参照に対する最大絶対誤差は、今回の先頭行では両経路とも約1.738 × 10⁻⁶だった。密経路だけが数学的に正しく疎経路が破損している、という結果ではない。問題は**同じ履歴なのにMTPのまとめ方で丸め結果が変わり、生成の再現性に影響すること**。

## モデル全体での因果確認

モデル全体の診断は、セッション容量4096・入力2040・生成128トークン・同時6セッションで実施した（変更前draft 1・2の切り分けは生成32まで）。同じ初期snapshotを使い、temperature 0、同じモデル・重み・ROCm・COPY待機設定で比較した。HTTPサーバーの応答キャッシュは介さず、EngineのMTP生成を直接実行している。

| 試験 | 結果 |
|---|---|
| 境界を越えない同一状態、評価幅1～8 | 各層・logitsが一致 |
| 逐次生成対まとめ検証、受理長1～7のrollbackを含む46比較 | 差なし |
| 変更前、固定draft 1・2、各6セッション | 32トークンまで検査した範囲で差なし |
| 変更前、固定draft 3、各6セッション | 位置2045からの4行検証で、全6セッションに同じスコア差を再現 |
| 診断用境界変更後、固定draft 3 | 各128トークンの回答一致。全語彙342比較一致 |
| 同変更後、固定draft 7 | 各128トークンの回答一致。全語彙318比較一致 |

診断用変更は、選択予算の範囲内にある行を密経路に保つもの。QFNの今回の選択条件で原因を確認するための実験であり、任意マスクを受け取る汎用APIの仕様まで検証した製品用修正ではない。通常ソースやDockerイメージには取り込んでいない。

## 前回の「3回目にまとめて分岐」との関係

今回、3回目という回数を条件にしなくても、MTPの検証幅を固定すれば再現した。前回の全6要求が一緒に変化した事象と、同じ先頭付近のスコア差を説明できる具体的な経路が見つかった。

動的なdraft数やまとめ方が境界のまたぎ方を変えるため、実行回ごとに発現が変わると考えられる。ただし、過去のHTTP試験の全サイクルのdraft数を今回さかのぼって測ったわけではないため、**その3回目の全スケジューリングを完全再現した、という主張はしない**。

キャッシュや独自量子化を差の発生に必要としないことは確認できた。これとは別のROCm／WSL転送待ち問題を今回解決したわけではなく、`AMD_SERIALIZE_COPY=2` は維持している。他モデル・他GPU・全コンテキスト長への一般化や品質／速度の本格ベンチは未実施。

## 通常設定への復帰

| 項目 | 復帰後 |
|---|---|
| イメージ | `gufo:v0.27.0-iq-performance-fbf0a9f8-rocm101` |
| モデル | Huihui v2 UD-IQ4_XS |
| 外部n-gram | BF16 PLEあり |
| セッション／コンテキスト | 2セッション・各131,072トークン |
| ROCm | 10.1、`AMD_SERIALIZE_COPY=2` |
| MTP | 最大draft 7・最小1（通常の動的設定） |
| thinking | auto |
| Docker再起動 | on-failure:3 |
| 確認 | 同じコンテナ・イメージ・起動引数、設定ファイル不変、通常ソースのGit差分なし |
| 応答 | `/ready` HTTP 200、短文要求に `OK` |

通常版は元のままなので、今回特定した境界での丸め差は引き続き起こり得る。診断用変更の本採用はしていない。

## 記録と再実行資料

- 通常復帰の検証（ローカル保管：final-verification.json）
- 公式と通常版の関数本文比較（ローカル保管：attention-source-equality.json）
- [層ごとの採取値比較](../../../../evidence/benchmark/reports/kernel-divergence-20261101-152253/raw-attention-comparison.json)
- [公式Attention単体の比較](../../../../evidence/benchmark/reports/kernel-divergence-20261101-152253/upstream-replay-comparison.json)
- 境界変更後の全語彙比較（ローカル保管：boundary-control-results.json）
- 単体ビルドの由来・コマンド（ローカル保管：upstream-replay-build-provenance.json）
- 診断コピーだけの変更パッチ（ローカル保管：diagnostic-source.patch）
- 採取ファイルのSHA-256一覧（ローカル保管：trace-manifest.json）

WSLの実記録: `/home/user/Apps/Gufo/diagnostics/kernel-divergence-20261101-152253`。

全採取値は `traces.tar.zst` に圧縮して保持。単体再現用入力は `replay-capture`、実行ファイルは `upstream-attention-replay`。通常モデルを再ロードせず、同じROCm実行環境で `upstream-attention-replay CAPTURE_DIRECTORY OUTPUT_DIRECTORY` により9条件とFP64参照を再計算できる。比較元のAR採取値は `replay-capture/reference-attention-raw.f32`。モデル全体の診断は通常モデルと同時ロードしない。

ビルド途中のダウンロードキャッシュと不要なビルド成果物は削除し、必要な採取値・診断ソース・再実行用バイナリを残す。今回、新しいモデルのダウンロードは行っていない。

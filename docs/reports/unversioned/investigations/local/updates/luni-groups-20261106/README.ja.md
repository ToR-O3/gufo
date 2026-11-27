# Luni変更の群別切り分け

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

状態: 最初の3候補の診断完了。すべて組み立て検査は合格、応答品質は不合格。その後の単独群2候補を含む [最終報告](../../../../../v0.30/investigations/local/updates/luni-groups-followup-20261107/README.ja.md) を参照。これは短い品質診断であり、正式な速度評価・一般的な拒否率評価ではない。

ユーザーが2026-11-06に承認した、Luniの変更を一群ずつ除外して不調の範囲を調べる作業。残す変更の倍率は1。除外するテンソルは再量子化せず、未加工Unsloth v3.1のバイト列に戻す。

| 候補 | 原本に戻す群 | Luniの変更を残す本数 | 9問の自動判定合格数 |
|---|---|---:|---:|
| drop-shared | 共有エキスパート down 12本 | 6 | 1 |
| drop-attention | 通常Attention出力3本 | 15 | 2 |
| drop-deltanet | DeltaNet出力3本 | 15 | 6 |

3候補とも作成・実行済み。作成処理とは別の照合で、除外部分は原本、残す部分は保存済みα=1データとバイト単位で一致することを確認した。組み立て時の全ファイル・変更範囲検証にも合格。未加工モデルは同条件の9問すべてに合格した。

同じGufo候補イメージ、AR・8192コンテキスト・内蔵PLE・temperature=0・seed=1で、未加工モデルと3候補を順番に試した。既存9問と、政治・歴史・保健教育などの無害な質問8問を使用。自由記述は本文を確認した。未加工モデルも無害8問を拒否しておらず、この比較から拒否の減少は測れない。防御的セキュリティの質問は未加工でも192トークンで打ち切られたため、完全な回答の合否には使わない。

`runtime-result.json`は実行結果、`assessment.json`と`manual-review.json`は判定、個別JSONは要求と応答、`*-assembly.json`は組み立て証拠。メモリ記録は `docs/benchmark/runs/luni-groups-20261106/`。通常Gufoは待機を確認してから停止し、このバッチの終了時に同じコンテナ・設定でHuihui＋外部Q8 PLEへ復旧した。

## Orcaを移植元とする案の調査

2026-11-06に公開モデルカードとHF APIのメタデータを確認した。モデルの重みは新規取得・加工していない。

- [Orca GGUF](https://huggingface.co/orcarouter/Qwen3.8-Flash-Next-Uncensored-GGUF): revision `98eb3357afd966c25cadd082c7f830eafde7aa52`。
- [量子化前BF16版](https://huggingface.co/orcarouter/Qwen3.8-Flash-Next-Uncensored): revision `da29941aee972c7eeab497e05a292c569da1f1d6`。APIでは179,999,981,424 BF16要素。カードの容量表示は独立実測していない。
- 作者の説明では、単一の拒否方向を残差へ出力する149テンソルから除去。通常Attention13、DeltaNet36、融合MoE down49、共有エキスパートdown49、PLE value projectionとtoken embeddingの2。通常Attention等の集計にはMTPを含む。これはカードの申告であり、こちらによる全重み比較ではない。
- Luniの18本に限定した現行手順は、そのまま流用できない。特に融合MoE、埋め込み、MTPの扱いとUnslothの量子化型・重要度行列への対応を調べる必要がある。HFテンソル数とGGUFテンソル数は変換により異なる可能性がある。
- BF16同士で差分を確認し、変更するGGUFテンソルを元の量子化型で作り直す案は検討できる。ただしOrcaの加工を完全に再現した場合、再量子化範囲も広がる。GGUF同士を差し引くと量子化方法の差が混ざるため、その差を拒否抑制の変更だけとは扱わない。
- 作者の品質・拒否評価は作者によるBF16/vLLMの報告。Unsloth v3.1へ移植した場合の結果を保証しない。
- 両リポジトリのAPIは `gated: auto`。公開カードには利用条件への同意とHFアカウント情報共有の案内がある。現在のアカウントに取得権限があるかは未確認で、同意操作はしていない。

Orcaへの切り替えは相談段階。許可済みのLuni切り分けは後半2候補まで実施し、結果を上記の最終報告へまとめた。

その後に提示された石川さんの公開レシピは、Orca由来の方向ベクトルを公式BF16へ適用する方式だった。ソース確認・IQ4_XSとの対応・必要な修正（ローカル保管：ishikawa-source/REVIEW.ja.md） を別記した。公開コードと小さな方向ベクトルのみ調査用に取得し、モデル加工はしていない。

## 追加で提示された公開例

以下は作者の報告と公開コードの調査であり、このPCで加工・再現した結果ではない。閲覧ツールでのHFページ取得はエラーになったが、公開APIとREADME原文はHTTP 200で取得できた。新しい重みのダウンロードや適用はしていない。

### Kujira CHiPS版

[モデルカード](https://huggingface.co/Kujira/Qwen3.8-Flash-Next-CHiPS-Abliterated-GGUF/blob/05e4338d94ed32fa22f926815dd45609854ca622/README.md)。Unsloth UD-Q3_K_XLを加工し、Attention出力47本をF16へ変更、その他のテンソルをバイト単位で維持したという報告。再量子化で変更が弱まったためF16を採用。公開LoRAは約1.52MB。GPT-6は96問の回答を採点したと記載されており、GPTが手法や実装を作ったかまではこの資料から確認できない。加工は作者の [gguf-neuron-explorer](https://github.com/balaena01/gguf-neuron-explorer) をModal上で実行。

作者報告: 拒否92/96→24/96、完全な回答29/96、無害100問で反復・空応答0。ただし無害質問の512トークン上限到達は79→95件に増加。短答55問は双方100%、小規模テキストでperplexity 2.29→2.40。試験セットとCHiPS+加工レシピは非公開。各試験1回、thinking・ツール呼び出し・長い課題・他の量子化での評価は未実施。

公開LoRAを実行前にGGUFへ統合する案は、実行時LoRAが不要という点で今回の条件に合う。ただしIQ4_XSへの移植品質は未確認。F16へ型を変える場合はサイズとオフセットが変わるため、現在の同サイズ差し替えスクリプトをそのまま使えない。GNEの [qwen4exp資料](https://github.com/balaena01/gguf-neuron-explorer/blob/main/docs/qwen4exp.md) では、既存bakeに分割GGUFの結合が必要、小型合成モデルでの確認を含むことが記載されている。Strix Haloでインストールすれば直ちに全工程が動くと保証しない。

### NeuralNotwerk MXFP4/FP8版

[モデルカード](https://huggingface.co/NeuralNotwerk/Qwen3.8-Flash-Next-heretic-2-MXFP4-FP8/blob/a398d13412aeb4e5682aeed38aecaa7023a9a54b/README.md)。元はtrohrbaugh heretic-2のBF16、snapshot `207cd22`。新規のHeretic探索ではなく、tcclavigerのツールでMXFP4/FP8へ再量子化したもの。PLEは既存のint6表を再利用し、元Qwenの設定・トークナイザーを採用している。

作者の実行環境はR9700×4、システムRAM512GB、専用tcclaviger/vllm。公開形式はGufoのGGUFでもStrix Halo向け配布でもない。一般説明・簡単な計算・JSON等の動作と日常利用を報告しているが、この量子化版自体のKLや広い能力評価は未実施。Luniの失敗をHeretic全般の失敗とみなさないための参考事例にはなるが、trohrbaughの除去強度や、こちらへの移植品質の保証にはならない。

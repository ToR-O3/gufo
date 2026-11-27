# v0.37 カスタム外部化の調査（2026-11-27）

> この文書は各作業段階の記録です。最終判断（2026-11-27）はv0.37の採用保留・外部化見送り、通常利用はv0.36 main、キャッシュ上限4 GiBです。本文の当時の状態・計画・測定値は維持しています。ローカル生ログは同梱せず、公開用コピーでは利用者ホームパスを匿名化しています。

## 結論

外部化の条件は大きく進んだ。公式Flash-Nextの実行基盤を依存先として再利用し、独自WeightSource／Stagesだけを別リポジトリ・Linuxモジュールにすることができる。EXL3拡張が実例。ただし既存カスタムをすべて公式未改変Gufoへ外付けし、現在の速度も維持できる状態ではない。

まず外部PLEと重みの解釈・検証を外部WeightSourceへ分離する方針が妥当。外部Stagesは既存のF16／Q8_1再利用を保てる契約を確認・整備してから。キャッシュ保持・ページプールなどの共通処理はモデル拡張の担当ではなく、本体側に残すか上流へ提案する。

実装・ビルド・試験・モデルダウンロード・通常サービス変更・Git公開は今回行っていない。実現性の判断は資料とソースによる。性能維持を実証した報告ではない。

## 基準と根拠

- 上流 v0.37.0 とレビュー用fork bb1c3e7c（実行コードb0e2750b）を区別。
- カスタム台帳: docs/local/updates/v0370-20261126/README.ja.md の棚卸し表。冒頭の通常起動・ベンチ状況は後日の作業以前の記載で、現在の運用状態には流用しない。
- .worktrees/v0370/docs/{extensions,writing-extensions,extension-api}.md
- .worktrees/v0370/docs/internal/{extension-build,extension-conformance}.md
- include/gufo/ext/families/qwen/models/flash_next/ と転送先のモデルヘッダー・実装
- 公開EXL3拡張README: https://github.com/gufo-org/qwen38-flash-next-exl3-gufo-extension
- GitHubのv0.37 writing-extensionsページ取得は失敗。ローカル同タグ・レビューソースを確認。
- v0.36タグにはFlash-Next公開ヘッダー群がない。同じ方式をv0.36へ適用するならAPI群のバックポートが別途必要。

## 今使える入口

### WeightSource

Probe/Open、BindTrunk/BindMtp、Tokenizer、Ngram、Regions、Upload/UploadDone、Identity/DraftIdentity、NativeContextLength、MtpPath、DraftHead等。独自形式・サイドカー・重みの検証とGPU uploadを担当できる。

Uploadはサイズ計測と実転送の2巡で同一の割当を要求する必要がある。未知形式はTensorRef::format_stateを使い、UploadとStagesを対応させる。GGUF未知型はsized=falseを許容するreaderとaccepts_unsized_tensorsの仕組みがあるが、サイズ・境界・形状の検査は拡張が責任を持つ。

注意: weight_source.hpp冒頭には「独自PLE表を提供できない」という古いコメントがある。しかし上流v0.37のNgramTableにはOpenEncodedと行デコーダがあり、EXL3でも独自符号化PLEの読み出しが使われる。実装の現状を優先する。

### Stages

Linear/LinearGroup、Attention、LinearAttention（GDN）、MoeFfn、MoeDefersEpilogue、MtpHead、PromptLayerを差し替えられる。DefaultStagesには公式のSharedExpert、WaitCounts/WaitRoutes、CompactRoutes、HalfRows、RoutedExperts、MoeEpilogue等がある。全実行器を複製する必要はなく、変更しない演算を既定実装へ委譲できる。

ただしLinearはformat_stateを持つテンソルに限られる。既存GGUFの全投影を、Stagesを渡すだけで横取りする仕組みではない。

AttentionArgs、LinearAttentionArgs、MtpHeadArgsは外部側では不透明な型。メソッドが存在することと、内部scratch・KV・出力候補等を自由に操作できることは同じではない。現在のAPIだけであらゆるカーネル差し替えが簡単とは判断しない。

Stage内は指定streamへキューし、原則同期・動的GPU割当は禁止。ScratchBytes/Attachで確保し、HIP graphの記録・再生で同じ処理が成立する必要がある。

### 外部拡張の組込み・配布

- 別リポジトリをGUFO_EXTENSION_DIRSで指定し、Gufoと同時にビルドできる。
- Linuxではgufo extension installによる動的モジュールも使える。
- depends:[qwen38_flash_next]、gufo::qwen38_flash_next_hipへのリンクで公式実装を再利用できる。
- gufo::qwen_familyで公開されたQwen共通処理も再利用できる。
- モジュールはホストの正確なビルドと依存先に一致させる必要があり、更新時は再ビルド。安定ABIではない。
- Windowsは依存付きモジュールをサポートしないため、同時ビルドを選ぶ必要がある。Windows実機保証は今回ない。
- Flash-Next/familiesとexportは実験的で互換保証外。汎用ext APIの0.3 revision互換と混同しない。
- 公開ヘッダー試験自身が「header確認でありisolationの証明ではない」と記載。exportは広いincludeを公開しており、将来の保守を考えて公開入口だけに依存する。

## 台帳との対応

| カスタム | 外部化の見込み | 必要な条件・残る制約 |
|---|---|---|
| 外部PLE | 優先候補 | 独自WeightSource::Ngramで既存readerを再利用し、metadata検証・寿命・ファイル識別を移す。初めはStages=nullで実行経路を保つ |
| Q/IQ/Q2_0・Q5_0 expert | 条件付きで可能 | WeightSource＋format_state＋独自Linear/MoeFfn。ただし既存Q8_1/F16入力再利用が公開LinearInputだけで再現できるわけではない |
| 混在形式・FP32/BF16/F16重み | 読み込み部分は優先候補 | 形状・層・消費先を見た検証と上限付き変換を移せる。共通化を形式単位に保ち、不要な全重み展開を避ける |
| Q6_K出力とMTP Q4_K軽量化 | 部分的 | DraftHeadはformat_state出力の場合に呼ばれる。通常GGUFのQ6_Kだけを外からフックできる入口ではない。既存軽量化はDeviceModel::SetDraftVocab内部 |
| HC混在形式・BF16 HC/PLE固定順序WMMA | 現APIでは単純移植しにくい | HC mix/combineやPLEの実行・融合はexecutor内部。HC/PLE専用Stageがない。必要な一般契約かモデル実装の置換が必要 |
| HC/GDN中間結果再利用 | 性能上の要注意 | forkのhalf_only/half_consumersはoptions_.stages==nullptrを条件とする。外部Stagesを渡すだけで一部のF32生成省略が無効になる |
| IQ expertのQ8_1再利用 | API補強候補 | LinearInputはF32主体。量子化済み入力・有効性・寿命・丸め契約が必要。拡張側で再量子化して機能だけ再現しても速度維持にならない |
| DFlash2共通backend・2件同時・継続状態 | 現在の独自変更は残りやすい | 既存runnerを使えばそのホスト版のDFlash2機能は使えるが、独自のbatch・rollback・scratch・continuation変更をStagesだけで注入できない |
| WSL Attention経路・共有expert校正 | 個別評価 | MoeFfnの構成変更には入口がある一方、校正・内部Attention状態の自由な交換は別問題。以前見送った最適化案は再導入しない |
| 日本語ASR loader | 独立候補 | ASR providerとして分離可能だが、Flash-NextのAPIとは別。公式ASRには同様の公開loaderフック/exportがなく、単純な差分ファイル移動ではない |
| retention eviction／メモリ圧力 | 本体側 | 共通キャッシュ保持方針・ページプールの修正。モデル拡張は状態を宣言する側で、共通保持方針の置換口ではない |
| Windows CI補修 | ビルド／CI側 | モデル実行拡張へ移す対象ではない |

## 外部PLEの具体案と注意点

provider_optionsに--ple-modelを宣言し、外部WeightSourceが主GGUFとPLEファイルを所有する。NgramTable::Open/OpenEncodedを利用し、内容・形状を検査する。Identityに主モデルとPLE、状態へ影響する設定を含め、古いPLEキャッシュを誤再利用させない。

現在のCreateGgufWeightSource(ple_model_path)とTextRunnerOptions::ple_model_pathはfork追加。上流のCreateGgufWeightSourceは引数なし。既存fork機能を呼ぶ外部モジュールを作るだけでは「公式未改変Gufoへ外付けした」ことにならない。外部側へ検証と所有権を移し、必要なBind処理も実装する必要がある。常に薄い委譲だけで済むとは限らない。

Stages=nullを保つ案は、現在のカスタムホスト上で省略経路を維持する有力案。ただし公式ホストに存在しない高速化が、WeightSourceだけで自動的に得られるわけではない。

## provider競合

公式Flash-Nextのgguf:qwen4expと同じキーを別拡張が名乗ると、公式／コミュニティの衝突検査にかかる。providerのIDを変えるだけでは解決しない。

候補は、明確に区別できる入力形式・専用ディレクトリとprobeを設計する、識別可能なキーの一般化を上流へ提案する、または--replace-builtinで同名公式拡張を置換する。専用ディレクトリは登録・識別方法まで試験し、曖昧なprobeで既存モデルを奪わない。

公式Flash-Next拡張全体を外部版へ置換する方式は、細かなフック不足を避けやすい一方、公式モデル実装の追従責任が残る。private core依存・リンク・シンボル・パッケージ構成の調整も必要で、現在のフォルダを動かすだけではない。

## 推奨順

1. 外部化の到達点を「別リポジトリ化」「公式Flash-Next再利用」「Gufo本体無改変」に分けて追跡する。
2. 外部PLE＋形式検証から外部WeightSourceを試作。最初はStages=nullとし、現在のカスタムホストを対照にする。
3. 同時に、公式ホストに残る必要差分を小さな一覧にする。fork独自APIへ依存したまま完了としない。
4. 高速化の外部化には、F16/Q8_1入力の再利用、HC/PLEの境界、draft headの所有権を表す最小の一般契約を検討する。単に「外部でも省略してよい」というフラグで安全性を飛ばさない。
5. その契約で速度・丸め・キャッシュを維持できる物だけStagesへ移す。共通キャッシュの修正は上流化または小さなcore patchとして別管理。
6. 最初は同時ビルドで再現性を固定し、安定後にLinuxモジュールの配布へ進む。

検証は、独立した数値基準、各形式の境界、AR/MTP/DFlash2、2セッション、継続・キャンセル・PLE切替時のキャッシュ識別、実配布物の起動、同条件のPrefill/Decode/メモリ比較を対象にする。キャッシュ上限4 GiB。性能が落ちる外部化は採用しない。今回これらの実装・検証を実行したものではない。

通常運用をv0.36へ戻す方針の相談と、v0.37以降での外部化研究は分けられる。外部化のためにv0.37をmainへ採用する必要はない。

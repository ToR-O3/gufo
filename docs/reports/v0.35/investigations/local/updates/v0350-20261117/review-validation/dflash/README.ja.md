# Flash-Next DFlash2 の独立検証

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

対象は v0.35 の Flash-Next、通常の IQ4_XS 本体と Q8_0 DFlash2。ここにあるパッチと実行ファイルは検証専用。パッチはそれぞれ元の v0.35 に対する独立した差分で、順に重ねるものではない。以下の実機項目は作成担当では実行していない。実行側のログと照合する。

## 8　2件の一括提案とターゲット検証

既存の `runner.cpp::SupportedPlans` が DFlash2 で返すのは幅1のみ。`DecodeBatch` は `!use_mtp_` で基本クラスへ戻り、ネイティブの `Session::RunIsolatedBatch` にも DFlash2 専用の逐次分岐がある。単独の `PrepareDFlash` は GPU の貪欲スコアを読み、累積確率0.7で候補列を切る。`ProposeBatch` は元の状態ではこの処理を呼ばず、selector の `greedy_score_confidence` も false。一括ターゲットの `ForwardBatchImpl` は各会話の logit 行を保持するが、単独で実行する `DFlashMeanStreams` を呼ばない。`CommitDFlash` は最初の特徴量行から読む。

- batch-negative.patch（ローカル保管：batch-negative.patch） は2つの異なる実プロンプトから独立したドラフトを作り、それぞれの保存点から単独と公式の一括提案を比較する。続いてターゲットの特徴量出力だけを既知のビット列で埋め、単独検証で有限値に置換されることと、一括検証で未更新値が残ることを調べる。終了0は現行版の欠落を再現した意味。信頼度の高い入力では、単独と一括の候補数が偶然同じになる場合もある。
- batch-prototype.patch（ローカル保管：batch-prototype.patch） は固定幅の2件、貪欲選択に限定した切り分け用。公式一括提案へ要求別の信頼度カットを通し、ターゲットの各対象層で全行の特徴量を書き、各会話の行位置から受理した分だけ注入する。通常の `FinishDecode`、巻き戻し、ドラフト側のフィードバックを通って、単独2件と6回連続で比較する。
- batch-pair-checks.patch（ローカル保管：batch-pair-checks.patch） は貪欲、シード固定、途中キャンセル、単独経路を扱う。既存の単独、一括でドラフト整形を共通化し、ターゲットのサンプリングとドラフトの乱数は会話別に保持する。未完了の会話を無効化し、その会話の RNG と保留中の補正トークンを戻す。検証用の runner が公開する DFlash2 計画は幅1と2。幅1、最終1トークン、残り1行、停止トークン、未シードの確率選択は元の経路に戻る。検証は物理幅2の共有 scratch を要求し、それぞれのセッションが調整役になり得る分を見積もる。

`Gufo-v0350-tests` 内で、専用バイナリを使う。

```sh
e=/result/review-validation/dflash
target=/models/ToRO3-Qwen3.8-Flash-Next-ZH-Hangul-Ban32260-GGUF/UD-IQ4_XS/Qwen3.8-Flash-Next-UD-IQ4_XS-00001-of-00004.gguf
draft=/models/DFlash2/Qwen3.8-Flash-Next-DFlash2-Q8_0.gguf
"$e/qwen38_flash_next_session_test" --dflash-batch-negative "$target" "$draft"
"$e/dflash_pair_prototype" --dflash-batch-prototype "$target" "$draft"
"$e/dflash_pair_checks" --dflash-pair-check "$target" "$draft" greedy 12
"$e/dflash_pair_checks" --dflash-pair-check "$target" "$draft" single 12
"$e/dflash_pair_checks" --dflash-pair-check "$target" "$draft" seeded 12
"$e/dflash_pair_checks" --dflash-pair-check "$target" "$draft" cancel 4
```

後半4本は各ラウンドで候補数、受理数、出力、セッション位置、乱数および次回用の保留トークンを単独相当の対照と比較する。最初のラウンドを時間の集計から外し、以降は対照先行と候補先行を交互にする。貪欲では少なくとも一度、2会話の受理長が違うことを要求する。キャンセル試験では1行目のターゲット実行中に割り込み、2行目を単独で継続する。これらの時間にはモデル処理とネイティブのセッション管理が含まれる。HTTP の同時2件の完了時間、単独のリリース比較、実際の scheduler 計画は別に測る。固定幅で得た結果から自動幅の採否を決めない。

## 9　継続キャッシュ転送

現物の Q8_0 ドラフト GGUF で attention.sliding_window=2048、block_count=5、KVヘッド8、キー次元128を確認した。K/V とも FP32。検証済みトークン1行が40,960 B、最大2,048行が83,886,080 B。長さ接頭辞8 B、ドラフトヘッダー80 B、GPUヘッダー64 Bの合計152 Bを加えた1区分の固定容量は83,886,232 B。Flash-Next は検証後に特徴量を直接注入する。リングが一周した保存時には K/V が全容量を使い、早い保存点で余白が生じる。

cache-probe.patch（ローカル保管：cache-probe.patch） は `GUFO_DFLASH_CACHE_AUDIT=1` の診断バイナリに実行境界ごとの時刻、バイト数を記録する。既存の同期を含むホスト単調時計を使う。GPUイベントや転送帯域の単体ベンチではない。

| 記録名 | 計測区間・バイト数 |
|---|---|
| capture_pack | 退避開始から一時 GPU スナップショット破棄まで。bytes は固定区分の容量 |
| capture_device_alloc / capture_d2d | GPU上の一時K/V確保と、そこへ詰める有効K/Vのコピーおよび既存の完了待ち |
| capture_resize / capture_serialize / capture_d2h | CPUベクトルの resize と増加バイト、直列化全体と有効シリアル化長、デバイスからホストへ転送した K/V バイト |
| capture_host_copy / capture_zero | キャッシュの各区分へ memcpy した有効バイトと、未使用区間に実際にゼロを書いたバイト |
| restore_resize / restore_host_copy | 最大容量の再確保・初期化と、各区分から一時ベクトルへ戻した物理バイト |
| restore_decode / restore_h2d | 長さで切った有効な形式の解析・復元全体と、各層K/VをGPUへ戻したバイトおよび完了待ち |

包含関係がある。capture_pack は D2D、resize、serialize を、serialize は D2H を、restore_decode は H2D を含む。これらを足した値は待ち時間にならない。同じ区分が複数回要求されたときは、実際に発生したコピーが各回分載る。非同期処理と要求の対応は時刻の重なりであり因果関係を表さない。確保済みの vector 容量が使われた場合、resize 時間に OS の新規確保は含まれない。

cache_workflow.py（ローカル保管：cache_workflow.py） は専用ループバックポート、空のディスク領域と1セッションで起動する。冷たい会話、異なる会話、元の会話のRAM再開を同じ設定で送る。wrapped では永続化されたことを待ち、専用プロセスを停止し、同じディスクで新しいプロセスを起動して再開する。shallow はディスクを設定せず、RAMからの復元に限定する。通常の2,048トークン間隔は既存の保存点から次の保存点までの条件で、初回の短い会話も永続化され得る。wrapped の同一プロセス内で選ばれた階層は記録し、再起動後はディスクヒットを要求する。サーバーが申告するTTFT・再開時間と、クライアントが最初の本文または推論文字列を受信するまでの時間を保持する。SDKのロールだけの先行イベントは最初のトークンに含めない。

```sh
# python3 は試験コンテナの Nix Python、またはその絶対パスを使用する。
src=/home/user/workspace/Gufo/.worktrees/v0350
python3 "$e/cache_workflow.py" --source "$src" --binary "$probe_gufo" \
  --baseline-binary "$baseline_gufo" --depth shallow --port 5819 --output "$new_shallow_dir"
python3 "$e/cache_workflow.py" --source "$src" --binary "$probe_gufo" \
  --baseline-binary "$baseline_gufo" --depth wrapped --require-partial --port 5819 --output "$new_wrapped_dir"
# 同じ形式を使う以前のライターから読む。形式を意図的に変えたバイナリは miss を指定する。
python3 "$e/cache_workflow.py" --source "$src" --binary "$probe_gufo" --phase probe \
  --producer-binary "$prior_gufo" --expect-restart hit --port 5819 --output "$new_compat_dir"
```

出力フォルダは毎回新規。各要求の本文・出力・使用量・開始時刻と終了時刻を report.json、各段階の全計測を *-probe.json、基準との対照を comparison.json に記録する。必要なイベントがないと不合格。専用の時間計測を有効にしたバイナリはログによる負荷も持つため、最適化を採用する性能判定は計測を外した同条件のバイナリで行う。1回の cold/retry だけで採否を決めない。

batch-cache-boundary.patch（ローカル保管：batch-cache-boundary.patch） は元の continuation_adapter_test の Capture/Load を使い、24回の上限以内で `0 < accepted < proposed` のラウンドを見つけた直後に全成分を保存する。別の会話が入っていたセッションに区分を逆順で戻し、3回の生成・全 logit・シード付き乱数・受理数・次回用の補正を照合する。旧バックエンド形式v3を明示的に認識し、未知の形式は復元を拒否する。ケースを構成できない場合は失敗として残す。

```sh
"$e/dflash_cache_boundary" --dflash-partial-cache "$target" "$draft" shallow
"$e/dflash_cache_boundary" --dflash-partial-cache "$target" "$draft" wrapped
```

公式27Bの分割転送は同じDFlashバックエンドを用いるが、Qwen27Bの continuation adapter が private state の friend として層別K/Vを直接参照する。分割すれば自動的に小さいキャッシュになるわけではなく、公式もリング最大容量の各成分に余白を持つ。幅の短い保存点のゼロ埋めと、満杯でも発生する D2D・ホスト中継・復元バッファを区別して採否を決める。導入する場合はドラフトの入出力だけを共有処理に抽出し、DFlash2の識別子に形式を加え、旧・新を区別する。ターゲット本体、MTPの互換識別まで変更する必要はない。

独立したCPU幾何試験 `/result/build-gpu/tests/models/qwen38_flash_next/qwen38_flash_next_continuation_layout_test` は作成担当が実行し `Flash-Next continuation geometry: PASS` を確認した。この試験範囲はDFlash用区分の数、容量、位置の会計まで。参考として本家27Bの `qwen_dflash_gpu_test BASE_27B.gguf DRAFT_27B.gguf` にはリング折返し、旧形式、破損したデータ、学習済み制御状態の独立した試験がある。27B専用の本体と対応ドラフトを必要とする。

### 実測で待ち時間を確認した後に検討する分割案

**現行の復元順序。** Flash-Next の BeginRestore は区分位置を照合し、`dflash_payload.clear()` の後、固定容量（通常構成で83,886,232 B）へ resize する。既に容量を持つ vector も全長を改めて値初期化する。LoadPrivatePiece は区分全体をホスト領域として登録し、全範囲を memcpy する。Validate は全区分の完全な到着を確認してから、長さ接頭辞の有効分だけを RestorePersistentSnapshot に渡す。そこで K/V を H2D 転送し、ドラフトのストリームを同期し、ようやく実行可能になる。RAM経路は component_text_cache.cpp、ディスク経路は cache/streaming.cpp が転送完了と Validate を待つ。同一スロットの生存状態を引き継いだだけの要求は、この一連の処理を通らない。

**一番小さい公式27B型の分割。** 公式の64 Bドラフトメタデータと、各層のK・Vの直接転送を共有DFlashバックエンド内の内部ヘルパーに抽出する。現行の公開 Snapshot/RestorePersistentSnapshot は一括退避専用で、外から層のポインタを取り出すAPIはない。公式27Bが使う friend によるアクセスを参照し、公開拡張APIには追加しない。Flash-Nextでコミットした特徴量は各ステップで既にGPUへ注入されており、保存時は injected==target、pending==0 を確認する。5層×K/Vの10区分を kPrivateState で追加し、各区分の容量は 2048×8×128×4=8,388,608 B。各チェックポイントの有効位置はターゲット位置とする。27Bの保留特徴量8行分をそのまま追加すると、このドラフトではさらに409,600 Bかかる。Flash-Nextに保留状態が導入される場合に初めて、形式に含める。

1. BeginRestore で旧一括 vector を廃し、受け取った区分位置と小さいメタデータ、範囲の到着記録だけを保持する。実行中のスロットを読めない状態にして、ドラフト自身のストリームと既存の転送が競合しないことを保証する。
2. LoadPrivatePiece で、K/V区分の入力を対応する層の物理リングへ `hip::CopySnapshotToDevice` と既存の Transfer/Completion で直接書く。入力順序に依存せず、メタデータが最後に来てもよい。公式27Bと同等の単純版はリング全容量をGPUへ送る。Flash-Nextでは pending==0 を保証しているため、既に検証した要求位置から `min(target, window) × KV行幅` を求め、有効域だけ転送することもできる。余白を飛ばしても受信した区分全体の範囲は記録する。折返し後は全リングをそのままの物理順で戻す。
3. Validate で、完全な範囲・完了イベント・ドラフト位置・primed・controller・lookup・pending==0を確認する。プロポーザの一時履歴・語彙表・copy幅を従来の復元と同じ初期状態にしてから公開する。未知形式、欠けた区分、誤った位置、失敗した転送ではスロットを無効にし、既存の再計算へ戻す。
4. CapturePrivatePiece 側では、単一の GPU 退避バッファにコピーせず、各層の元リングから `hip::CopySnapshotToHost` で有効分を直接転送し、その区分の残りだけをゼロで埋める。転送完了までそのスロットのドラフト状態の上書きと破棄を防ぐ。ネイティブ SessionSnapshot の形式と、そのテストのオラクルとして使う一括直列化は別経路なので温存できる。
5. Flash-Next DFlash2の component identity だけに新しい component layout 識別を加える。現行コアはそのidentityのSHA-256を使い、さらにディスクの component数・id・layout_version・kind・容量等を照合する。旧ライターで実データを永続化して、新方式でディスクミス・再計算・同一出力を確認し、その後、新方式同士の再起動ヒットも別に確認する。AR/MTPの識別文字列は温存する。

**容量会計。** 上記の64 Bメタデータを使う案の生の私有データは 83,886,144 Bで、旧形式との差は88 Bのみ。チェックポイントごとのファイル、区分メタデータ、ホストページの丸めが増え得る。主に消えるのはスロットに残る最大80 MiB超の一時ベクトルと保存時のK/V二重コピー。永続ファイルの大幅な縮小を見込まない。layout.PrivateBytes、ページの backing plan、streaming の必要ステージング量、実ログの payload・ファイル数・一時最大メモリを突き合わせる。kWindowRows で差分保存する案は別の変更で、実リングには window_rows + rows_per_chunk - 1 の物理容量と、上書き前の MutationGuard が必要。

**切り分け。** 実測で浅い復元の resize・ホストコピーだけが大きければ、先に旧一括形式のまま BeginRestore が既知の有効長だけを確保し、LoadPrivatePiece が余白を廃棄する案を比較できる。旧ポジションにおける実データ長は 152 + min(target, 2048) × 40,960 B（このモデル）。全成分の到着、wire length・形状の検証は保つ。この方法には、有効なK/Vをホストに一度揃える処理とH2D転送が残り、リングが一周したケースでのコピー削減は期待できない。wrappedでもホスト中継が支配的なら直接分割を試す。ディスクのI/Oや他のターゲット成分が支配的なら、DFlash2分割によるTTFT改善を主張しない。

実装の参照先。Flash-Nextの復元先確保（ローカル保管：../../../../../../.worktrees/v0350/src/models/qwen38_flash_next/continuation_adapter.cpp#L457）・Flash-Nextの検証と復元（ローカル保管：../../../../../../.worktrees/v0350/src/models/qwen38_flash_next/continuation_adapter.cpp#L937）・27Bの区分（ローカル保管：../../../../../../.worktrees/v0350/src/models/qwen/continuation_adapter.cpp#L296）・27BのK/V転送（ローカル保管：../../../../../../.worktrees/v0350/src/models/qwen/continuation_adapter.cpp#L680）・27Bのメタデータ復元（ローカル保管：../../../../../../.worktrees/v0350/src/models/qwen/continuation_adapter.cpp#L967）・ストリーミング復元の形式照合（ローカル保管：../../../../../../.worktrees/v0350/src/cache/streaming.cpp#L477）・ディスク保存間隔（ローカル保管：../../../../../../.worktrees/v0350/src/cli/serve/component_text_cache.cpp#L669）・リング形式の容量条件（ローカル保管：../../../../../../.worktrees/v0350/src/cache/adapter.hpp#L85）。

### #8 実測不一致の切り分け（私有診断バイナリ）

12サイクルの独立照合で、greedy は 0 始まり第8サイクルの行1、seeded と cancel は第5サイクルの行1で最初の一致判定に失敗した。単独対単独は12サイクル通過した。元の照合は採択、出力列、乱数、次トークン、位置を一括判定していたため、最初に異なった項目はこのログのみでは特定できない。

`batch-pair-trace.patch` と `dflash_pair_trace` は同じ2プロンプト・同じシードで、各行の anchor と全 draft ID、採択済み ID、提案直後の乱数、確定後の乱数と繰り越しトークン、head/lookup の選択、候補 ID 一致、確率最大差、各共通入力までの target raw logits の最大差・argmax と相互のスコア、採択済み共通入力の5層の特徴値の最大差を記録する。数値は各行で候補/独立 scalar の順。特徴の3値はビット差要素数、最大有限差、非有限値の不一致数。比較する履歴が分岐した後の logits と feature は共通入力までで打ち切る。次サイクル出発時の raw logits は emitted ID が異なった場合には同じ位置を表さない。同期読み取りを追加したため、この実行の時間を速度評価に使わない。

```sh
bin=/result/review-validation/dflash/dflash_pair_trace
"$bin" --dflash-pair-check "$target" "$draft" seeded 7
DFLASH_TRACE_SCALAR_DRAFT=1 "$bin" --dflash-pair-check "$target" "$draft" seeded 7
DFLASH_TRACE_SCALAR_TARGET=1 "$bin" --dflash-pair-check "$target" "$draft" seeded 7
DFLASH_TRACE_SCALAR_DRAFT=1 DFLASH_TRACE_SCALAR_TARGET=1 "$bin" --dflash-pair-check "$target" "$draft" seeded 7
"$bin" --dflash-pair-check "$target" "$draft" greedy 10
DFLASH_TRACE_MATCH_HEAD=1 "$bin" --dflash-pair-check "$target" "$draft" greedy 10
```

フラグの有無で経路を選ぶ。試験ごとに新しいプロセスを使う。単独 target 版は各行の forward の直後にその行の検証、rollback、特徴の受け渡しまで完了する。cancel の性質を確認するときは既定の組合せを使う。単独 target に切り替えたアブレーションはキャンセル監視の呼出順まで変える。

ソース上、単独側が固定長の greedy でも小さい head を使えるのに対し、バッチ側は同じ固定長で greedy 全員の head を full に限定する。また、単独側では条件に応じて次層に繰り延べた FFN の residual 加算より先に特徴を読む。バッチの候補実装は同じ加算の後に特徴を読んでいる。実機 artifact の GGUF ヘッダを確認し、対象5層は 6,16,26,36,46、48層の hc_attn down/up は Q8_0、第7層で 320×10240 と 10240×320、hidden は 2560 だった。各相違が実際に出力のどこへ現れるかは上の4経路で判定する。

#### 実機の原因特定と候補

実機の seeded では、最初のサイクルの提案と入力候補確率、ターゲットの検証 logits、出力、乱数、繰り越しトークンが一致した。これに対し、最初のコミットで5層すべて、各層2,560要素の特徴に差が発生した。行0の層別最大絶対差は 0.0210、0.0332、0.0457、0.0740、0.2605。行1は 0.0471、0.0182、0.0256、0.0557、0.1871。この後、第1サイクルで提案 top-k 候補 ID が変わり、第5サイクルの行1では候補版が 5122,321,4237,13,1368、単独版が 5122,321,4237,13 を返して、位置が18対17になった。そこまでの共通入力の検証 logits 80行はビット一致。元の greedy の第8サイクル行1では、返した4トークンと到達位置は同じで、提案数が3対4、head の選択は full 対 subset だった。初期の trace バイナリの draft_rng の scalar 欄は計測対象の取り違えにより常時0。これを修正した retap バイナリでは実際に提案に使った乱数を記録している。元の request sampler の rng および pending は最初から実値。

| seeded の経路 | 結果 | 共通入力の検証 logits で差の出た行 | 特徴の異なるセッション・サイクル |
|---|---|---:|---:|
| 提案・検証ともバッチ | 第5サイクル行1で失敗 | 0/80 | 12/12 |
| 提案のみ単独 | 同一箇所で失敗 | 0/80 | 12/12 |
| 検証のみ単独 | 7サイクル通過 | 0/112 | 0/14 |
| 両方単独 | 7サイクル通過 | 0/112 | 0/14 |

`batch-pair-retap.patch` は、各セッションの実際の行数で「次層の HC mixer まで FFN residual の加算を繰り延べるか」を判定し、単独版と同じ境界で特徴を読む。実測で greedy、seeded、cancel がそれぞれ12サイクル通過。共通入力の logits はそれぞれ90、192、96行で全てビット一致、比較できたコミット済み特徴も24、24、12セッション・サイクルで全要素ビット一致。cancel は6回目の監視で一方が中断し、残るピアが12サイクル通過した。これらの詳細採取時の速度値は使わない。

`batch-clean-retap.patch` と `batch-scalar-target.patch` は、性能計測用に診断読み取りを含まない2候補。前者は修正した特徴抽出とバッチ検証、後者は公式のバッチ提案と単独ターゲット検証を使用する。両者とも Flash-Next の呼び出しが個別リクエストの draft 長および head 選択を要求し、固定長 greedy の subset 判定を引き継ぐ。同じ入力の単独要求、未対応の幅／状態は既存経路を通す。

| 用途 | 修正したバッチ検証 | 単独ターゲット検証 |
|---|---|---|
| ネイティブ（計測可能） | `dflash_pair_retap_timing` | `dflash_scalar_target` |
| HTTP（計測可能） | `gufo_dflash_pair_retap` | `gufo_dflash_scalar_target` |

両実行形式は、候補ごとに同一のエンジン・ドラフト・runner オブジェクトからリンクした。HTTP は `../sparse-shared/http/gufo-control` と同じ Flash-Next 選択、CLI オブジェクト、Nix ライブラリ。上記2候補の `--version` は Gufo-v0350-tests 内で確認済み。各 SHA-256 は `clean-retap-provenance.json`、`scalar-target-provenance.json`。ビルドは `build_pair_candidates.py` を従来の Nix 開発環境で実行する。

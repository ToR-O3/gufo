# Gufo v0.35.0 カスタム台帳再監査（2026-11-19）

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

公式タグ `0260e116` と、11月19日に通常運用へ反映したfork `8296d0a4` を比較した。最終差分は140ファイル、6,204行追加・950行削除。[カスタム台帳](../../../../../cross-version/investigations/gufo-commit-guide.ja.md)、[v0.34の実機記録](../../../../../v0.34/investigations/local/updates/v0340-validation-20261112/README.ja.md)、[v0.35の取り込み記録](README.ja.md)を起点とし、各機能の呼び出し元、公式実装、通常モデルの重み形式、過去の採否と測定範囲を照合した。ソースの参照先はすべてこの候補の作業ツリー（ローカル保管：../../../../.worktrees/v0350）。

## 判断と優先順位

以下は監査時点の所見。実施時は末尾の[レビューで合意した実施順と採用条件](#レビューで合意した実施順と採用条件)に従う。

| 優先 | 対象 | 確認できたこと | 改修の方向 |
|---|---|---|---|
| 1・正しさ | HCの層別形式 | 読み込みは層ごとに複数形式を許可する一方、幅広い入力のQ8高速経路は先頭FFNの型で全体を決定する。通常モデルの本体96個は全てQ8 | 次のHCが実際に読む型と、直前の正規化が書く形式を一致させる |
| 1・互換性 | 外部PLE | 意味が同じGGUF整数配列を符号表現の違いで拒否。CPUで再現 | 公式の整数読み取り規則に合わせ、実際に違う表は引き続き拒否 |
| 1・互換性 | 外部拡張API | 公式ヘッダーでコンパイルできる位置指定の初期化がforkでは失敗。旧forkからの差分検査は通る | 公式と既存forkの両方の呼び出しを試験し、互換範囲を決める |
| 2・通常の生成 | IQ expertのgate/up | 通常モデル48/48層が対象形式。短い生成・MTP検証で、作成済みのQ8_1入力をIQ側が受け取れず再量子化する場合がある | 同一の入力・行数・列数・配置に既存のQ8_1を渡す |
| 2・通常のPrefill | GDNのQ6_K入力 | 通常モデル35/36層のQKV・gateとalpha/betaが幅広いPrefillでは同じF16入力を消費できる。HCのF16だけを渡す判定がこの組み合わせを拾わない | 次に読む全演算の条件を検査して余分な出力を省く。既存の積和を使う |
| 2・通常の長文 | WSLの疎Attention | HIPグラフを使わない短い生成でも、グラフ再生を前提に最大コンテキスト分のスコア用GPUグループを起動 | 正確な履歴位置が分かる直接実行で、既存カーネルへ有効な範囲を渡す |
| 3・起動／並行処理 | 共有expert | 47/48層はQ6_K/Q6_K/Q8_0で、公式の並行処理はQ8三本限定。対象1層でも起動時校正は48層を8巡する | 通常時のPrefill・AR・MTPの実測で直列／並行を決める。その速度を保てる場合に限り起動校正を短縮 |
| 3・DFlash2利用時 | 複数要求、継続キャッシュ | Flash-Nextは生成を逐次処理。状態を固定最大サイズで一度ホストにまとめる。公式27Bに別実装がある | 要求間の状態分離、キャッシュ復元を先に試験し、共通バックエンドを段階的に接続 |

優先2、3の新規案に対する速度差は今回未測定。以下の層数は現用の **Qwen3.8 Flash-Next、Unsloth v3.1系 IQ4_XS** の4分割GGUFヘッダーを直接読んだ結果。重み1,224個、先頭分割はメタデータのみ。GGUF上のデータ型とロード後の型を区別した。[通常モデルの形式・expert組み合わせ](../../../../evidence/local/updates/v0350-20261117/custom-audit-model-headers-20261119.json)

## 正しさ・互換性

**HCの混在は受け付けられるのに、計算の判定が一致しない。** 公式由来の `wide_mixer_` は幾何条件と先頭FFN側の `down.type == Q8_0` を見ている。forkはHC downにIQ4_XSも追加し、各層は独立に重みを保持する。正規化済み入力を前段がF16・タイルQ8で書いた後、後続HCは現在のdownの型を確認せず、Q8として重みを読む経路がある。先頭Q8で後続BF16/IQ4_XSなら広い入力で誤った解釈に進み得る。逆順ならQ8層で高速経路を逃す。コード上の不整合を確認した。混在ファイルによる誤出力の実測は今回行っていない。通常モデルのHC downはAttention側48、FFN側48全てQ8_0であり、この並びでは発生しない。

小さい修正は、初期化時に対象となる全HCが同じ前提を満たすか調べ、混在時に従来の汎用処理へ戻す方式。層別に高速経路を活かすなら、次の消費先まで確認して正規化の書き出しを決める。後段の分岐だけを直すとF32の入力が前段で書かれていない場合がある。両順の混在、単一形式、Prefill境界、再開、MTPを確認する。受理する型（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/weights.cpp#L109）・全体判定（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/executor.cpp#L1258）・書き出し（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/executor.cpp#L2367）・読み出し（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/executor.cpp#L2506）

**外部PLEには独自の旧キャッシュは見つからなかった。** 外部GGUFの対応を検査した後は内蔵と同じ表読み出し・先読みワーカーを使う。永続キャッシュでは外部GGUFの公式識別子をモデルの互換性判定に加える。サーバーのprovider optionと旧公開フィールドも、内部では同じ読み込みへ合流する。

不一致は接続前のメタデータ検査にあった。`ple.layers` の値を `[1]` に固定し、本体がUINT64の場合、外部もUINT64なら成功、INT64なら失敗した。値の違う `[2]` は想定どおり失敗。公式はこの配列を非負の整数列として読む。他のキーを含め、値域・負数・長さ・順序・欠落・実値不一致・形状・形式の検証は保ったまま、整数を比較する部分を共通化する。通常使用中の外部表の速度に関わる変更ではない。CPU再現ログ（ローカル保管：custom-audit-ple-20261119.log）・サイドカー比較（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/ngram.cpp#L130）・公式の配列処理（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/config.cpp#L46）・永続キャッシュ識別（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/runner.cpp#L992）

**拡張APIには外部呼び出し元から見える差がある。** `TextRunnerOptions` でforkの公開済み `ple_model_path` が公式の `provider_options` より前に入る。最後の位置に型付き `ParsedOptions` を渡す同一のC++23コードは、公式タグで成功し、候補では文字列に変換できず失敗した。名前で代入する通常のCLI経路は影響を受けない。公式・forkともAPI表示は0.3.20。公式タグを基準にした差分検査は公開ヘッダー3件を検出し、現系列の変更記録と版の増分を要求する。一方、forkの実際のv0.34 main `48974e3d` を基準にすると合格する。モジュールは別途ヘッダー等から作るビルド識別子を照合する。

公式の位置順を保つ配置・移行を検討し、公式の位置指定、既存forkの引数指定、名前付き初期化をそれぞれコンパイルする。旧フィールドを消すと旧fork側の公開契約が変わる。過去系列の変更記録は永久保存し、今回決める互換方針を現系列に記す。同一コードと検査結果（ローカル保管：custom-audit-ext-api-20261119.log）・フィールドの順序（ローカル保管：../../../../.worktrees/v0350/src/ext/text.hpp#L738）・モジュール照合（ローカル保管：../../../../.worktrees/v0350/src/cli/extension/modules.cpp#L122）

## 通常モデルで優先する高速化

1. **短い生成・MTP検証でIQ expertの再量子化を省く。** routed expertのgate/upは48層全て同じ型の対で、IQ3_XXS 28層、IQ4_XS 17層、IQ2_S 3層。短い1～8行では、HCが準備済みのQ8_1行を書けるのに、gate/up側でそれを探すのはQ4_K・Q5_K・Q8_0だけ。IQの対はFP32から一組につきもう一度Q8_1を作る。内部の投影関数は準備済み行を既に受け取れ、量子化器も重み型には依存しない。入力アドレス・行列形状・パディング・scratchの寿命が合う場合だけ渡せば、IQ積和とSwiGLUを変えずに済む。全層で毎回省けるかは実行時の再利用ヒット率による。生成と検証の各幅で、Q8_1のバイト列、logits、起動回数と処理時間を確認する。呼び分け（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/executor.cpp#L2282）・HCの準備済み入力（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/executor.cpp#L2486）・IQの投影（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/mmq/qfn_mmq.hip.cpp#L438）
2. **長いPrefillではGDNが消費するF16だけをHCから引き渡す。** 通常モデルは35/36層でQKVとgateがQ6_K、それぞれ出力10,240／6,144、入力2,560。alpha/betaはファイル上F32でロード時にF16へ変換する。対象の広いPrefillでは既存の3投影が同じF16行を使える。一方、F16だけを書かせる現在の判定はQ8専用に連結した重みを要求し、Q6_Kの35層が落ちる。各投影の実際の型・形状、他の消費先、injectと作業領域の条件を確認し、F32・タイルQ8などを不要なときだけ省くのが小さい。Q6をQ8用の連結カーネルに流す変更は重み表現が合わない。PP512/2048、短い末尾、キャッシュ境界・継続で出力と状態を確認する。ロード時の構成（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/device_model.cpp#L411）・入力処理（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/executor.cpp#L2052）・渡す形式の判定（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/executor.cpp#L4033）
3. **WSLの短い生成は履歴のある範囲だけ疎Attentionを起動する。** グラフ再生用の起動サイズを保つため、8行以下のindexerは有効ブロック数を0として最大予約長をGPUに渡す。WSLカスタムは直接実行を選ぶので、この固定サイズを必要としない場合がある。例えば予約131,072、処理済み範囲8,192、圧縮率4、256スレッドなら、スコア1行当たりの起動は128グループ、実データ範囲では8グループ。範囲外はすぐ戻る。**16倍はグループ数の比**。高速化率は未測定。既存の有効範囲引数を使い、ホスト位置が正しい呼び出しだけで適用する。MTP・巻き戻し・バッチ、キャッシュ復元、長文の境界と8/9行、WSL以外のグラフ再生を検証する。過去のv0.33ではWSL直接実行で内蔵MTPの2件同時が7.49→44.98 tok/s。v0.35でグラフを戻せるという根拠はない。起動範囲の決定（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/executor.cpp#L3109）・GPU側（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/kernels.hip.cpp#L9235）・[過去の対照](../../../../../v0.33/investigations/local/updates/v0330-wsl-eager-20261112/README.ja.md)

## その他のカスタムの採否

| 台帳項目 | 判定と理由 |
|---|---|
| モデル別の接続、CPU参照・試験 | **共有を維持。** Huihui、Orca、ISTA、Unsloth対応は同じ形式を使う部分があり、モデル名ごとの再実装は不要。IQ2_S・IQ3_XXSのCPU復号は上流と共有。独自Q2_0の64要素／18バイトとGPUの役割別接続は必要。独立fixtureはGPUの丸めやスケール補正を検証するためのもので、公式と同一になったGGUF先頭分割の独自試験とは目的が異なる。[形式別棚卸し](../../../../../v0.31/investigations/local/updates/v0310-20261108/quantization-inventory.ja.md)・共通復号（ローカル保管：../../../../.worktrees/v0350/src/core/quant/ggml_dequant.cpp#L651）・CPU/GPU基準（ローカル保管：../../../../.worktrees/v0350/tests/models/qwen38_flash_next/native_quant_ops_test.cpp） |
| Q系・IQ・Q2_0共通形式、Q5_0 expert、IQ expert WMMA | **維持。** 形式の読み込み、GPUでの行数・役割別の投影、独立の数値試験がそれぞれ必要。通常モデルのdownはQ5_0が33層、IQ4_NLが13層、Q2_0が2層。Q5_0のWMMA接続と小規模downの既存経路を維持する。Q5_0の専用128行版は過去にF16丸め差が出た。形式判定だけを広げない。[台帳](../../../../../cross-version/investigations/gufo-commit-guide.ja.md)・不採用実験（ローカル保管：../../../../.worktrees/v0350/docs/internal/models/qwen3.8-flash-next/EXPERIMENTS.md#L120） |
| 複数セッションのIQ gate/up | **条件別に再評価。** 2～8セッション、合計9～64行の対象経路でIQ2_S・IQ3_XXS・IQ4_XSをまとめる。単一の広い検証は別経路。IQ3_SにはPrefill WMMAがあるがこの一括処理には未接続で、通常モデルのgate/upには該当しない。v0.34の外部PLE C4対照は有効60.81、無効63.02 tok/s、双方24/24一致。一律の拡張を裏付ける速度結果はない。呼び出し（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/batch.cpp#L1286）・対象型（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/batch.cpp#L814）・[旧版の対照](../../../../../v0.34/investigations/local/updates/v0340-validation-20261112/README.ja.md) |
| Q6_K Attention、NativeQuantDense、F16入力の寿命 | **維持・部分整理。** 通常モデルは12/12 Attention層でQ/K/VがQ6_K。Qは出力12,288で広いPrefillにF16を使う。K/Vは各512で既存のFP32縮約を使う。三本を一律連結すると丸めが変わる。K/V間で同じFP32演算順を守る最適化は別途評価できる。nativeのFP32枝はhalf scratchを更新しないのに、呼び元が保持済みF16を一律失効させ、特定のIQ4 HCだけ手動復元する。またhelper内のF16自己変換枝は本体の2呼び元で先に処理済みか条件外。入力を上書きした場合・領域重複・並行実行の失効を残して整理し、試験を実行経路につなぐ。精度の境界（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/native_quant.hpp#L46）・helper（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/native_quant.cpp#L15）・再登録（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/executor.cpp#L2642） |
| 共有expertの並行化と校正 | **計測後に判断。** gate/upがQ6_KでdownがQ8_0の47層は公式のQ8三本限定の並行処理から外れる。残る1層が対象なので起動時に4ラウンド×2方式×48層＝384層分のMoEを実行する。全層を流すのはキャッシュが温まった1層の偏りを避けるため。先に通常モデルで入力・履歴・キャッシュ条件をそろえ、Prefill、AR生成、MTP生成ごとに出力の一致と実リクエストの速度を比較する。通常処理で再現する低速化があれば、その条件は現行経路を維持する。実行方式が決まった後に、校正の短縮で選択結果・通常処理の速度が保たれ、待ち時間が初回リクエストに移らないか確認する。Q6_Kを並行化する場合は別ストリームのscratch・実効帯域を確認する。過去に同じ640×2,560でgate/upとSwiGLUを一体化した案は遅く、丸めも変わった。校正（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/executor.cpp#L1334）・並行実行条件（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/executor.cpp#L3245） |
| 共有expertのSwiGLU→F16 | **採用済みの段階融合は維持。** IQ4_NL down用では中間のFP32丸めを保って専用F16バッファに直接書き、routed側の入力を保護する。台帳の旧試験は1,556,811点で差0。これはgate/up投影まで一体化して不採用になった案とは範囲が違う。通常モデルの共有downは48/48層ともQ8_0なので、このIQ4_NL特例は通らない。対象分岐（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/executor.cpp#L1975）・丸めの試験（ローカル保管：../../../../.worktrees/v0350/tests/models/qwen38_flash_next/swiglu_half_test.cpp） |
| GDN出力、IQ4 HC、BF16 HC／PLE固定順序WMMA | **維持。** GDN出力のnative向けF16受け渡しとIQ4 HCの特例は別形式向け。通常モデルはSSM出力36/36がQ8、HC inject 96/96がF32。BF16用のfork差分は対象4形状を追加し、準備・経路選択・実計算で公式の同じ固定順序カーネルを使う。通常モデルのHC down/up・PLE key/valueはQ8。BF16の24本はindexerのQ/Kで役割が異なる。BF16選択（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/blaslt.cpp#L24）・演算試験（ローカル保管：../../../../.worktrees/v0350/tests/models/qwen38_flash_next/blaslt_ops_test.cpp#L160） |
| Q6_K出力ヘッドとMTP軽量化 | **維持。** 公式Q8_0→Q4_Kで使う仕組みにQ6_Kを接続済み。98,304語×幅2,560の提案用prefixだけを別領域へ一度変換し、135 MiBを全セッションで共有。ターゲット検証と追加語彙には元の重みを保持。汎用Q4_Kとの区別は形式と重みの由来で行う。対象GPU演算2件・実モデル3件には保存済み合格記録がある。AR/MTP全文一致と単独の速度効果は別問題。実装（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/device_model.cpp#L516）・5試験（ローカル保管：../../../benchmark/runs/v0350-validation-20261119/qualification/results.json） |
| ISTA／SC117などの浮動小数点重み | **形式対応を維持。** 役割別のBF16/F16→実行時型への変換に、有限値検査・丸め規則・変換結果のハッシュがある。公式の生バイト転送への単純置換で契約が変わる。現在は8 MiB単位の同期転送と、変換した全バイトのSHA-256がある。対象形式のモデルの冷間起動で読み出し・変換・ハッシュ・転送を切り分けてから、公式の転送機構を再利用できる部分を判断する。通常モデルの今回の起動時間増をここに帰属させる根拠はない。変換・転送（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/float_upload.hpp#L75）・数値変換（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/float_weights.cpp） |
| Q6_Kの対応表示 | **役割別に明確化。** Denseや語彙出力のQ6_Kは使える。一方、routed expertのQ6_Kは形式判定では受け入れ、HIPロードでは明示拒否する。公式v0.35にも存在する食い違い。GPUで動く形式を案内するときに区別する。通常モデルのexpert downにはQ6_Kは無い。形式表（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/weights.hpp#L19）・GPUの拒否（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/device_model.cpp#L559） |
| 日本語ASR、NPU | **維持。** 単一safetensorsと、出力と埋め込みの重みを共有する構成への対応。重みを複製せず別名で登録し、HIPとNPUは同じ重みストアを使う。通常のFlash-Nextテキスト推論と独立している。今回NPUでの音声認識は未測定。ローダー（ローカル保管：../../../../.worktrees/v0350/src/models/qwen3_asr/loader.cpp#L234） |
| GGUF先頭メタデータ分割、共通キャッシュ、MTP提案数制御 | **公式のまま維持。** GGUF reader本体と元の単体試験は公式タグと同一。基本readerヘッダーに残るQ2_0宣言は別機能。サーバー共通のcomponent-cache本体とFlash-NextのMTP提案数ポリシーもタグと同一。これらへ旧カスタムを戻す理由は見つからなかった。reader（ローカル保管：../../../../.worktrees/v0350/src/core/gguf_reader.cpp）・cache（ローカル保管：../../../../.worktrees/v0350/src/cli/serve/component_text_cache.cpp）・MTP（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/mtp_policy.hpp） |
| 旧Attention境界対策・IQの64トークン専用分岐 | **廃止済みでよい。** 密／疎Attention境界の計算順序と専用試験は公式へ統一済み。WSLで見直すのは疎スコアの起動範囲で、境界の計算順序は維持する。旧IQ専用64幅も台帳で削除済み。現行の複数要求で合計64行を扱うこととは別。[過去の整理](../../../../../cross-version/investigations/gufo-commit-guide.ja.md)・現行の境界（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/executor.cpp#L3137） |
| Windows CI・公開文書 | **CIの取得検証は維持。** ccacheアーカイブ検証・vcpkgの再試行はCIの取得失敗対策。2つのワークフローにあるbootstrap再試行は共通化候補で、推論速度への影響はない。公開モデルカードのgreedy MTP完全一致という表現は要改修。forkの保存済み同一入力・温度0では内蔵5/24、外部8/24。Q6_Kの対象試験「未実行」も11月19日の記録へ更新できる。原因・全般の品質合格・速度向上は未確定。モデルカード（ローカル保管：../../../../.worktrees/v0350/docs/models/qwen3.8-flash-next.md#L53）・品質記録（ローカル保管：../../../../.worktrees/v0350/docs/internal/models/qwen3.8-flash-next/QUALITY.md#L10）・[定例の照合結果](../../../../benchmarks/benchmark/runs/v0350-validation-20261119/README.ja.md) |

**Q6_Kの入力準備にはもう一段調べる余地がある。** 短い生成でも、非連結のAttentionのQ/K/VやGDNのQKV/gateは共通のQ8入力準備を呼ぶ。通常モデルのQ6_K投影自身が使うのは元のFP32入力。前段が既にQ8_1を作っていれば照合だけで済むが、未準備だと使われない量子化を発行し得る。広い行列経路ではこの準備関数はすぐ戻る。消費する側が一つでもQ8を使うか確認し、測る場合は準備済みヒットと新規変換を区別する。Q8の準備（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/executor.cpp#L1801）・投影の振り分け（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/executor.cpp#L1927）・Attention側の呼び出し（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/kernels/rocm/executor.cpp#L3040）

## DFlash2とサードパーティ拡張

- **複数要求の生成**。Flash-NextはDFlash2を1要求ずつ処理する。11月19日定例で通常文の合計が1件34.15、2件33.59、4件33.15、6件32.51、8件32.59 tok/s。2件以上は `serial-fallback`。決定した検証方針は、共有DFlashバックエンドにある2～8件用の `ProposeBatch` を再利用し、まず異なる入力の2件で試すこと。候補の受理数が異なる場合も各会話の結果が単独実行と一致し、単独の生成速度を維持しつつ2件合計の実効速度が向上することを採用条件とする。接続には要求別の特徴量の保持、受理数が違うときの巻き戻し・乱数・キャンセル、検証側の一括処理が必要。長いPrefillの途中チェックポイントや層間譲渡も同じ特徴量の寿命に制約がある。通常の共有Q8 MTPにはこの逐次処理はかからない。既存の信頼度カット・巻き戻し統合・DFlash用の診断表示は維持する。Flash-Next側（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/runner.cpp#L405）・共通の一括提案（ローカル保管：../../../../.worktrees/v0350/src/families/qwen/dflash/backend.cpp#L353）・[実測](../../../../evidence/benchmark/runs/v0350-validation-20261119/summary.json)
- **DFlash2のキャッシュ転送**。Flash-Nextは状態を固定最大容量の1成分に登録し、ホストで一度まとめ、未使用部をゼロ埋めしながら区分にコピーする。復元時も全容量の一時領域を確保する。公式27Bは同じバックエンドのメタデータ、層別K/V、特徴量を分けて転送できる。採否は、実使用量・コピー・ゼロ埋め・復元時間を測って決める。ここが待ち時間の原因なら公式27Bの分割転送を再利用する。再開後の生成が一致し、通常の生成速度を維持し、再開要求から最初のトークンまでが短くなることを確認する。旧形式の識別、リング折り返し、部分受理、別の会話とプロセス再起動後の復元を試す。現行レイアウト（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/continuation_layout.cpp#L87）・転送（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/continuation_adapter.cpp#L629）・公式27B（ローカル保管：../../../../.worktrees/v0350/src/models/qwen/continuation_adapter.cpp#L311）
- **試験選択と単体bench**。Flash-Next変更時の機能試験はAR/MTPだけを自動選択し、キャッシュ起動試験もFlash-Next DFlash2プロファイルを扱わない。定例のHTTPベンチではDFlash2の実行記録がある。単体 `gufo bench --speculative dflash2` はsidecarを省略すると本体ロード前の検査をすり抜ける。既存のserve・他モデルと同じ段階で拒否できる。外部PLEのオプションはserve用で、HTTP定例はそこを通る。自動選択（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/functional/impact.json#L21）・キャッシュ試験（ローカル保管：../../../../.worktrees/v0350/tests/functional/cache_startup.py#L42）・単体bench（ローカル保管：../../../../.worktrees/v0350/src/models/qwen38_flash_next/drivers.cpp#L309）
- **公式の第三者向けレシピ #1163** は、外部提供の拡張をビルドし、実行・配布する手順。公開SDKだけで完結する独立providerの分離には使える。動的モジュールは公開ヘッダーのみを使用でき、他の拡張への `depends` や `kind: library` は使えない。組み込みFlash-NextのHIP演算だけ差し替えるAPIは見当たらず、`--replace-builtin` は拡張単位を置き換える。既存ASRへのロード対応や量子化カーネルなど、少量の差分のために公式providerを丸ごと複製すると更新負担が増える。公式配布レシピとモジュール制約（ローカル保管：../../../../.worktrees/v0350/docs/internal/extension-distribution.md#L73）・組み込み拡張の置換（ローカル保管：../../../../.worktrees/v0350/docs/extensions.md#L65）

## 前回の速度低下との関係と、改修の評価条件

11月19日の保存済み3版対照では、同じモデル・同じ要求に揃えると、内蔵32K AR Prefillの中央値はv0.34が1,109、v0.35現行が1,110 tok/s。外部64K MTP Prefillは992→1,001 tok/s。通常文MTPの6件同時はv0.34／v0.35 Q6改修前／改修後で60.74／58.14／63.13 tok/sだった。定例の大幅なPrefill低下はこの対照では再現しなかった。連続要求の最初のトークンについては公式のキャッシュ保存完了待ちを直接観測した。今回見つけた余分な仕事と、当時の時間差の原因は分けて扱う。[3版対照と待ち時間の生記録](../../../../benchmarks/benchmark/runs/v0350-slowdown-diagnosis-20261119/README.ja.md)

改修するなら、先にHC混在・外部PLE・公開APIの小さな回帰試験を固定する。速度はIQ再量子化、GDN、WSLの順に一つずつ、通常モデルで旧・新を交互に比較する。短いAR/MTPを1・2セッション、広いPrefill、短い末尾、長い履歴からの再開に分け、同じ入力・履歴・キャッシュ状態・提案数・出力量を照合する。量子化バイト、各位置のlogits、再開状態の一致を確認してから、該当段階の処理時間と起動回数を比べる。既存の5%かつ10 msの回帰判定は要求・段階ごとに適用し、試行の揺れと出力不一致を残す。他モデルの適用範囲は対応形式で記述し、ISTA/Huihuiなどで実測できた範囲を別記する。

今回の新規検証はGGUFヘッダー調査、実装差分と呼び出し追跡、PLEのCPU再現、拡張APIのC++コンパイル・差分検査。稼働サービスの操作、GPUベンチ、実装変更、公開操作は実施していない。

## レビューで合意した実施順と採用条件

1〜3は正しさ・互換性の方針を決定済み。4〜9は検証方針を決定済みで、採用する実装は実測後に決める。この一覧は実装前の計画。

| 順 | 対象と進め方 | 検証して判断すること |
|---|---|---|
| 1 | **HCの混在形式。** 次に実行するAttention側・FFN側のHCごとに、入力の準備と読み出しを同じ条件で決める（②）。 | Q8→他形式、逆順、全Q8を比較。混在時の出力が参照計算と一致し、全Q8で従来の高速経路と速度を保つ。②の判定に起因する通常モデルの低速化を再現できた場合だけ、全Q8に従来判定を残す③を試す。 |
| 2 | **外部PLE。** GGUFの各項目を公式の読み取り規則で解釈して照合する。 | 符号付きと符号なしの同じ層番号を受理する。負数、値・順序・長さ・形状などの不一致は引き続き拒否する。正常な外部PLEは従来の共通キャッシュ経路で読み込める。 |
| 3 | **第三者拡張API。** 0.3系列では旧 `ple_model_path` の位置と意味を維持。新規コードは名前付きの `provider_options` を使い、旧フィールド廃止は次の公式互換系列への移行時とする。 | 既存forkの直接呼び出しと新しい名前付き指定をコンパイルし、両方指定した際は公式オプションが優先されることを確認する。公式の10番目の位置指定との差と移行例を明記する。 |
| 4 | **IQ expertの入力。** 通常生成とMTP検証の1〜8行で、FP32からの再量子化が実際に重なる回数を調べる。既存Q8_1の直接受け渡しは候補。 | 入力の配置と有効期間、量子化バイト列、投影結果、最終logitsを現行と比較。二重変換が減り、影響する実リクエストが速くなる条件に限って採用する。 |
| 5 | **GDNの入力。** 広いPrefillで後続がF16だけを読む場合に、HCで不要な形式の書き出しを省けるか調べる。 | F16の丸めがビット単位で一致し、QKV・gate・α/βの投影、GDN状態、最終logitsも一致すること。95/96行、短い末尾、作業領域の上書き、履歴からの再開を含め、通常Prefillの速度で採否を決める。 |
| 6 | **WSLの疎Attention。** 現行の直接実行を基準に、履歴位置が確定している場合だけGPUの起動範囲を絞る案を試す。 | 2,048トークン付近、8/9行、MTP巻き戻し、キャッシュ復元、履歴長の異なる2件でスコア・選択ブロック・最終logitsと安定性を確認。実際の単独・同時生成で比較し、クラッシュや条件別の低速化があれば適用を見直す。 |
| 7 | **共有expertと起動時校正。** Q6_K混在の47層はまずPrefillで直列・並行を比較。AR、MTPも個別に実行方式を決め、その後で起動時の校正を検討する。 | 共有作業領域の競合がなく出力が一致し、速さを再現できる条件だけ並行化する。校正短縮は同じ実行方式を選び、通常の速度を保ち、待ち時間が初回応答へ移らない場合に限る。 |
| 8 | **DFlash2の複数要求。** 公式の一括提案を再利用し、異なる入力の2件から検証する。 | 受理数が異なる場合、巻き戻し、乱数、片方のキャンセルでも他方の状態が独立すること。各会話の結果が同条件の単独実行と一致し、単独の速度を保ち、2件合計の実効速度が上がれば採用する。 |
| 9 | **DFlash2の継続キャッシュ。** 実際の保存・復元を測り、コピーやゼロ埋めが再開待ちの原因なら公式27Bの分割転送を試す。 | 部分受理、リング折り返し、別会話、再起動後の復元で状態と生成が正しいこと。旧形式を識別し非互換データの誤復元を防ぐ。通常生成の速度を保ち、再開要求から最初のトークンまでが短くなる場合に採用する。 |

比較の基準はローカルv0.35と通常設定のFlash-Next（Unsloth v3.1 IQ4_XS、内蔵PLE＋MTP）。外部PLE・DFlash2は該当構成でも検証する。入力・履歴・キャッシュ状態・生成設定をそろえ、変更を一つずつ切り替えて旧・新を交互に測る。ARは旧新のAR、MTPは旧新のMTPで比較し、提案数・受理数・出力量も記録する。要求別・処理段階別の実時間、中央値とばらつきを残す。5%かつ10 msの回帰検出は調査の目印で、再現する低速化を許容する基準ではない。差を判別できない場合は保留し、影響する条件だけを再測定する。他構成の実機確認範囲は別記する。

採用候補だけを組み合わせ、通常設定で初回応答・Prefill・生成・再開に相互影響がないか最終確認する。公開する場合は結果をユーザーとレビューし、承認後にPRを作成する。

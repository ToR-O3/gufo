# Gufo v0.36 取り込み・検証

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

## 範囲
基準: fork main 8cbde688 / upstream v0.36.0 4a7d8c6d。
候補: .worktrees/v0360、codex/v0360-custom-review。元作業ツリーの未コミット変更は保存。
PR・push・main更新は未許可のため実施しない。
通常モデル・DFlash2・内蔵PLE・2セッションの現設定を normal-settings.before.json に保存。

## 実施順と判定
1. 公式更新・台帳照合。公式の同等処理で足りる場合は独自処理を撤去。
2. API/読み込み/演算/状態寿命の順に最小改修。整形・拡張依存・API契約・CPU/GPU対象試験。
3. 対象モデルのAR/MTP/DFlash2、混在量子化、HC/GDN、継続・2件同時を実機確認。
4. 内蔵PLE MTP、外部Q8 PLE MTP、内蔵PLE DFlash2の定例各3回。旧版と条件を照合し、速度・出力・メモリ・所要時間を記録。
5. 合格候補のforkイメージで、保存した通常モデル・DFlash2設定へ戻し、応答と実行モード確認。

## 棚卸し（53a217cbで定例3回完了）
|対象|方針|根拠・追加確認|
|---|---|---|
|Q系/IQ/Q2_0とQ5_0 expert|維持・接続確認|v0.36も追加形式は未対応。公式wave32/Q8経路を維持し、IQ/Q5_0固有経路との入力配置を確認|
|混在HCの層別判定|維持|公式は依然先頭HC型を見る。カスタムは実際の消費先ごとに判定|
|HC/GDN F16再利用|維持・検証|公式の可変幅作業領域にも同じ入力寿命を適用。数値と境界の対象試験|
|BF16 HC/PLE固定順序WMMA|維持|追加形式を公式入口から分岐。加算順序維持|
|ISTA等の浮動小数点重み|維持|モデル名でなく実際の形式・構造で判定。対象外モデルの実測を混同しない|
|外部PLE|維持|公式component-cacheへ接続。新GGUFパーサでも整数値比較・不正入力拒否を確認|
|外部拡張API|改修|0.3系列で旧フィールド互換を維持し、現行系列の変更記録不足を補完|
|Q6_K出力MTP軽量化|改修|Q6_K→Q4_K draft接続を保持。公式の新短行カーネルは1行/8行のビット不一致を検出したため不採用、既存MMVQを維持。検証重みは元のまま|
|DFlash2共通backend/2件同時|改修|公式の新scratch/session管理と整合。ターゲットはAR状態、draft別状態。新候補判定に丸め許容を接続|
|DFlash2継続キャッシュ|現状維持を基準に点検|独自snapshotと公式ページ共有の寿命・会計を照合。以前見送りの転送方式は復活させない|
|WSL eager実行|維持|過去のクラッシュ・速度低下回避。公式Run入口の条件を保持|
|共有expert並行化/校正|公式追従・既採用範囲維持|前回見送り案は再導入しない。通常処理速度を優先|
|raw prompt整列checkpoint|改修|公式CaptureBytesがoptional化。split-only adapterの現在状態サイズと有効性確認を両立|
|実メモリ圧力と断片化の区別|採用|53a217cb。十分な利用可能メモリがある際の過剰なキャッシュ解放を防止。実不足時の貸し出しは維持。実モデル54要求と正式定例で再利用確認済み|
|復元中のretention eviction|追加修正|28fd573f。台帳から外れても保持中payloadの復元は有効。利用記録更新の失敗で正常な状態を破棄しない。修正前に失敗する回帰試験付き|
|日本語ASR|維持|独立した単一safetensors/tied出力ロード対応。対象CPU試験|
|Windows CI補修|維持|公式には該当のccache/vcpkg取得補修がまだなく保持。今回Windows CI実行は未確認|

## 初回候補 f6b37aef の確認（後続のキャッシュ修正は別記）
- 別作業コピーでマージ競合解消。整形1134ファイル、family graph、公式v0.36基準の拡張API検査合格。APIは0.3.43。
- 製品用ビルド f6b37aef 成功。forkイメージ作成・版情報確認済み。レビューHEAD 5c8fbc08は試験登録・品質記録を追加し、実行コードは同一。
- GPU演算・AR/MTP同時生成・DFlash2ペア・途中保存復元・スロット再利用の対象試験合格。画像sidecar依存項目はスキップ、画像の合格ではない。
- HTTPの長文・キャッシュ11項目×3構成は全合格。定例ベンチ進行中、通常切替は未実施。

## 追加の実測と判断
- CPU: native_quant、continuation_layout、ngram、float_weights、mtp_sampling、runner_options、ASR checkpoint/config/loader合格。ASR loaderの初回77はモデル未指定によるスキップ、配置済み日本語モデル指定後に合格。
- GPU: DFlash scratch実機試験で transient_mapping=1、固定領域との中間結果完全一致、解放後の参照拒否を確認。混在HC試験合格。
- 新公式Q4_K draft短行経路は旧MMVQとの差が1行で最大8.94e-8、相対RMS1.31e-7。8行は旧MMVQと一致し、同じ行を1行で処理した場合と8行で処理した場合のビット一致に失敗。この経路は不採用とし旧MMVQ経路へ戻す。数値許容への変更は診断のみで撤回し、元の厳密な試験を保持。
- IQ grouped expertは新公式のグループ化を再利用。IQ側はsingle/multiを分離せず既存の出力順を維持する特殊化を使い、2カウンターと必要なgroup数を確保する。
- DFlash target tapsを公式のnarrow/wide scratchへ接続。Forward復帰時にscratch viewが戻っても、CommitDFlashまで有効な出力参照を保持。wide領域解放時に参照を無効化。
- この時点では定例ベンチ開始前。その後の中断・切り分け結果は下記に保存。現在は利用許可に従い通常Gufoを停止して検証中。

## 対象GPU試験の完了
`qualification/selected-passed.json` の13項目は全て終了0。継続途中保存423.77秒、スロット再利用45.09秒、同時生成216.03秒、DFlash2ペア54.72秒。以前のattempt1に保存した混在HC・DFlash scratch合格も保持。最初のdraft head不一致は失敗記録を残し、厳密な元試験で再合格した。数値許容は緩和していない。

`qualification/core-results.json` のcomponent text cache、GGUF reader、GGML dequantも合格。対象モデルはUnsloth v3.1 IQ4_XS、他の実モデル、画像、NPU、Windows実行を今回合格としない。

製品ソースと検証コピーのsrc/include 1,247ファイルは全一致。試験ソースの差はCTestへの新試験登録のみ（qualification/test-source-comparison.json）。

## HTTP確認
製品イメージで内蔵PLE＋MTP254.13秒、外部PLE＋MTP310.65秒、内蔵PLE＋DFlash2 223.99秒。各11項目全合格。context8192、2セッション、RAM4GiB、ディスク8GiB、thinking off。入力・キャッシュ復元の正しさを候補上で確認したもので、旧版と同時期の速度比較合格を意味しない。正式速度は別の定例条件で測定する。

## v0.36公式更新との関係

- **状態メモリとキャッシュ**（#1251/#1258/#1308/#1333）: coreが状態を宣言・確保し、必要なページだけを割り当て、checkpointと共有して復元する。公式経路を採用。DFlashのターゲットはARと同じ状態として申告し、draft固有状態は既存の独立した管理へ接続する。旧カスタムのサイズ固定・optional非対応を残さない。
- **長文prefill**（#1265/#1274）: 単独入力で約14K行の一時領域を使い、アイドル時に解放する。公式方式を採用。DFlashの特徴量だけ常駐最大幅で確保する不整合を改修し、同じ一時領域の寿命に合わせた。
- **MoE/Q8高速化**（#1321）: 公式wave32・Q8の経路を取り込み、追加IQ形式のpaired処理も新しい2カウンターのrouting配置に接続。通常WSLでは既存eager方針を維持し、未検証のHIP graph再有効化は行わない。
- **Q4_K短行経路**: draftの行数による数値差を既存試験が検出したため、この接続は不採用。演算許容を広げて通すことはせず、元のMMVQを維持。
- **GGUF等のRust化**: 公式readerを採用。追加量子化形式はC++の共通サイズ定義から解決されるため、Rustに独自の重複形式表は足さない。PLE sidecarの正常・異常metadataと読み込み後の寿命をCPU試験で確認。
- **外部拡張**（#1303等）: ディレクトリモデル・grammar hook・状態宣言などの新契約を採用。現行API系列0.3へ互換性記録を追加。量子化演算やexecutor内部の最適化を、未整備の境界を越えて無理に外部plugin化する変更はしない。
- **ASR/画像/NPU/Windows**: 新音声形式、Rust JPEG、EmbeddingGemmaのNPU修正、Windows HIP最適化フラグ等は公式更新として取り込む。独立した日本語ASRローダーは対象CPU試験で維持。画像/NPU/Windows nativeの実機合格は今回の結果に含めない。

完全に代替されて不要になった独立カスタム機能は今回の台帳照合では確認できなかった。旧公式インターフェースへの接続は上記の通り更新し、不要になった宣言は除去した。対応形式を減らす一括削除は行っていない。

## 初回定例ベンチで見つかった再利用退行
内蔵PLE AR C1/C2のprepared decodeで、2040-token promptがcached=0となり、公式ベンチの再利用契約に失敗。単独72測定は完了したが、正式測定を中断し切り分け中。RAM4GiB固定でも再現。診断ブランチはレビュー候補とは別に保存。機能試験11項目の合格だけで、この短い同一promptの即時再利用を合格とは扱わない。

## キャッシュ再利用退行の特定と修正
診断イメージ1d7b06c2で、2040/2032-token checkpointの復元後に `unknown retained checkpoint` が発生した。メモリ確保時のretention eviction後も、Acquireの共有所有権によりpayload自体は有効。しかし復元・Validate後のTouchが台帳不存在を例外として扱い、正常な状態を破棄していた。

28fd573fは `TryTouch` を追加し、この利用記録更新だけを任意にする。元の `Touch` の不正ID例外契約、転送完了待ち、状態Validate、失敗時Invalidateを保持。CPU回帰試験は修正前にcached_tokens assertion失敗、修正後にキャッシュ試験全体とretention試験が合格。書式1134ファイル・拡張API0.3.43検査合格。診断変更は別cloneに隔離し、製品には含めない。

旧v0.35対照は同じC1入力3回とも cached2040/prefill0/output128。初回v0.36のRAM4GiB固定でもcached0となり、自動RAM設定のみの問題ではなかった。28fd573fの実機確認結果と残った退行は次節に記載。正式ベンチは最終候補でやり直す。初回ベンチは `docs/benchmark/runs/v0360-initial-f6b37aef-20261125` に保存し、修正版の主値へ混ぜない。

### 同時要求で残った再利用失敗（調査継続）
- 28fd573fで単独復元時の例外は解消。内蔵PLE AR C1/C2、外部PLE AR C1、DFlash2 AR C1/C2・DFlash2 C1は各3回通過。
- 外部PLE AR C2、およびDFlash2 C2で `cached=0,prefill=2040` が残った。`state_lending` の保持削除で候補が全て消え、次の要求が `no_compatible_boundary` となる。
- `PagePool` は `CanCreatePagesWithoutReclaim` がfalseなら貸し出し側を解放していた。この判定は利用可能メモリだけでなく、MemFreeとbuddyinfoの連続空き領域も見る。MemAvailable約12GiBでも全ての保持点を解放することがある。
- 候補修正: 既存のhost floor未満でのみBorrowし、余裕があれば既存の上限付き生成経路を使う。容量・floor・低メモリ時の解放は変更しない。
- 回帰テスト: 断片化相当（may_create=false）＋余裕ありで貸し出しが呼ばれないこと、不足時には呼ばれて新規ページを作らないこと。変更前はcalls==0で失敗、変更後0.18秒で通過。
- 既存mem_region_testはLendingのrange->Commit、mem_cache_lending_test/mem_cache_surrender_testは貸し出し/返却量で失敗。変更前でも同一失敗を確認。これらを合格扱いにしない。
- focused測定のMTP参照ファイル不足も保存済み。該当結果は検証合格に含めず、参照生成後に再実行する。

- 既存貸し出し/返却テストは28fd573fのstate_page_poolでも同一assertで失敗（qualification/cache-pool-control-tests.json）。追加回帰は53a217cbで通過。既存失敗を採用候補の合格証拠には使わない。
- 追加候補コミット: `53a217cb fix(cache): retain reusable state above the host memory floor`。実機C1/C2再測定待ち。

### 53a217cbの実モデル再検証
3構成×AR/投機生成×C1/C2×3回は全通過。計54要求のcached=2040、prefill=0、生成128、各単独AR対照との出力完全一致を確認。`cache-reuse-pressure/verified.json` に全件を保存。初回の測定設定ファイル不足は実行前エラーであり、モデル測定の成功に数えない。長文・継続HTTPと正式定例はこの製品を使う。

53a217cb製品でHTTP11項目×3構成も全合格。内蔵PLE223.72秒、外部PLE331.51秒、DFlash2 212.15秒。通常起動前の機能確認であり、正式速度比較は以下の定例測定を使う。

正式ベンチは53a217cbで開始。完了後に対照実験用test-sourceのstate_page_poolを候補へ戻して再ビルドし、追加の回帰テストを最終確認する（製品・測定用ソースは既に53a217cbで固定）。

## 最終測定と追加確認
- 53a217cbで69ジョブ×3回が終了。全ジョブの出力・再利用条件に合格。測定本体は8598.16秒（2時間23分18秒）。
- 最終回帰確認の初回失敗は、対照用旧コードのオブジェクトが残り、tar復元のmtimeにより再コンパイルされなかったため。失敗ログはstale-build名で保持。対象ソースを更新時刻込みで再構築し、追加回帰は合格。src/include 1247ファイルも候補と全一致。製品ビルドは別の固定ソースであり、この検証コピーの問題とは独立。
- 生成速度は全体として改善し、DFlash2の通常文・2件同時は合計44.38→49.62 tok/s。全項目が高速化したわけではなく、内蔵PLE MTPの反復64K/128K Prefillは約5.3%低下した。これらは別日の3回比較で、変更との因果は未確定。同一版の対照ARにもPrefill約−2.75～+6.60%の変動があった。準備済みARのTTFT増加は、公式#1248により最後の生成トークンまで状態を進める変更と、旧版live／新版RAM復元の違いを確認した。旧動作へ戻すと会話継続の修正を失うため維持する。DFlash2・2件同時のTTFTは約202→227msで測定範囲が重なり、単独原因は未確定。速度全項目の退行なしを宣言する結果ではない。
- 詳細証拠: qualification/ttft-investigation.json。既存ページ貸し出しテストの変更前からの失敗は未解決であり、対象実モデル試験の合格と区別する。
- システムメモリ監視の最大間隔はWSL17.02秒・Windows23.14秒。その間の未観測ピークは保証しない。

## 通常起動への反映
2026-11-25T09:11:18.693749+09:00 に gufo version 0.36.0 (53a217c) のforkイメージで通常起動を確認。指定モデル、内蔵PLE、DFlash2、2セッション、context131072を保持。実応答でdraft34件中33件採用。起動引数・実行バイナリhash・Docker設定を照合。旧コンテナは停止したrollback用として保持。初回の設定照合はDNS未指定のnull/空配列表記差で停止し、同等性を確認後に再作成した。

PR・push・mainへの反映は行っていない。レビュー候補は53a217cbのまま。定例表: [v0.35/v0.36比較](../../../../benchmarks/benchmark/reports/v0360-review-20261125/report.ja.html)、[速度差](../../../../benchmarks/benchmark/reports/v0360-review-20261125/previous-v0350.ja.html)。

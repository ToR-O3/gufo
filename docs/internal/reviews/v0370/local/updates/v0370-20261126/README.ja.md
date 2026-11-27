# Gufo v0.37取り込み・カスタム棚卸し

> この文書は各作業段階の記録です。最終判断（2026-11-27）はv0.37の採用保留・外部化見送り、通常利用はv0.36 main、キャッシュ上限4 GiBです。本文の当時の状態・計画・測定値は維持しています。ローカル生ログは同梱せず、公開用コピーでは利用者ホームパスを匿名化しています。

## 結果
v0.37.0を隔離コピー `.worktrees/v0370` の `codex/v0370-custom-review` へ取り込み、カスタムを再適応した。レビューHEADは **bb1c3e7c**、実行コードは **b0e2750b**。最後のコミットは文書のみ。基準は53a217cb、上流タグは684b716a。push・PR・main反映・通常版のv0.37採用は行っていない。

製品ビルド、対象CPU/GPU確認、HTTP4構成、通常パラメータでの候補起動・実生成は成功。速度比較では回答・処理量が一致した。ARの初回Prefillには再現が揃わない遅延が残り、全体の速度合格や速度改善は断定しない。通常環境は変更前のv0.36＋内蔵PLE＋DFlash2へ復帰し、イメージ・バイナリ・引数・設定・実応答を再確認した。

正式3回ベンチは未実施。EXL3は資料・ソース調査のみで、重みダウンロード・実機評価は未実施。Flash-NextのUnsloth v3.1 IQ4_XSを中心とした検証であり、他モデル・Windows実機・音声/画像の品質を新たに保証する結果ではない。

## カスタムの棚卸し
|項目|方針|今回の対応・確認|
|---|---|---|
|Q/IQ/Q2_0・Q5_0 expert、混在Q6_K出力|維持|公式は追加形式全体を代替しない。共通形式判定・既存routing/演算順を維持|
|Q4_K draftの旧MMVQ固定回避|公式へ置換・採用|公式#1397が1行と多行の丸めを統一。Q6_K由来draftも新kernelを使い、厳密な行数間一致を確認して採用。一般Q4_K投影は従来native経路を保持|
|Q6_K→Q4_K draft head|維持・改修|新WeightSourceが供給するheadを参照。検証target重みは変更しない|
|外部PLE|改修|GGUF WeightSourceがsidecarを保持しNgramを供給。旧Model内reader所有を撤去。metadata検査を再利用しcache identityを分離。非GGUF源への無条件適用はしない|
|HC混在形式、BF16 HC/PLE固定順序WMMA|維持|層別・消費先の形式判定を保持。新stage呼び出しを通す|
|HC/GDN F16中間結果再利用|維持・改修|新MoeDefersEpilogueに接続。共有expertのIQ4_NL F16処理を新SharedExpertRowsへ移す。外部StagesにはFP32入力も生成し、通常GGUFだけ省略最適化を維持|
|ISTA等の浮動小数点重み|維持・改修|新WeightSourceのhost tensor入力も既存の上限付き変換で処理。新しいコピー入口を使う|
|DFlash2共通backend・2件同時・継続キャッシュ|維持・改修|familyの共通実装を保持し、公式Qwen family公開・新モデルSource/Stagesと整合。旧raw-prompt/optional Capture対応も維持|
|WSL eager、共有expert並行化/校正の前回採否|現状維持|過去に見送った案を再導入しない|
|キャッシュ復元時のretention eviction/メモリ圧力判定|維持|v0.37にも同等修正はなく、前回の回帰修正を保持|
|日本語ASR|維持|独立loader対応を保持|
|Windows CI補修|維持|上流更新を取り込み、残存する取得失敗への補修は保持。Windows実機は未確認|
|外部拡張API|公式へ追従|実験的Qwen family/Flash-Next公開と拡張間リンクを採用。安定API互換記録は保持|

## 公式更新と新機能
公式リリース: https://github.com/gufo-org/gufo/releases/tag/v0.37.0

- HTTPをtokio/hyperへ、要求解析・SSE出力・生成済みreasoning/tool解析・RealtimeをRustへ移行。API互換と継続・キャンセルを確認する必要がある。
- PNGエンコードとtokenizer.json読み込みもRust化。libpng依存撤去。recursive tool schemaの検査上限とrequired alternativesを修正。
- Flash-NextのStages/WeightSource、Qwen family公開、extension間依存/動的moduleリンク。EXL3など別形式がパイプライン全体をコピーせず差し替え可能になった。
- expertストリーミングは実験的API。公式RFCはcore modelへのストリーミングを非目標としており、既存GGUFの自動offloadではない。
- deferred token後のmulti-token decode、KV mapping境界を跨ぐsnapshot copy、Q4_K draftの丸め、非有限router値の診断を修正。
- Inkling 2 MiniとDSpark/vision改善、GTT不足・モデルロード失敗時の診断、Debian apt配布。Flash-Nextの速度向上と混同しない。

## EXL3
出典: https://github.com/gufo-org/qwen38-flash-next-exl3-gufo-extension

別拡張 `qwen38_flash_next_exl3`。Linux x86-64/gfx1151向け。4.05/3.05/2.05bpwの専用safetensorsディレクトリ、内蔵MTP、独自tokenizer.jsonと圧縮n-gramを使う。公開READMEの配布サイズは108/86/64GBで、GPU使用メモリではない。n-gram39/33/26GBは全体をGPUへ置かず必要行を読む。

公開の開発ブランチ測定ではARは約36/40/44 tok/s、比較Q4 GGUFは約30 tok/s。入力・対象重み・版がこちらの定例と異なるため、現在のIQ4_XSより速いという実測結果にはしない。Prefillも専用展開とtrellis演算を使い、bit数だけで速さは決まらない。重み変更で数値も変わる。

EXL3拡張のREADMEは現在の実装、docs/EXL3.md冒頭には移植前を指す古い記述が残る。現行README/ソースと、旧開発ブランチの性能・品質資料を分けて扱う。EXL3は今回未インストール・未ダウンロード・実機未確認。DFlash2利用を確認した資料ではない。このEXL3拡張のplatform宣言はLinuxのみ。Gufo一般のWindows向け組込み拡張機構と、依存付き動的moduleの制約を区別する。

## 検証結果

|確認|結果・範囲|
|---|---|
|製品Nix releaseビルド|成功。既存と同じflake.lock/toolchain。b0e2750bのバイナリを実行|
|対象CPU16件|全件成功、合計36.96秒。HTTP解析、scheduler、cache、PLE正常/不正metadata、ASR config等|
|初回GPU14件|全件成功。HC混在形式、Q4 draft、IQ expert、GDN、浮動小数点upload、DFlash2、継続・複数セッション等|
|追加修正後GPU4件|stage-seam / HC / GDN / session-batchが成功|
|HTTP AR|17項目成功、381.47秒|
|HTTP 内蔵PLE＋MTP|17項目成功、315.40秒|
|HTTP 外部Q8 PLE＋MTP|17項目成功、390.97秒|
|HTTP DFlash2|17項目成功、344.13秒|
|通常設定での候補起動|context131072・2セッション・内蔵PLE・DFlash2・mmproj読み込みでHTTP200。draft 53 / accepted 48。画像入力の試験ではない|
|静的確認|整形1244ファイル、拡張API0.3.48、family graph 8 libraries / 13 edges、文書249件 / リンク883件が合格。依存調査の既存legacy警告は別扱い|

HTTPはdiscovery、responses、conversation、structured、tool-streaming、sampling-ranges、long-context、cacheを選択。cacheのキャンセル・再起動復元等の内訳を含め各17レポート項目。所要時間は試験実行時間で、生成性能の値ではない。

### 再現して修正した問題
外部Stagesへ渡すFP32入力を、既存HC/GDNのF16再利用処理が省略していた。実モデルのstage-seamへF32/F16一致確認を追加すると、修正前1b21f08cで61.88秒後に失敗した。同じ確認がb0e2750bで成功。外部Stagesでは完全なFP32入力を渡し、通常GGUFでは従来の省略最適化を維持する。neutral hookのPrefill/Decode bitwise一致・AR/MTP一致も保持。routing hookは公式試験の別演算契約で評価し、全routing logits一致とは扱わない。

### 時間比較と未確定事項
詳細は[要求別・区間別の全結果](timing-comparison.ja.md)。同じモデル・入力・token数・cache履歴・ツールチェーンで比較。通常の3回ベンチや同時2件の性能測定ではない。

- 4構成×短い入力・長い入力・継続入力で、回答hash・prompt/cached/completion token・draft提案/採用数は新旧一致。
- AR・外部PLE・DFlash2は遅延フラグのため実行順を逆に再測定。外部PLEの生成、DFlash2のcache保存の遅延は逆順で再現せず、未確定として保持。
- AR初回Prefillは最初の2組で1465.83→2252.72 ms、1446.55→2442.72 msと遅延。その後、同じ初回入力・cache有効・生成128 tokenで1423.99→1426.50 msとなり再現しなかった。後続の新規入力2件も遅延せず。原因は未特定で、PC負荷や初期化が原因と断定しない。
- cacheを無効にした別条件でも、初回1440.49→1460.07 ms、暖機後1195.95→1188.85 / 1112.80→1139.63 ms。これは生成32 tokenの補助試験で、上の128 token結果と混ぜない。
- 初回遅延を除外した平均や、異なる要求をまとめた速度改善率は作らない。恒常的な演算遅延は今回確認できておらず、根拠不足の最適化変更は追加しない。初回ARのばらつきはレビュー時の注意点として残す。

## コミットと復元
|コミット|内容|
|---|---|
|421e4342|v0.37統合、カスタムのWeightSource/Stages移植、公式Q4_K丸め修正採用|
|1b21f08c|GGUFポインタを前提とするDFlash2へ外部WeightSourceを接続しないようロード前に拒否|
|b0e2750b|外部StagesへのFP32入力契約を修正、実モデル回帰確認を追加|
|bb1c3e7c|互換性・検証範囲の文書整備|

候補イメージ: `tor-o3/gufo:v0.37.0-wsl-b0e2750b-rocm101`。
候補binary SHA256: `811ab44dbe517d57402ae3503f9c2f280ca53c0cdbc4a65185f6e9a74743b274`。
最終復元: `tor-o3/gufo:v0.36.0-wsl-53a217cb-rocm101`。`normal-restored.json`にhealth、実応答、draft、image ID・引数・設定一致、binary SHAを保存。

## 証跡
- `qualification/`、`qualification-stage-before/`、`qualification-stage-fixed/`: GPU結果と修正前の失敗。
- `http/functional/`: 全HTTP要求・判定。`http/functional-results.json`: 経過時間。
- `phases/`、`phases-repeat/`: 新旧・逆順の全履歴。`ar-warm/`、`ar-cache-control/`: AR追加確認。
- `http-window.json`、`phases-repeat-window.json`、`ar-warm-window.json`、`ar-cache-control-window.json`: 停止から通常版の復帰確認までの経過時間。合計約44.4分。ビルドや先行GPU試験の時間は含めない。
- `source-revision.txt`は実行コード、`review-head.txt`は文書込みHEAD。`review.bundle`の検証成功。公開操作はしていない。
- 初期ビルドのGit所有者・alternates・JPEG依存不足、文書行数制限、通常復帰時の一時的connection resetは修復して再確認。失敗ログも保存。未参照だった旧配置の文書・存在しない補助sourceについても読み取り成功とは扱っていない。

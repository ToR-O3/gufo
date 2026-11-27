# Flash-NextのWSL向けHIPグラフ仮実装

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

2026-11-12 JST。v0.33.0統合版 `e55f3b29` を基点に、⑤のDFlash2信頼度カット `94028ef0` と⑱の融合巻き戻し `27999f08` を含む。実測リビジョンは source-revision.txt（ローカル保管：source-revision.txt）。仮実装差分（ローカル保管：diagnostic.patch） は実測後に1行だけ書式調整し、空白を除いた内容が実測版と同一であることを確認した。実測ソース（ローカル保管：source.bundle）には実測版を保存した。

Flash-Nextの `Executor::Create` で既存のプラットフォーム情報からWSLを判定し、`Executor::Run` の既存の直接実行経路を使う。DFlash2共有ドラフト、ターゲット検証、巻き戻し、キャッシュ処理は既存の実装を通る。ネイティブLinuxとWindowsではグラフ有効を選ぶ。`GUFO_TEST_FLASH_NEXT_HIP_GRAPHS=on` はWSL上で同一バイナリの対照を取る試験用。新しい公開CLI・HTTP設定は設けていない。

## 測定条件

ToR-O3 UD-IQ4_XS＋内蔵PLE、WSL＋Docker、gfx1151、ROCm 10.1、`AMD_SERIALIZE_COPY=2`。通常起動の2枠×128K、RAMキャッシュ4 GiB、画像プロジェクターを使う。AR、MTP Q8_0、DFlash2 Q8_0についてグラフ有効・無効で独立したプロセスを起動。両側とも同じHIP呼び出し確認用ライブラリを使い、最初のキャプチャと実行だけを記録する。

各構成で短いウォームアップを行い、約2,048トークンの入力から128トークン生成する単独利用を通常文・反復文で各3回測る。同時利用は、同じ約2,048トークンの入力を事前に2件処理してから、同一入力の2件を同時に各128トークン生成。通常文・反復文で各3回。各プロセスでは単独利用の後に同時利用を行い、グラフ利用後に起きる低速状態も測る。HTTPの要求・SSE応答、キャッシュ済み数、実際のドラフト数・採用数、実行方式を全件保存した。

速度の主値は外れ値除外後の中央値。TTFTはHTTPストリームの最初の出力、同時利用の実配信速度は出力総数を両方の応答が完了するまでの時間で割る。出力照合は本文、生成トークン数、終了理由。ネイティブLinux・Windowsの動作と性能は今回の実測範囲外。

[比較表](../../../../../benchmarks/benchmark/reports/wsl-hip-graphs-20261112/report.ja.html) · [集計JSON](../../../../../evidence/benchmark/runs/wsl-hip-graphs-20261112/summary.json) · 測定ログ（ローカル保管：../../../../benchmark/runs/wsl-hip-graphs-20261112/run.log） · [前回の原因調査](../mtp-c2-cause-20261112/README.ja.md) · [本家v0.33.0のリリース記録](https://github.com/gufo-org/gufo/releases/tag/v0.33.0)

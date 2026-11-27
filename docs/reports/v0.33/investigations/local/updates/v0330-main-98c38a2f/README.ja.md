# Gufo v0.33.0のmain切り替え

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

[ToR-O3/gufoのmain](https://github.com/ToR-O3/gufo/commit/98c38a2f21f015ed4a3c6f1a4c9a7caf9874da14) を別の作業コピーに取得し、Nixのロック指定でリリースをビルド。Gitのツリーは、前回の実機計測版 `4910bc54` と同じ `4a8b26b6dc9c31d575697fce03a44cd3e4c64e02`。

## 起動構成

ToR-O3 UD-IQ4_XS、内蔵PLE、131,072トークン×2セッション。Q8_0 MTPはWSLでの2件同時利用の実測に基づき採用し、最大7・下限1。思考はモデル既定に従う `auto`、RAMキャッシュは実績のある4 GiB。最大出力は32,768トークン。プリフィルの層分割はv0.33の自動選択を使う。APIモデル名は `Qwen3.8-FN-UD-IQ4_XS`。

[公式のFlash-Next設定](https://github.com/gufo-org/gufo/blob/v0.33.0/docs/models/qwen3.8-flash-next.md) / [前回の定例測定](../../../../benchmarks/benchmark/reports/v0330-wsl-eager-20261112/report.ja.html) / ビルドログ（ローカル保管：build.log）

## 実機確認

`Gufo` は `gufo:v0.33.0-wsl-98c38a2f-rocm101` で稼働。実行バイナリの版、`/ready`、`/slots` の2枠・各131,072トークン、RAMキャッシュ実効4 GiBを確認。短い生成ではドラフト112トークン中75を採用。通常文2件の実配信は3回の中央値47.45 tok/sで、単独ARと6/6件一致。長い入力を生成中に処理して8層グループに分割した。

通常起動の記録（ローカル保管：normal-restored.json） / 同時生成（ローカル保管：normal-concurrency.json） / 起動引数（ローカル保管：docker-create.json） / 切り戻し前の設定（ローカル保管：normal-settings.before.json）

最初のビルドではバンドルにHEADが広告されず、複製時のブランチ選択で停止した。`main` を明示した再実行でビルド・起動に成功した。初回ログ（ローカル保管：build-initial-branch-selection.log）

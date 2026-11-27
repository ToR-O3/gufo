# Gufo v0.35.0 02369e4f の最終測定

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

Qwen3.8 Flash-Next（Unsloth v3.1 IQ4_XS ベースの通常モデル）を使い、内蔵PLE＋MTP、外部Q8 PLE＋MTP、内蔵PLE＋DFlash2の3構成を各3回測定した。

- [v0.34との比較・本家参考・メモリ・測定条件](../../reports/v0350-reviewed-02369e4f-20261120/report.ja.html)
- [同じモデルを使った前回v0.35との比較・同日再確認](../../reports/v0350-reviewed-02369e4f-20261120/previous-v0350.ja.html)
- [各回の実測値と出力検証](../../../../evidence/benchmark/runs/v0350-reviewed-02369e4f-20261120/summary.json)・経路別テスト7件（ローカル保管：qualification/passed.json）・測定時間（ローカル保管：timing.json）
- 通常設定での起動、実際の生成応答（ローカル保管：normal-restored.json）

正式測定は2時間38分11秒。同日再確認は23分25秒。内蔵PLEの2構成はそれぞれ全23項目を通過した。外部Q8 PLEの同時生成ではARの4件、MTPの4件と8件の各条件で、単独対照と異なる回答が1件ずつあった。うち前回にない2条件は同日の追加測定（ローカル保管：quality-followup/results.json）で前回版・今回版とも全回答が一致した。

通常用のGufoは `02369e4f` の内蔵PLE＋MTPで稼働を確認した。

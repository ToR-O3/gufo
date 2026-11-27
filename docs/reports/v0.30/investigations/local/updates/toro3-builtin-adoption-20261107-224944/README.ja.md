# ToRO3＋内蔵PLEの通常採用

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

完了：2026-11-07T22:55:53.272810+09:00。通常GufoはToRO3＋内蔵PLEで稼働しています。API名はユーザー指定により `Qwen3.8-FN-UD-IQ4_XS` を維持しました。Hermes等の設定は変更していません。

## 変更内容

- 本体：`/home/user/Models/ToRO3-Qwen3.8-Flash-Next/UD-IQ4_XS/` の4分割GGUF。
- PLE：`--ple-model` を外し、モデル内蔵PLEを使用。外部Q8 PLEファイルは保持。
- イメージ：既にToRO3試験で使用した `gufo:v0.30.0-q5-0-fe85127b-rocm101`。Q5_0 expertとメタデータ専用shard対応のため必要。新しいビルド・モデル加工は実施していません。
- 通常設定：`/home/user/Apps/Gufo/normal-settings.json`。同内容を `normal-settings-toro3.json` に保存。Huihui・Orca・SC117・ASRの保存設定は変更していません。

モデルと外部PLE以外の起動オプションは変更前と一致します。API名、2セッション×131072、thinking auto→on、MTP Q8 draft7/min1、prefill512、RAMキャッシュ4GiB、mmproj、ポート、ROCm10.1、環境変数、マウント、再起動方針on-failure:3を維持しています。

## 今回の確認

verification.json（ローカル保管：verification.json） に確認結果を保存。ready、API名、2×128K、実起動引数、実モデルの4shardと共通MTP/mmprojのマッピングを確認。外部PLE指定・マッピングはありません。通常の思考ON・MTP設定で「こんにちは。」の応答が正常終了しました。Gufo-ASRとStrataは同じ停止状態・設定を維持しています。

モデルの全体SHA256は整理時の記録（ローカル保管：../toro3-cleanup-20261107/result.json）を再利用し、今回はファイルサイズ・更新時刻・inode・モードを照合しました。新たな全体ハッシュ・品質試験・性能ベンチは実施していません。既存の[ToRO3内蔵／外部PLEの3回ベンチ](../../../../../unversioned/benchmarks/benchmark/reports/toro3-ple-20261107-164909/report.ja.html)は、通常の思考ON設定そのものの品質保証を意味しません。

切り替え前の厳密なDocker照合は、未指定DNSとOOM既定値の表現、環境変数の順序差で一度停止しました。意味の同一性を確認して再開し、最終状態で残る差は未指定DNSのnull／空配列のみです。API名の選択前に起動した候補は削除し、従来名で再作成済みです。

## 復旧用記録

変更前設定は normal-settings.json.before（ローカル保管：normal-settings.json.before）、Huihui＋Q8の作成引数は docker-create-before.json（ローカル保管：docker-create-before.json）。旧イメージとモデルは保持しています。旧停止コンテナは動作確認後に削除しました。復旧が必要な場合は稼働要求がないことを確認して通常Gufoを停止・置換し、保存済み引数を使って旧コンテナを再作成、保存済み通常設定を戻します。自動的にHuihuiへ戻さないでください。

# モデルディレクトリ名の変更（2026-11-09）

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

ユーザー指定によりGufoを停止し、モデルディレクトリを変更して通常設定で再起動した。

- 変更前：`/home/user/Models/ToRO3-Qwen3.8-Flash-Next`
- 変更後：`/home/user/Models/ToRO3-Qwen3.8-Flash-Next-Uncensored-GGUF`
- `normal-settings.json` のモデル本体・内蔵PLE参照・モデル保管先を更新。現在の起動引数配列は本ディレクトリの `docker-create.json`。
- `normal-settings-toro3.json` は旧版イメージの記録が残っていたため、現行の通常設定へ同期。両方の変更前設定を保持。
- 同じv0.31.0採用イメージ・API名・内蔵PLE・MTP・thinking auto/on・2セッション×131072・prefill512・RAMキャッシュ4GiB・on-failure:3を維持。
- 停止前の処理中・待機要求は0。モデル4ファイルはrename後もinode・デバイス・サイズ・更新時刻が一致。旧ディレクトリは存在しない。GGUFの内容・ファイル名は変更していない。
- 準備段階の厳密比較はDockerのDNS []/nullとOomKillDisable null/falseの表現差により停止。意味を正規化してその他のHostConfig・イメージ・環境・マウント・引数の一致を確認後、切り替えた。
- `/ready`、APIモデル名、短い生成（返答4、reasoning50 tokens、MTP54提案/38採用）を確認。現在のコンテナはrunning・再起動回数0。
- 退避した旧コンテナは後片付けの確認時点ですでに削除済みだった。Dockerのdestroyイベントを保存したが、操作元は記録から特定できない。現在のGufoは今回起動したIDと一致し、動作とモデル4ファイルに影響がないことを再確認した。
- 配置・起動設定の変更確認であり、性能ベンチマークは実行していない。Gitソース・公開リポジトリへの変更はない。

証跡：`operation.json`、`verification.json`、`container-before.json`、`container-after.json`、`model-files-before.json`、`smoke-request.json`、`smoke-response.json`、`server.log`、`old-container-events.jsonl`。

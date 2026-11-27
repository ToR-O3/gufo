# v0.37保留とv0.36通常復帰

> この文書は各作業段階の記録です。最終判断（2026-11-27）はv0.37の採用保留・外部化見送り、通常利用はv0.36 main、キャッシュ上限4 GiBです。本文の当時の状態・計画・測定値は維持しています。ローカル生ログは同梱せず、公開用コピーでは利用者ホームパスを匿名化しています。

2026-11-27のユーザー指示により、v0.37はレビュー用ブランチに保存し、通常利用はmainのv0.36へ戻した。

- GitHubブランチ: codex/v0370-custom-review、508e1f4ebaeb4ab4c82100a93433f9e8a60464dc。ToR-O3専用認証でpushしremote refを照合。
- main: 53a217cbd8fc9a1a01d5081e91942fd6ecd764bc。変更なし。PR未作成。
- 公開する調査結論は英語でreview-v037.mdへ記録。私用パス・起動設定・ローカル生測定は公開せず従来の保存先に保持。
- 通常稼働: tor-o3/gufo:v0.36.0-wsl-53a217cb-rocm101。既存の検証済みコンテナをGufoへ戻した。
- モデル、内蔵PLE、DFlash2 Q8_0、context 131072、sessions 2、RAM cache 4294967296 bytes、thinking autoを維持。
- ready、モデル一覧、実行バイナリSHA-256、起動引数とDocker設定を照合。短い応答1件でdraft 34、accepted 33。ベンチは実行していない。
- v0.37コンテナはGufo-v0370-review-heldとして停止保持しrestart=no。通常設定JSONはv0.36に復帰。
- ルート作業ツリーの別作業の未コミット変更は未変更。

証拠: verification.json、smoke-response.json、container.after.json。切替前の状態と設定も同じディレクトリに保持。

# バージョン別レポート

Gufo forkのv0.36以前の測定・改修・調査記録を収録しています。各資料は作成当時の記録で、現行仕様とは区別してください。

[現行カスタム台帳](../customizations.md) · [共通ベンチ方針](../benchmark-policy.md)

## バージョン別

- [v0.36](v0.36/README.md) — 5文書
- [v0.35](v0.35/README.md) — 20文書
- [v0.34](v0.34/README.md) — 15文書
- [v0.33](v0.33/README.md) — 8文書
- [v0.31](v0.31/README.md) — 11文書
- [v0.30](v0.30/README.md) — 12文書
- [v0.28](v0.28/README.md) — 7文書
- [v0.27](v0.27/README.md) — 21文書
- [v0.26](v0.26/README.md) — 3文書
- [v0.25](v0.25/README.md) — 2文書
- [v0.23](v0.23/README.md) — 12文書
- [v0.22](v0.22/README.md) — 3文書
- [v0.21](v0.21/README.md) — 1文書
- [v0.19](v0.19/README.md) — 1文書
- [v0.17](v0.17/README.md) — 3文書
- [v0.16](v0.16/README.md) — 2文書
- [v0.15](v0.15/README.md) — 8文書
- [v0.14](v0.14/README.md) — 2文書
- [v0.13](v0.13/README.md) — 9文書
- [v0.12](v0.12/README.md) — 1文書
- [v0.9](v0.9/README.md) — 1文書
- [v0.8](v0.8/README.md) — 5文書
- [v0.7](v0.7/README.md) — 11文書
- [unversioned](unversioned/README.md) — 56文書
- [cross-version](cross-version/README.md) — 2文書

## 保存方針

- 版番号はディレクトリ、本文、同じ測定の併設資料で確認したものを使用。測定日だけから推定していません。
- パッチ版・比較対象・開発コミットは原文に保持しています。版を特定できないものはunversioned、版をまたぐ台帳等はcross-versionへ分離しました。
- 完全に同じ内容の複製は一つにまとめ、[対応表](manifest.json)で元ファイルを追えます。
- HTMLはダウンロードしてブラウザーで開けます。測定値は再計算していません。
- 個人ホームパスを匿名化。個人用設定、認証情報、巨大な生ログ、生成物や公式資料の複製は対象外です。
- 同梱していない参照はローカル保管と明示し、[参照一覧](local-references.json)に記録しました。未同梱は確認成功や試験合格を意味しません。
- v0.37の資料はレビュー用ブランチに保持し、このmainには含めません。

## 対象範囲

保存済みのbenchmark/reports、benchmark/runs、local/updates、local/reportsにあるGufoのレポート・作業文書、および過去のコミット別台帳を対象にしました。モデル回答の生データや試験fixture、他アプリの個人設定、取得した公式文書、コピーしたソースツリーは除外しています。

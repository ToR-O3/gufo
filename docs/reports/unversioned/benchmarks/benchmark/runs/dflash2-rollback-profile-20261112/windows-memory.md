# windows-memory

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

| 条件 | Windows観測数 | Windows空き GiB 中央値 / 最小 | VmmemWSL GiB 中央値 / 最大 |
|---|---:|---:|---:|
| rollback-ar/ar | 6 | 14.245 / 13.754 | 12.515 / 12.626 |
| rollback-forward/auto-fused | 4 | 14.471 / 14.358 | 12.612 / 12.727 |
| rollback-forward/auto | 5 | 14.407 / 14.157 | 12.706 / 12.739 |
| rollback-reverse/auto-fused | 3 | 14.652 / 14.590 | 12.717 / 12.738 |
| rollback-reverse/auto | 3 | 14.770 / 14.536 | 12.433 / 12.732 |
| rollback-c2/ar | 3 | 13.977 / 13.870 | 12.269 / 12.287 |
| rollback-c2/auto-fused | 3 | 13.655 / 13.479 | 12.512 / 12.519 |
| rollback-c2/auto | 4 | 13.750 / 13.444 | 12.445 / 12.469 |
| rollback-state/auto-fused | 2 | 17.134 / 16.687 | 9.964 / 10.433 |
| rollback-state/auto | 2 | 15.948 / 15.906 | 11.099 / 11.287 |

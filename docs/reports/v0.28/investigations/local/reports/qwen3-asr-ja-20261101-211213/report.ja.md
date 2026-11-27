# Qwen3-ASR-1.7B-JA 導入・動作確認

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

2026-11-01。Huihuiを停止・再起動せず、日本語ASRを別のGufoコンテナとして追加しました。Hermesの設定は未変更です。

## 保存先と起動構成

- 配布元：[neosophie/Qwen3-ASR-1.7B-JA](https://huggingface.co/neosophie/Qwen3-ASR-1.7B-JA)。リビジョン `987bda160f2dabfa6757550bcff7cdda2ba0648c`。
- 保存先：`/home/user/Models/Qwen3-ASR-1.7B-JA`。配布物13ファイル、合計4,092,095,812 bytes（約3.81 GiB）。追加の元Qwenモデルはダウンロードしていません。
- モデル重みSHA-256：`4e378e29a060f4a8464a59976a6b84661dc2f88f23a3f43634651de3fc84ccb4`。全取得ファイルの公開ハッシュ照合済み。
- ASRコンテナ：`Gufo-ASR`。イメージ：`gufo:v0.28.1-asr-ja-4336a8d5-rocm101`。
- API：`http://127.0.0.1:18081/v1/audio/transcriptions`。モデル名：`Qwen3-ASR-1.7B-JA`。
- ROCm 10.1、既存WSLライブラリ、`AMD_SERIALIZE_COPY=2`、`GPU_MAX_HW_QUEUES=1`、context 1024、Docker `on-failure:3`。ポートはlocalhostのみ公開。
- 設定ファイル（ローカル保管：/home/user/Apps/Gufo/asr-settings.json） / 再作成コマンド（ローカル保管：../../evidence/deployments/qwen3-asr-ja-20261101-211213/docker-create.json）。停止は `docker stop Gufo-ASR`、再開は `docker start Gufo-ASR`。

## 変更内容

コミット：`4336a8d5cac97a3d4ce4d21d31147d12fbce6870`（`feat(asr): support single-file checkpoints with tied output weights`）。ソース（ローカル保管：../../../..）。

Gufoが想定していた2分割に加え、`model.safetensors` 1ファイルを読み込みます。`thinker_config.text_config.tie_word_embeddings=true` で出力重みが省略されている場合は、そのモデル自身の埋め込みを同じメモリ上で参照します。明示的な出力重みがあれば優先します。配布された重み、計算カーネル、Huihuiの実行ファイルは変更していません。

整形チェック722ファイル、設定と保存形式のCPUテスト2件、実モデルのローダーテストが合格。配布物707テンソルに共有名1件を追加し、読み込み時の見かけは708件になります。重みの複製ではありません。

## 動作確認

| 確認 | 結果 |
| --- | --- |
| 日本語合成音声7.59秒、3回 | 全文一致。0.890 / 0.554 / 0.571秒 |
| 公式の英語音声10.00秒 | 参照文と全文一致。0.634秒 |
| Huihuiの生成開始後にASRを呼び出し | 両方完了。ASR約0.856秒、文字起こし一致 |
| 整理後に通常ASRイメージを再起動 | ready・日本語文字起こし一致 |
| Huihuiの維持 | コンテナID、開始日時、イメージ、引数が作業開始時と同じ。ready |

日本語音声はWindows内蔵Microsoft Haruka Desktopで生成した機能確認用です。一般的な日本語の認識精度を測るベンチではありません。初回をウォームアップとして除外した正式な3回ベンチも未実施です。並行確認はLLMの文字出力期間内にASR処理が完了した確認で、GPUカーネルの同時実行を直接測ったものではありません。

## メモリ

ビルド成果物と一時コンテナを整理してから、ASRだけを止めて再起動した際のWindows全体の値です。OSから見える物理メモリは約63.65 GiB。GPU予約分を含む全実装RAMの値ではなく、ASR単独の使用量でもありません。

| 状態（日本時間） | Windows使用 | Windows空き |
| --- | ---: | ---: |
| ASR追加前（21:35:52） | 52.41 GiB | 11.23 GiB |
| ASR追加後の最終観測（21:37:51） | 56.54 GiB | 7.10 GiB |
| 今回の最終起動で観測した空き最小（21:36:04） | 56.96 GiB | 6.68 GiB |

この前後比較では空きが約4.13 GiB減りました。他のアプリも動作中のため、全量をASRだけに帰属する値ではありません。WSL/GPUカウンターは同じ物理メモリと重なるので加算しません。約5～6秒間隔の観測であり、瞬間的なピークや長時間音声・長文LLM負荷の上限は未測定です。

整理前の作業全体では空き最小2.51 GiBを観測しました。ビルド後のファイル等を整理すると回復したため、この最小値を通常ASRだけの負荷とは扱いません。2 GiB未満でASRだけを停止する試験中の監視は発動しませんでした。常設監視としては設定していません。

## 失敗・未確認事項

初回の通常イメージ起動は `cannot make Qwen3-ASR safetensors GPU-visible` で失敗しました。OOMKilledではありません。診断付き起動ではGPUメモリ確保・転送が成功し、その後の診断なし通常イメージ2回の起動も成功しています。初回の原因は未特定であり、根本修正済み・長期安定性確認済みとは扱いません。診断コードは採用せず、初回ログ（ローカル保管：../../evidence/deployments/qwen3-asr-ja-20261101-211213/logs/asr-first-failed.log）と診断ログ（ローカル保管：../../evidence/deployments/qwen3-asr-ja-20261101-211213/logs/asr-diagnostic.log）を保持しています。

Hermesへの接続先変更とマイク入力は未実施です。GufoのこのAPIはWAV入力です。今後Hermesを接続する際は、OpenAI互換STT接続先とモデル名の指定に加え、M4Aへ変換する無音除去設定を無効化する等、WAVを維持する必要があります。

## 記録

検証データ（ローカル保管：../../evidence/deployments/qwen3-asr-ja-20261101-211213/checks/smoke-results.json） / 実行期間が重なった確認（ローカル保管：../../evidence/deployments/qwen3-asr-ja-20261101-211213/checks/stream-overlap.json） / メモリ集計（ローカル保管：../../evidence/deployments/qwen3-asr-ja-20261101-211213/checks/memory-summary.json） / 最終確認（ローカル保管：../../evidence/deployments/qwen3-asr-ja-20261101-211213/checks/final-verification.json）。一時ビルド成果物、キャッシュ、診断実行ファイル、ビルド用コンテナを削除済み。コミット済みソース、起動設定、検証ログ、モデルは保持しています。

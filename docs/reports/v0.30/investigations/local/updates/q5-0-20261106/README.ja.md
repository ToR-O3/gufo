# Unsloth v3.1用Q5_0対応

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

2026-11-06（JST）。通常構成 `c1170a4a` に必要な形式対応を追加し、GPU演算・未加工Unsloth v3.1の通常生成／MTP生成が合格した。通常のGufoは元のHuihui＋外部Q8 PLE構成へ復旧済み。新イメージは試験用として保持しており、通常運用には採用していない。実機試験結果（ローカル保管：runtime-result.json）、復旧後の照合（ローカル保管：restoration-verification.json）。

## 実装

| コミット | 変更 | 理由 |
|---|---|---|
| `1f2fe101` | テンソル数ゼロのGGUFではデータ領域の位置合わせを行わない | Unslothの第1ファイルはメタデータだけで終端に6バイトのpaddingがなく、変更前は読込に失敗した |
| `fe85127b` | Flash-NextのQ5_0 expertを既存MMQ経路へ接続 | 取得したGGUFのdown expert 33本がQ5_0であるため |

Q5_0の追加は既存の `qfn_mmq_q5_1_moe_raw` 等の命名・構造に合わせた `qfn_mmq_q5_0_moe_raw`、型の許可一覧、CPU展開とGPU dispatchの分岐に限定した。内積は既存の `vec_dot_q5_0_q8_1`、prefillは既存の `mul_mat_q_case<GGML_TYPE_Q5_0>` を使用する。Q5_0を既存MMQへ送るため、独自のWMMAカーネル・実行時スイッチ・重み変換は追加していない。既存型の演算・分岐は維持した。

読込修正は独立したコミットに分けた。正常な未整列メタデータ、先頭／後続ファイルからのsplit発見、途中で切れたメタデータの拒否を回帰テストで確認する。テンソルがあるファイルの範囲・alignment検証は従来どおり行う。

## ビルドと検証

- 作業ブランチ：`codex/qwen38-q5-0`。
- 試験イメージ：`gufo:v0.30.0-q5-0-fe85127b-rocm101`。
- 実行ファイルSHA-256：`18cbb29468e03866d8abbafe8712c70b5fe94f53e2dc049f7078ef8eb99eaa18`。
- 前回と同じNixのGCC 16.2.0・ROCm clang 24.0.0／ROCm 10.1・gfx1151を使用。既存のCMake構成はRelWithDebInfo。テストの判定はassert無効化に依存しない。
- CPU：GGUF reader、GGUF identity、native quantが成功。新規Q5_0行検査ではFP16スケールの正負、5bit全コード、行・expert strideを独立に組み立てた期待値と照合した。
- 修正前のmetadata回帰テストが失敗し、修正後に成功。実GGUFは変更前に `Malformed or truncated GGUF`、変更後に1,224テンソルと共通MTPの構成一致を確認した。
- 共有整形チェック：892ファイル成功。
- GPU：Q5_0の40条件すべてで独立したFP32期待値と一致し、最大絶対誤差は0。既存native形式・IQ形式の演算試験も成功。native結果（ローカル保管：native_expert_ops.log）、IQ結果（ローカル保管：iq_expert_ops.log）。
- 実モデル：通常生成／MTP生成それぞれでAPI discovery、日本語64トークン生成、再実行、2同時要求、簡単な算術が成功。同一モードの単独／再実行／同時要求、両モード間の日本語回答ハッシュが一致。算術は両方とも `5` を返した。通常生成（ローカル保管：off-checks.json）、MTP生成（ローカル保管：mtp-checks.json）。
- 標準ベンチ測定関数による限定試験：両モードでウォームアップ1回＋正式3回を完了。正式3入力の実トークン数は2,032／2,053／2,023、生成はすべて128トークン。MTPのdraft発生を確認し、3回すべてで通常生成と回答ハッシュが一致した。

GPUではK=640/2560、M=7/640/2560、1/8/9/32/33トークン、expert 0/511と無効ID、downの1入力1expert、paired投影を確認した。期待値は製品のdecoderを使わず量子化コードから作った。paired検査では同じ行列を両出力へ渡して一致を確認しており、異なる2行列の組み合わせを独立に検証したものではない。

実モデルは未加工Unsloth v3.1・内蔵PLE・8K context・2セッション・prefill chunk512・256MiB RAM cacheを対象とし、通常生成と共通Q8 MTP（draft7/min1）を確認した。測定はpp約2048/tg128、depth0、greedy、thinking off。生成速度の3回平均は通常29.39 tok/s、MTP32.53 tok/sだった。旧版はこのGGUFを読めないため、同一モデルでの改修前後の速度比較はできていない。深いcontext・全モデル・一般的な回答品質の合格を意味しない。性能・メモリ・各測定値は [ベンチ記録](../../../../benchmarks/benchmark/runs/q5-0-20261106/README.ja.md) を参照する。

## 通常サービスと再開点

ユーザーの指定どおり、稼働中の推論が終わった後に一時停止して試験した。終了後に元の同一Gufoコンテナを起動し、構成・HostConfig・通常設定ファイルの一致、ready、日本語応答を確認した。通常イメージは `gufo:v0.30.0-reviewed-c1170a4a-rocm101`、Huihui＋外部Q8 PLEのまま。ASR・Strataのコンテナと状態も不変だった。

Luni差分の取得・GGUFの重み加工は行っていない。未加工版の実機確認記録は `/home/user/workspace/LLM/heretic-luni-v31/reports/unmodified-gufo-check.json`。次の差分取得・数値検証から再開できる。加工版の試験では今回のQ5_0対応イメージを使用する必要があり、復旧した通常イメージではこのGGUFは読めない。

## 失敗・復旧の記録

- ビルド環境確認のPATH不足は、既存Nix環境のPython・clang-format・coreutilsを使って解消。最初の整形チェック失敗と成功ログを両方保存。
- 最初のDocker image buildは、sandboxからbuildxの活動記録へ書き込めず失敗。同じビルドを許可された昇格実行で成功。
- Windowsメモリ統計は通常sandboxのWSL vsock接続で失敗し、昇格実行で取得できた。WindowsがOSへ公開するメモリ、WSL割当、HIPの全体プールは集計範囲が異なるため、足し合わせない。
- 通常Gufoの処理中に始めようとしたoperator試験は、起動前のbusy検査で停止した。GPU検査の失敗として数えない。

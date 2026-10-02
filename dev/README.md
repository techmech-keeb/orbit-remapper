# 開発者向け

Orbit Remapper を作る人・試す人向けの入口です。使い方は [docs/](../docs/README.md) にあります。

## リポジトリの構成

| 場所 | 内容 |
| --- | --- |
| [`firmware/orbit/`](../firmware/orbit/README.md) | Orbit のファームウェア（ESP-IDF）。構成、ビルド、ログの読み方、遅れの測り方 |
| [`firmware/hid-remapper/`](../firmware/hid-remapper/UPSTREAM.md) | 本家 HID Remapper（git subtree、無改造）。取り込んだ版は `UPSTREAM.md` |
| [`docs/`](../docs/README.md) | 使う人向けの文書と画像 |
| `dev/drafts/` | 開発の記録。日付ごとのフォルダで、確定版ではない |
| [`dev/experiments/q31-s3-two-ble/`](experiments/q31-s3-two-ble/README.md) | Q31 の実験（M5Dial で BLE 機器 2 台を 7.5 ms で受ける試験プログラム） |
| [`dev/tools/serial-capture/`](tools/serial-capture/README.md) | ログの COM ポートをファイルに残す PowerShell のスクリプト |
| `dev/artifacts/drafts/` | 当初の筐体案（E1）の CAD と検査記録。今の構成ではなく、経緯として残している |

## ビルド

ESP-IDF v5.5.5 で、`firmware/orbit/` から：

```sh
idf.py build merge-bin
```

詳しくは [firmware/orbit/README.md](../firmware/orbit/README.md#2-ビルドと書き込み)。

## 記録の読み方

最初に読むもの：

1. [要件と決定](drafts/2026-09-26/requirements.md)
2. [実装の設計](drafts/2026-09-27/implementation-design.md)
3. [未解決事項](drafts/2026-09-26/open-questions.md)
4. [設計の経緯](drafts/2026-09-26/history.md)

段階ごとの依頼書と結果：

| 段階 | 依頼書 | 結果 |
| --- | --- | --- |
| Q31（実験） | [q31-experiment-brief.md](drafts/2026-09-26/q31-experiment-brief.md) | [q31-results.md](drafts/2026-09-27/q31-results.md) |
| M1（本家をそのまま動かす） | [m1-brief.md](drafts/2026-09-27/m1-brief.md) | [m1-results.md](drafts/2026-09-27/m1-results.md) |
| M2（台帳、機器ごとの設定） | [m2-brief.md](drafts/2026-09-30/m2-brief.md) | [m2-results.md](drafts/2026-09-30/m2-results.md) |
| M3（画面） | [m3-screen.md](drafts/2026-10-01/m3-screen.md) | 同じ文書 |

2026-09-26 の資料の一覧は [drafts/2026-09-26/README.md](drafts/2026-09-26/README.md)、実機での試験の手順は [m1-handoff.md](drafts/2026-09-27/m1-handoff.md) にあります。

## 実機の試験の流れ

1. 変更をビルドし、`merged-binary.bin` を試験する人に渡す（`.bin` はリポジトリに入れない）。
2. 試験する人が書き込み、`dev/tools/serial-capture/` でログを残し、手順書のとおりに試す。
3. 結果を `dev/drafts/2026-09-27/reports/` に `<段階>-<版>-report.md` の名前で書く（例：`m2-4903ae1-report.md`）。ログの全文は入れず、要る行だけを抜き出す。

## 守ること

- 本家のコア（`firmware/hid-remapper/`）は改造しない。やむを得ず直すときは、変更箇所に `// ORBIT:` の印を付ける。
- BTstack 由来のコードは持ち込まない（ライセンスのため）。
- 他人のコードや素材は、ライセンスを確かめ、出典を書く。
- ログに、機器の完全なアドレス、ペアリングの鍵、M5Dial の MAC アドレスを出さない（アドレスは下位 2 バイトだけ）。
- `.bin`、ビルドの生成物（`build/`、`sdkconfig`、`managed_components/`）、ログの全文はリポジトリに入れない。
- 報告に、PC の名前、ユーザー名、`C:\Users\...` のようなパス、個人や会社の名前を書かない。

## PR の番号について

`dev/drafts/` に出てくる「PR #16」などの番号は、2026-10-02 に公開するまで使っていた非公開の開発リポジトリのものです。このリポジトリの PR とは対応しません。そのころの変更は、`main` の履歴のマージコミット（「Merge pull request #16 …」）でたどれます。

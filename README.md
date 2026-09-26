# Orbit Remapper — DRAFT

HID Remapperを基盤に、画面と操作部を備えた卓上リマッパーを検討するプロジェクトです。

**現在は設計・開発検討のドラフトです。完成品、動作確認済みファーム、製造承認済みデータの公開ではありません。**

Built on [HID Remapper](https://github.com/jfedor2/hid-remapper) by jfedor2.
本家から派生した独立プロジェクトとして検討しています。本家の公式モデル・承認済み製品ではありません。

## まず読む

- [ドラフト資料の入口](docs/drafts/2026-09-26/README.md)
- [要求と決定状況](docs/drafts/2026-09-26/requirements.md)
- [デザイン・構成の検討経緯](docs/drafts/2026-09-26/history.md)
- [E1 / M5Dial / S3の構成比較](docs/drafts/2026-09-26/architecture.md)
- [未解決事項と確定条件](docs/drafts/2026-09-26/open-questions.md)
- [成果物一覧と原本の位置づけ](docs/drafts/2026-09-26/artifacts.md)

## 構成

| 構成 | 状態 |
| --- | --- |
| XIAO nRF52840 Plus（本家 BLE 版で BLE 受信・リマップ・PC への USB 出力）＋初代 M5Dial（画面とダイヤル） | **2026-09-26 採用。** 入力は BLE 機器のみ（同時 4 台、7.5 ms を目指す）。有線 USB 機器は対象外。透過表示の要件（R02）は取り下げ。ファーム、筐体は未着手・未検証。 |
| M5Dial 単体（ESP32-S3 が BLE 受信・リマップ・画面操作・PC への USB 出力） | 同日に検討し、4 台・7.5 ms の要件から XIAO＋M5Dial に置き換え。 |
| M5Dial + XIAO（S3がUIとUSBホストを兼任） | 同日に検討し、M5Dial 単体の構成に置き換え。 |
| E1 Orbital Pod + Glass2 + XIAO + Pico | 機構試作案v0.3のCAD等を経緯として保存。現行の構成ではない。 |

## リポジトリ

- [orbit-remapper](https://github.com/techmech-keeb/orbit-remapper): 製品全体の資料・独自開発を置く場所。
- [hid-remapper](https://github.com/techmech-keeb/hid-remapper): 本家由来コードの開発先。

この資料追加では、HID Remapper側のコード変更は行っていません。
名称はリポジトリ名として使用していますが、正式なブランド確定・商標確認を済ませた意味ではありません。

## ドラフトの扱い

2026-09-26に、一定の完成度に達するまで非公開で開発する方針へ変更しました。この資料の格納先 `orbit-remapper` はprivateです。`hid-remapper` は現在publicなので、独自の未公開実装は追加せず、そちらの開発開始前に非公開の作業先を用意します。公開時期・公開範囲は未確定です。

将来はオープンソースとして公開する前提です。そのため、このリポジトリの履歴には公開できる物だけを入れます。非公開の原本（元BOM、引き継ぎ文書など）は別の private リポジトリに置き、第三者の素材は出典のリンクだけを残します（[成果物一覧](docs/drafts/2026-09-26/artifacts.md)）。

検討資料は `docs/drafts/`、当時の成果物は `artifacts/drafts/` に保存します。
ユーザーが内容を確定した後、対象文書ごとに確定版へ整理します。Draft PRのマージと、製品仕様・製造承認は別の判断です。

## ライセンス

公開前のため、対象ごとのライセンスはまだ確定していません。

| 対象 | ライセンス |
| --- | --- |
| ソフトウェア（CAD・描画の生成スクリプト、今後のファームウェア・ツール） | [MIT License](LICENSE) |
| ハードウェア設計（STEP、DXF、今後の基板データ） | 未確定（CC BY 4.0 または CERN-OHL-P を検討） |
| 文書 | 未確定（CC BY 4.0 を検討） |
| 第三者の素材 | 各権利者のライセンスに従う。このリポジトリには含めない |

HID Remapper 由来のコードを取り込む場合は、本家の著作権表示（Copyright (c) 2023 Jacek Fedorynski）と MIT License の条文を残します。

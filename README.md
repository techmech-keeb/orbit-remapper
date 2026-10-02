# Orbit Remapper

初代 M5Dial（ESP32-S3）1 台で、Bluetooth のキーボードやマウスを受け取り、[HID Remapper](https://github.com/jfedor2/hid-remapper) のコアでリマップして、USB で PC に出す卓上リマッパーです。丸い画面とダイヤルで、つながっている機器の状態を見たり、ペアリングを管理したりできます。

![初代 M5Dial の画面。緑のリングの内側に「ORBIT 3/15」、IST TrackBall（P1、7.50 ms）と MISTEL-1（P3、7.50 ms）の 2 枚のカード、「2 devices connected」、「USB mounted e72c27a」が表示されている。右にトラックボール、奥にキーボード](docs/images/orbit-m5dial-2026-10.jpg)

*初代 M5Dial で IST Trackball と Mistel MD600 をつないだところ（版 `e72c27a`、2026-10）。*

Built on [HID Remapper](https://github.com/jfedor2/hid-remapper) by jfedor2. 本家から派生した独立プロジェクトで、本家の公式モデルや承認済みの製品ではありません。

**開発中です。** 動作は作者の手元の機器（IST Trackball、Cube Turner PRO、Mistel MD600 など）で確かめた範囲に限られます。製品として完成したものではありません。

## できること（2026-10 時点）

- Bluetooth LE の HID 機器を同時に 2 台、7.5 ms の間隔で受け取る。
- 本家 HID Remapper のコア（無改造）でリマップする。設定は本家の Web 設定ツール（https://www.remapper.org/config/ ）でそのまま行える。
- 機器ごとに固定のポート番号を振る台帳。1 台ずつの削除、上限 15 台、アドレスが変わった機器の引き継ぎ。
- 登録済みの機器が組み直しを求めたときは、本体で許可するまでつながない。
- 画面：状態の色のリング、機器のカード、ダイヤルで操作するメニュー（ペアリング、削除、画面の回転、画面を消すまでの時間）。

これからの予定（機器ごとのレイヤー、切断時の押しっぱなしの解除、設定セット、LED の送り返し）は [M2 の依頼書](dev/drafts/2026-09-30/m2-brief.md) にあります。

## 使い方と作り方

- ファームウェアのビルド、書き込み、ログの読み方：[firmware/orbit/README.md](firmware/orbit/README.md)
- 取り込んだ本家のコードと版：[firmware/hid-remapper/UPSTREAM.md](firmware/hid-remapper/UPSTREAM.md)

## 資料

開発の記録は `dev/drafts/` にあります。日付ごとのフォルダで、確定版ではありません。

- [要件と決定](dev/drafts/2026-09-26/requirements.md)
- [実装の設計](dev/drafts/2026-09-27/implementation-design.md)
- [M1 の結果](dev/drafts/2026-09-27/m1-results.md)、[M2 の結果](dev/drafts/2026-09-30/m2-results.md)
- [画面（M3）](dev/drafts/2026-10-01/m3-screen.md)
- [未解決事項](dev/drafts/2026-09-26/open-questions.md)
- [設計の経緯](dev/drafts/2026-09-26/history.md)
- 実機の試験の報告：`dev/drafts/2026-09-27/reports/`

当初の筐体案（E1）の CAD と検査記録は `dev/artifacts/drafts/` に経緯として残しています。今の構成ではありません。

## 注意

- **PR の番号**：資料に出てくる「PR #16」などの番号は、2026-10-02 に公開するまで使っていた非公開の開発リポジトリのものです。このリポジトリの PR とは対応しません。そのころの変更は、`main` の履歴のマージコミット（「Merge pull request #16 …」）でたどれます。
- **USB の VID/PID と製品名**：今は本家の Web 設定ツールをそのまま使うため、本家と同じ `0xCAFE` / `0xBAF2` と「HID Remapper Bluetooth」という製品名を名乗ります。正式な割り当てではありません（[Q35](dev/drafts/2026-09-26/open-questions.md)）。
- **名前**：「Orbit」はリポジトリ名として使っているだけで、商標の確認はしていません。
- **ログ**：ファームウェアのログには、機器のアドレスの下位 2 バイトだけを出します。ペアリングの鍵や M5Dial の MAC アドレスは出しません。

## ライセンス

| 対象 | ライセンス |
| --- | --- |
| ソフトウェア（ファームウェア、ツール、CAD・描画の生成スクリプト） | [MIT License](LICENSE) |
| ハードウェア設計（STEP、DXF、今後の基板データ） | [CERN Open Hardware Licence Version 2 - Permissive](LICENSE-HARDWARE)（CERN-OHL-P-2.0） |
| 文書と画像（`docs/` と各 README、`docs/images/` の写真） | [Creative Commons Attribution 4.0 International](LICENSE-DOCS)（CC BY 4.0） |
| 本家 HID Remapper のコード（`firmware/hid-remapper/`） | 本家の [MIT License](firmware/hid-remapper/LICENSE)（Copyright (c) 2023 Jacek Fedorynski）。一部のファイルは各自の表示に従う |
| ビルド時に取得する部品（ESP-IDF、NimBLE、TinyUSB、LVGL など） | 各部品のライセンス。このリポジトリには含めない |
| 第三者の素材（メーカーの写真、寸法図、フォント） | このリポジトリには含めない。出典のリンクだけを残す |

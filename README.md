# Orbit Remapper

初代 M5Dial（ESP32-S3）1 台で、Bluetooth のキーボードやマウスを受け取り、[HID Remapper](https://github.com/jfedor2/hid-remapper) のコアでリマップして、USB で PC に出す卓上リマッパーです。丸い画面とダイヤルで、つながっている機器の状態を見たり、ペアリングを管理したりできます。

![初代 M5Dial の画面。緑のリングの内側に「ORBIT 3/15」、IST TrackBall（P1、7.50 ms）と MISTEL-1（P3、7.50 ms）の 2 枚のカード、「2 devices connected」、「USB mounted e72c27a」が表示されている。右にトラックボール、奥にキーボード](docs/images/orbit-m5dial-2026-10.jpg)

*初代 M5Dial で IST Trackball と Mistel MD600 をつないだところ（版 `e72c27a`、2026-10）。*

Built on [HID Remapper](https://github.com/jfedor2/hid-remapper) by jfedor2. 本家から派生した独立プロジェクトで、本家の公式モデルや承認済みの製品ではありません。

**開発中です。** 動作は作者の手元の機器（IST Trackball、Mistel MD600 など）で確かめた範囲に限られます。製品として完成したものではありません。

## できること（2026-10 時点）

- Bluetooth LE の HID 機器を同時に 2 台、7.5 ms の間隔で受け取る。
- 本家 HID Remapper のコア（無改造）でリマップする。設定は本家の Web 設定ツール（https://www.remapper.org/config/ ）でそのまま行える。
- 機器ごとに固定のポート番号を振る台帳。1 台ずつの削除、上限 15 台、アドレスが変わった機器の引き継ぎ。
- 登録済みの機器が組み直しを求めたときは、本体で許可するまでつながない。
- 画面：状態の色のリング、機器のカード、ダイヤルで操作するメニュー（ペアリング、削除、画面の回転、画面を消すまでの時間）。

これからの予定（機器ごとのレイヤー、切断時の押しっぱなしの解除、設定セット、LED の送り返し）は [M2 の依頼書](dev/drafts/2026-09-30/m2-brief.md) にあります。

## 使い方

- [はじめに](docs/getting-started.md)：用意するもの、書き込み、最初のペアリング
- [ふだんの使い方](docs/using.md)：画面の見方、ダイヤルの操作、機器の追加と削除
- [リマップの設定](docs/configuration.md)：本家の Web 設定ツールでの設定、機器ごとに分ける方法
- [困ったとき](docs/troubleshooting.md)

ファームウェアは [Releases](https://github.com/techmech-keeb/orbit-remapper/releases) から取れます（`Orbit_Remapper_firmware_v<版>_M5Dial.bin`）。変更の履歴は [CHANGELOG.md](CHANGELOG.md) にあります。自分でビルドすることもできます（[firmware/orbit/README.md](firmware/orbit/README.md#2-ビルドと書き込み)）。BIOS の画面で使えるか、PC をスリープから起こせるかは、まだ確かめていません。

## 開発に加わる人へ

ファームウェアの構成、開発の記録、実機の試験の流れは [dev/README.md](dev/README.md) にあります。

## 注意

- **USB の VID/PID と製品名**：今は本家の Web 設定ツールをそのまま使うため、本家と同じ `0xCAFE` / `0xBAF2` と「HID Remapper Bluetooth」という製品名を名乗ります。正式な割り当てではありません（[Q35](dev/drafts/2026-09-26/open-questions.md)）。
- **名前**：「Orbit」はリポジトリ名として使っているだけで、商標の確認はしていません。
- **ログ**：ファームウェアのログには、機器のアドレスの下位 2 バイトだけを出します。ペアリングの鍵や M5Dial の MAC アドレスは出しません。

## ライセンス

| 対象 | ライセンス |
| --- | --- |
| ソフトウェア（ファームウェア、ツール、CAD・描画の生成スクリプト） | [MIT License](LICENSE) |
| ハードウェア設計（STEP、DXF、今後の基板データ） | [CERN Open Hardware Licence Version 2 - Permissive](LICENSE-HARDWARE)（CERN-OHL-P-2.0） |
| 文書と画像（`docs/`、`dev/` の文書と各 README、`docs/images/` の写真） | [Creative Commons Attribution 4.0 International](LICENSE-DOCS)（CC BY 4.0） |
| 本家 HID Remapper のコード（`firmware/hid-remapper/`） | 本家の [MIT License](firmware/hid-remapper/LICENSE)（Copyright (c) 2023 Jacek Fedorynski）。一部のファイルは各自の表示に従う |
| ビルド時に取得する部品（ESP-IDF、NimBLE、TinyUSB、LVGL など） | 各部品のライセンス。このリポジトリには含めない |
| 第三者の素材（メーカーの写真、寸法図、フォント） | このリポジトリには含めない。出典のリンクだけを残す |

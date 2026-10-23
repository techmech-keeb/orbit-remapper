# Changelog

Orbit Remapper のファームウェアの変更履歴です。形式は [Keep a Changelog](https://keepachangelog.com/ja/1.1.0/) に従います。版の番号は [`firmware/orbit/version.txt`](firmware/orbit/version.txt) と対応し、上げ方は [dev/README.md](dev/README.md#版とリリース) にあります。

## [Unreleased]

### 追加

- 機器の電池の残量（Bluetooth の Battery Service）を読み、画面のカードに電池の印と % で出す。30% 未満は黄、10% 以下は赤。10% 以下になったときは状態の行で 1 回知らせる。通知を送らない機器は 10 分ごとに読み直す。値はメモリだけに置き、機器が切れたら消す。
- ログの `orbit list` に `battery=`、設定ツール向けの `GET_STATE` に枠ごとの残量を足した。
- 画面のメニューに「Open config page」を足した。PC の Chrome でアドレス欄をクリックしてから中央を押すと、Orbit がキーボードとして本家の Web 設定ツールのアドレス（`www.remapper.org/config/`）と Enter を打つ。US 配列と日本語配列で同じキーになる文字だけを使う。ログの命令 `orbit openconfig` でも打てる。

### 変更

- 設定ツールとのやりとりの版（`PROTOCOL_VERSION`）を 2 にした。`GET_STATE` の最後の 2 バイトが電池の残量になった（版 1 では 0）。

## [0.1.0] - 2026-10-02

最初の公開版です。初代 M5Dial で、Bluetooth LE のキーボードやマウスを 2 台まで受け取り、本家 HID Remapper のコア（無改造）でリマップして USB で PC に出します。

### 追加

- Bluetooth LE の HID 機器を同時に 2 台、7.5 ms の間隔で受け取る。
- 本家 HID Remapper のコアでリマップする。設定は本家の Web 設定ツールで行える。
- 機器の台帳。機器ごとに 1〜15 の固定のポート番号を付ける。1 台ずつの削除、上限 15 台、アドレスが変わった機器の番号の引き継ぎ。
- 登録済みの機器が新しい鍵でつなぎ直そうとしたときは、本体で許可するまでつながない。
- 画面：状態の色のリング、機器のカード、ダイヤルで操作するメニュー（ペアリング、削除、画面の回転、画面を消すまでの時間）。
- 書き込みモードに入る方法：ダイヤルの中央を押したまま起動する、ログの COM ポートを 1200 bps で開いて閉じる。
- USB の bcdDevice に版の番号を入れる（0.1.0 → 0x0010）。

### 既知の制約

- 動作を確かめたのは、作者の手元の機器（IST Trackball、Mistel MD600 など）だけです。
- BIOS の画面で使えるか、PC をスリープから起こせるか（リモートウェイクアップ）は確かめていません。
- 機器ごとのレイヤー、機器が切れたときの押下の解除、キーボードの LED の送り返しは、まだありません。
- 本家の設定ツールで機器ごとの設定に選べるポートは 1〜8 です。

[Unreleased]: https://github.com/techmech-keeb/orbit-remapper/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/techmech-keeb/orbit-remapper/releases/tag/v0.1.0

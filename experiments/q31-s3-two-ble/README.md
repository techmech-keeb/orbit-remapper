# Q31 試験プログラム：M5Dial で BLE 機器 2 台を 7.5 ms で受けられるか

初代 M5Dial（ESP32-S3）が、BLE のキーボードとマウスの 2 台を、どちらも接続間隔 7.5 ms で受けられるかを測るための試験プログラム。仕様は [q31-experiment-brief.md](../../docs/drafts/2026-09-26/q31-experiment-brief.md) による。

**実機ではまだ動かしていない。** 確かめたのはビルドが通ることだけ（ESP-IDF v5.5.5、警告なし。下記の 3 通りの設定すべて）。

やること：HID 機器（サービス 0x1812）を探して最大 2 台につなぎ、暗号化して、入力の報告（0x2A4D）の通知を受け、届いた回数と間隔を 1 秒ごとに出す。報告の中身は読まない。リマップや PC への USB 出力はしない。Wi-Fi は初期化しない。

## 必要なもの

- ESP-IDF v5.5.5（ビルドを確かめた版。v5.3 以降なら動く見込みだが未確認）
- 初代 M5Dial と USB-C ケーブル
- BLE キーボード 1 台、BLE マウス 1 台

## ビルドと書き込み

ESP-IDF の環境を読み込んでから（`. $IDF_PATH/export.sh`、Windows は「ESP-IDF PowerShell」など）、このディレクトリで実行する。

```sh
idf.py build
idf.py -p <ポート> flash monitor
```

- `<ポート>` は Linux なら `/dev/ttyACM0`、Windows なら `COM3` など。
- 画面の部品（`espressif/esp_lcd_gc9a01`）は、初回のビルドのときに自動で `managed_components/` へ取り込まれる。
- ログをファイルに残すには、`monitor` の画面で `Ctrl+T` → `Ctrl+L` を押す（もう一度押すと止まる）。`idf.py -p <ポート> monitor --timestamps` にすると、行ごとに PC の時刻が付く。

### T7 用（接続後に 7.5 ms を要求する方式）

設定を分けたいので、ビルド先のディレクトリを別にする。

```sh
idf.py -B build-after -DSDKCONFIG=build-after/sdkconfig \
  -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.after_connect" build
idf.py -B build-after -p <ポート> flash monitor
```

### そのほかの設定

`idf.py menuconfig` → 「Q31 experiment」で変えられる。

| 項目 | 既定値 | 内容 |
| --- | --- | --- |
| How to request the connection interval | In the connection request | 接続を始める時点で指定する（T1〜T6）か、接続後に要求する（T7）か |
| Requested connection interval | 6（7.5 ms） | 1.25 ms 単位 |
| Supervision timeout | 400（4 秒） | 10 ms 単位。この時間通信がなければ切断とみなす |
| Connection event length | 0（コントローラーの既定） | 1 回の通信に使う時間の長さ（0.625 ms 単位）。ESP32-S3 がこの値に従うかどうかも未確認 |
| Accept parameter updates requested by the device | 有効 | 機器から接続条件の変更を求められたときに受けるかどうか。無効にすると断る（機器が切断することがある）。どちらでも要求の内容はログに出す |

## 使い方

1. 書き込むと、すぐに探し始める（画面に `SCAN`）。
2. キーボードかマウスをペアリング待ちの状態にする。HID 機器を見つけると、自分から接続・ペアリングする。**近くにペアリング待ちの HID 機器がほかにあると、そちらにつないでしまう**ので、試す機器だけをペアリング待ちにする。
3. 2 台つながると探すのを止める（画面に `IDLE 2/2`）。
4. 切断されると探すのを再開し、保存したペアリング情報を使って自動でつなぎ直す。

**ペアリング情報を消す**：画面（ダイヤルの中央）を押し込んだまま、RST ボタンを押して起動する。ログに `bonds cleared` が出る。

### T1〜T7 との対応

| 番号 | このプログラムでの手順 |
| --- | --- |
| T1 / T2 | ペアリング情報を消してから、1 台だけをペアリング待ちにする |
| T3 / T4 | 1 台目がつながって `DEV` 行の `subs` が 1 以上になってから、2 台目をペアリング待ちにする。2 台目以降は、つないだ順に D0、D1 の番号が付く |
| T5 | T3 の状態のまま、10 分以上ログを残す。`total` と `disc` を見る |
| T6 | 機器の電源を入れ直す／スリープさせて戻す。`EVT` 行の `disconnected` と、つなぎ直した後の `DEV` 行の `itvl` を見る |
| T7 | 上の「T7 用」でビルドし直して T3 を行う |

## ログの読み方

ログはすべて `Q31` で始まる。`grep '^Q31'` で抜き出せる。時刻 `t` は起動からの秒数。

```text
Q31 START idf=v5.5.5 app=<コミット> mode=at-connect itvl_req=6(7.50ms) to=400 ce_len=0 accept_peer_update=1 clear_bonds=0
Q31 SUM t=12 conn=2/2 scan=0
Q31 DEV t=12 D0 addr=..:3a:5f h=1 itvl=6(7.50ms) lat=0 to=400 enc=1 subs=3 rpt=120 maxgap=9.1ms gaps<=8/16/32/>32=100/18/1/0 total=1440 disc=0
Q31 EVT t=3.512 D0 addr=..:3a:5f connected itvl=6(7.50ms) lat=0 to=400(4000ms)
```

（上の数値は書式の説明のための例で、測った値ではない。）

| 行 | 出るとき | 内容 |
| --- | --- | --- |
| `START` | 起動時に 1 回 | ESP-IDF の版、プログラムの版（`git describe` の結果。コミットしていない変更があると `-dirty` が付く）、設定 |
| `SUM` | 1 秒ごと | つながっている台数、探しているかどうか |
| `DEV` | 1 秒ごと、機器ごと | 下の表 |
| `EVT` | そのつど | 接続、切断、接続条件の変更、機器からの変更要求、暗号化、通知の登録、失敗 |

`DEV` 行の項目：

| 項目 | 内容 |
| --- | --- |
| `addr` | 相手のアドレスの下位 2 バイトだけ |
| `h` | 接続の番号 |
| `itvl` | **実際の接続間隔**（1.25 ms 単位。括弧内はミリ秒）。`ble_gap_conn_find()` で得た値 |
| `lat` / `to` | latency（間引いてよい回数）／切断とみなすまでの時間（10 ms 単位） |
| `enc` / `subs` | 暗号化しているか／通知を登録できた Report の数。`subs=0` なら報告は届かない |
| `rpt` | 直近 1 秒に届いた報告の数。7.5 ms なら最大で約 133 |
| `maxgap` | 直近 1 秒の、報告と報告の間隔の最大値 |
| `gaps<=8/16/32/>32` | 報告の間隔を 8 ms 以下／16 ms 以下／32 ms 以下／32 ms 超 に分けた回数。7.5 ms で毎回届いていれば、ほぼすべて「8 ms 以下」に入る。1 秒以上の間隔は、操作していなかっただけとみなして数えない |
| `total` / `disc` | 起動してからの報告の合計／切断の回数（D0・D1 の枠ごと） |

`EVT` 行の失敗の理由は、`status` や `reason` に NimBLE の値を、`hci=` に Bluetooth の規格で決まった番号を出す（`hci=-1` は Bluetooth の番号ではない失敗）。よく出そうなもの：`0x08` 通信が途絶えた、`0x13` 相手が切った、`0x16` こちらが切った、`0x3e` 接続を始められなかった、`0x12` 条件が不正（先例で 15 ms より短い要求を断られたときの値）。

画面には同じ要点を出す：探しているか、台数、機器ごとのアドレスの下位・実際の間隔（緑＝要求どおり、黄＝違う）・latency・timeout（ミリ秒）・直近 1 秒の報告数と最大間隔、最後のイベント。

## 作りについて

| ファイル | 内容 |
| --- | --- |
| `main/ble_central.c` | 探す・つなぐ・暗号化・GATT の探索・通知の登録・報告の計数。NimBLE の GAP と GATT を直接使い、`esp_hidh` は使わない（prior-art.md の不具合の表を避けるため） |
| `main/main.c` | 起動、1 秒ごとのログと画面。ログと画面は core 1 で動かし、BLE（core 0）の邪魔をしないようにしている |
| `main/display.c`, `main/font5x7.c` | 丸い画面（GC9A01）への文字表示。字の形はこのプロジェクトで作った |
| `main/Kconfig.projbuild` | 上の「そのほかの設定」 |

先例（prior-art.md）への対応：

- §4.33：接続を始める時点で 7.5 ms を指定する。比較用に、接続後の要求にも切り替えられる（T7）。
- §4.29：接続したら自分から暗号化を始める。
- §4.20：スリープから戻った機器が HID の識別子を出さずに接続を求めても、保存したアドレスと一致するか、こちら宛ての接続要求であればつなぐ。

## 未確認のこと・制約

- 実機で動かしていない。画面の赤と青が入れ替わって見えたら、`display.c` の `LCD_RGB_ELEMENT_ORDER_BGR` を `RGB` に変える（見やすさは問わないので、測定には影響しない）。
- 数字入力が必要なペアリング（キーボードで 6 桁を打つ方式）には対応していない。こちらは「入出力なし」として名乗るので、通常は確認なしのペアリングになる見込み。
- 機器がアドレスを定期的に変える方式（プライバシー機能）を使っている場合、HID の識別子を出さずに接続を求められると、見つけられないことがある。
- ペアリング情報は最大 3 台分保存する（ESP-IDF の既定）。

## ライセンスと出典

- このディレクトリのコードは MIT（リポジトリの [LICENSE](../../LICENSE)）。
- ビルドに使う物（リポジトリには入れない）：ESP-IDF と NimBLE（Apache-2.0）、ビルド時に取り込む `espressif/esp_lcd_gc9a01` と `espressif/cmake_utilities`（Apache-2.0。版と検査値は `dependencies.lock`）。
- 画面・ボタン・電源保持の端子番号は、M5Stack の公式ライブラリ（M5GFX の `src/M5GFX.cpp`、M5Unified の `src/M5Unified.inl`、どちらも MIT。2026-09-26 確認）の M5Dial の設定から読み取った。コードは写していない。
- ESP-IDF の見本（blecent など）のコードは写していない。
- ログに出すアドレスは下位 2 バイトだけ。ペアリングの鍵は M5Dial の内部（NVS）にだけ残る。`sdkconfig` とビルドの生成物は `.gitignore` で除いている。

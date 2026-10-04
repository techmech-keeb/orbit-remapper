# ほかの M5Stack の機器に広げるための整理

状態：検討のみ（未着手）。2026-10-02 作成、2026-10-04 更新（M5Dial v1.1 のボタンの端子、Waveshare の機器）。製品の情報は公式資料・回路図・メーカーの見本のコードで確認した。どの機器も実機では試していない。

## 1. コア機能と UI は分かれているか

### 分かれているところ

- Bluetooth（`ble.c`）、台帳（`ledger.c`）、設定の保存（`storage.cc`）、USB（`usb.cc`）、本家とのつなぎ（`platform.cc`）、シリアルと設定ツールの命令（`commands.cc`・`tool.cc`）は、画面・UI のファイル（`ui.h`、`display.h`）を読み込んでいない。
- 画面（`ui.c`）は、`ui.h` の状態（1 秒ごとの写し）を受け取り、`orbit_ble_*` を呼ぶだけ。コアの中身には触れない。
- 本家のコア（`firmware/hid-remapper/`）は無改造。

### 混ざっているところ

| 場所 | 混ざっているもの |
| --- | --- |
| `main.cc` | 主ループ（コア）と同じファイルに、電源を保つ端子（G46）とボタンの端子（G42）、ボタンの短押し・2 秒押し・起動時の押下（書き込みモード）の判定、M1 からの文字だけの画面（`set_line` など）、画面に渡す状態の組み立て |
| `ui.c` | ダイヤルの端子（G40/G41）と読み取り（PCNT）が画面の処理の中にある。240×240 の丸い画面の配置が決め打ち |
| `display.c` | 画面の部品（GC9A01）と端子（G4〜G9）が決め打ち |
| ビルド | 機器を選ぶ仕組みが無い。LVGL と GC9A01 のドライバーがいつも入る |
| `partitions.csv` | NVS が 0x400000 にあるので、フラッシュが 4 MB より大きい機器が要る |

### 直し方（案）

1. **ボードの層**：端子、電源を保つ処理、入力（短押し・長押し・起動時の押下・ダイヤルの回転）を `board/<機器>/` にまとめる。画面とコアは入力を出来事として受け取るだけにする。動きは変えない整理なので、ビルドと短い回帰の試験で確かめられる。2b（主ループに手が入る）の前にやるとぶつからない。
2. **画面の配置を画面の形ごとに分ける**：機能（カード、メニュー、承認、電池）は共通、配置（丸い 240×240、四角い 320×240、135×240、128×128）は機器ごと。
3. **画面なしの構成**：UI を外しても、シリアルの命令と設定のページで操作できるようにする。
4. **機器ごとのビルド**：ビルドの設定で機器を選び、`sdkconfig.defaults` を機器ごとに分ける。リリースは機器ごとのファイル（`Orbit_Remapper_firmware_v<版>_<機器>.bin`）。
5. M1 の文字だけの画面（書き込みモードの表示などの予備）を LVGL にまとめるかを決める。

2〜4 は、2 台目の機器を手に入れて試せるときにやる。

## 2. 候補の機器

Orbit には、USB の機器として PC につながる機能（USB OTG）と Bluetooth LE の両方が要る。両方を持つのは ESP32-S3。下は仕様の上での候補。購入前に、国内の認証（技適）の表示と、今も買えるかを確かめる。

### A. ほぼそのまま動く見込み

| 製品 | 公式資料で確かめたこと | 残っていること |
| --- | --- | --- |
| M5Dial v1.1 | ESP32-S3FN8（Stamp-S3A）、8 MB、USB OTG。画面 GC9A01 240×240、画面の端子 G4〜G9、ダイヤル G40/G41、電源を保つ端子 G46 は初代と同じ。タッチ FT3267。v1.1 のページは初代と同じ回路図（`Sch_M5Dial.pdf`）を指していて、回路図では画面のボタン（WAKE）が G42 につながっている（2026-10-04 に読み取り） | 実機での動作（ボタン、ダイヤルの向き、Bluetooth）と技適（Q24） |

v1.1 で初代と違うのは、中のモジュール（Stamp-S3A）のアンテナと、モジュールの RGB LED の電源を G38 で入れるようになったことだけ。どちらも Orbit は使っていないので、今の .bin のまま試せる見込み（推測。実機では未確認）。

### B. 画面付き（画面のドライバーと配置の差し替えが要る）

| 製品 | 画面 | 入力 | 補足 |
| --- | --- | --- | --- |
| DinMeter / v1.1 | 1.14 型 ST7789V2 | ダイヤルあり | 操作が M5Dial に一番近い（製品一覧の記載のみ。個別の資料は未確認） |
| CoreS3 / CoreS3-SE / CoreS3-Lite | 2.0 型 320×240（ILI9342C）、タッチ FT6336U | 電源・リセットのボタンのみ。操作はタッチ | 16 MB。USB-C は「OTG と CDC」とあるが、ESP32-S3 自身の USB かは資料で確かめきれていない |
| StickS3 | 1.14 型 135×240（ST7789P3） | ボタン 2 つ（G11、G12） | ESP32-S3-PICO-1-N8R8。USB は ESP32-S3 自身 |
| AtomS3 / AtomS3R | 0.85 型 128×128（GC9107 / ST7735） | ボタン 1 つ | AtomS3R は ESP32-S3-PICO-1-N8R8、USB OTG |
| Cardputer / v1.1 / Adv | 1.14 型 | キーボード | 卓上のリマッパーとしては大きい |

### B'. M5Stack 以外：Waveshare ESP32-S3-Knob-Touch-LCD-1.8（2026-10-04 調査）

M5Dial と同じ「ダイヤル付きの丸い画面」の形。Orbit に要る機能（ESP32-S3 の USB と Bluetooth LE）はある。ただし端子・画面・ボタンが M5Dial と違うので、**今の .bin は書き込まない**（下の「今の .bin を書き込むと」）。

| 項目 | 公式資料で確かめたこと | Orbit で要る直し |
| --- | --- | --- |
| チップ | ESP32-S3R8（PSRAM 8 MB）と、別の ESP32（ESP32-U4WDH）の 2 つ。フラッシュ 16 MB | なし。ESP32 のほうは使わない |
| USB | USB-C の線は ESP32-S3 の GPIO19/20（自身の USB）に来る。ただし**差す向きで、ESP32-S3 につながるか、ESP32 の USB シリアルにつながるかが変わる**（Wiki の FAQ） | いつも ESP32-S3 側の向きで差す。逆向きでは PC からキーボード・マウスに見えない。使う人向けの文書に書く |
| 画面 | 1.8 型 360×360、QSPI。見本は SH8601 のドライバーに独自の初期化の命令を渡して使う。SCL 13、CS 14、D0〜D3 15〜18、RST 21、バックライト 47 | 画面のドライバーと 360×360 の配置 |
| タッチ | CST816。SDA 11、SCL 12、INT 9、RST 10 | 押すボタンの代わりに使う |
| ダイヤル | A＝GPIO8、B＝GPIO7（ESP32-S3 用。もう 1 つのダイヤルは ESP32 用） | 端子を変える |
| ボタン | **ダイヤルは押せない**（回路図の部品は押しスイッチの無いエンコーダー）。BOOT（GPIO0）と電源のボタンだけ | 短押し（メニュー・許可）と 2 秒押しを、タッチの操作に置き換える。起動時に押して書き込みモードに入るのは BOOT で足りる |
| 電源を保つ端子 | なし | 外す |
| そのほか | DAC（PCM5100A）、マイク、振動モーター（DRV2605）、TF カード、電池 | Orbit では使わない |
| 技適 | Wiki に記載なし。未確認 | 買う前に確かめる |

**今の .bin を書き込むと**：この機器では G46 がマイクのデータ線、G4〜G9 が TF カード・ダイヤル・タッチの線、G40/G41 が DAC の線になっている。Orbit は G46 を出力にして High にし、G4〜G9 を画面の線として動かすので、マイクの出力とぶつかるなど、電気的に良くない状態になる。

**移植の形**：§1 の 1（ボードの層）と 2（画面の形ごとの配置）のあと、この機器用の画面（QSPI）、タッチ、ダイヤルの端子を足す。USB-C の向きの注意は、ほかの機器には無い。

### C. 画面なし（シリアルの命令と設定のページで操作）

| 製品 | 補足 |
| --- | --- |
| AtomS3U | USB-A の差し込み口（OTG）。PC に直接挿すドングルの形。ボタン 1 つ、RGB LED |
| AtomS3-Lite | ボタンと RGB LED |
| Stamp-S3 / Stamp-S3A / Stamp-S3Bat | 部品として載せる基板。自作の筐体向け |

### D. 向かない

| 製品 | 理由 |
| --- | --- |
| Tab5 | ESP32-P4。USB OTG はあるが Bluetooth が無い（無線は別の ESP32-C6） |
| NanoC6、Stamp-C5、Stamp-C6LoRa など | ESP32-C シリーズは USB OTG が無い |
| Core2・Basic・Fire など | 初代 ESP32 は USB OTG が無い |
| PaperS3・PaperMono、Air Quality、VAMeter、StamPLC、Capsule、Unit CamS3 | ESP32-S3 だが用途が合わない |

### 試す順番（案）

1. M5Dial v1.1：差し替えがほぼ要らず、今の .bin のまま今までの試験の手順で試せる。届いたら技適のラベルも確かめる（Q24）。
2. DinMeter：ダイヤルがあり、四角い画面の配置の例になる。
3. AtomS3U：画面なしの形の例になる。

## 出典（2026-10-02 確認）

- M5Stack 製品一覧：https://docs.m5stack.com/en/products
- Dial v1.1：https://docs.m5stack.com/en/core/M5Dial%20V1.1
- StickS3：https://docs.m5stack.com/en/core/StickS3
- CoreS3：https://docs.m5stack.com/en/core/CoreS3
- AtomS3R：https://docs.m5stack.com/en/core/AtomS3R

2026-10-04 確認：

- Dial v1.1（端子の表、RGB LED の電源の説明）：https://docs.m5stack.com/en/core/M5Dial%20V1.1
- Dial（初代）：https://docs.m5stack.com/en/core/M5Dial
- Dial の回路図（初代・v1.1 のページの両方から）：https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/499/Sch_M5Dial.pdf
- Stamp-S3A（Stamp-S3 との比較の表）：https://docs.m5stack.com/en/core/Stamp-S3A
- Waveshare ESP32-S3-Knob-Touch-LCD-1.8（Wiki、FAQ）：https://www.waveshare.com/wiki/ESP32-S3-Knob-Touch-LCD-1.8
- 同 回路図：https://files.waveshare.com/wiki/ESP32-S3-Knob-Touch-LCD-1.8/ESP32-S3-Knob-Touch-LCD-1.8-schematic.zip
- 同 見本のコード（`ESP-IDF/08_LVGL_Test`、`04_Encoder_Test`）：https://files.waveshare.com/wiki/ESP32-S3-Knob-Touch-LCD-1.8/ESP32-S3-Knob-Touch-LCD-1.8-Demo.zip

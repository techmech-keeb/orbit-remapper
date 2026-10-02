# M1 の報告（途中）：版 `86a2d16`（`orbit-m1-86a2d16-ble.bin`、2026-09-27）

利用者の PC（Windows 11、PC 本体のポート）と初代 M5Dial で、m1-handoff.md §6.1（`ad73654` 版）を進めた。`.bin` の SHA-256 は一致。書き込みモードへは PC 側から 1200 bps で入れた（`1e37eea` からの切り替え）。ログは COM11、115200 bps、DTR あり。

## 結論（ここまで）

- **起動とログ：合格。** `M1 START idf=v5.5.5 app=86a2d16 upstream=51ab8b3 config_size=2048 descriptor=0 vid=cafe pid=baf2 max_devs=2 conn_itvl=6(7.50ms) lvgl_reserve=ok`、`ble ready bonds=0`、`scan start (pairing)`。
- **A2 前半（MD600 1 台）：合格。** 7.50 ms・latency 0、暗号化、Report Map 192 バイト、記述子の解析、入力 Report 4 つを登録。キー入力が PC に届いた（利用者が MD600 でメッセージを打った）。
- **A8（1 台・キー入力のみ）：今のところ合格。** `LAT` 35 秒分で最大 1.86 ms、通常は平均 0.7〜0.9 ms・最大 1.0 ms 前後、すべて 8 ms 以下。キューあふれ（`lost=`）なし。
- **A2 後半（2 台目）と A4 の Pair new device / Forget all devices：止まっている。** 本家の設定ツールは、USB の製品名に `Bluetooth` が入っている機器でしかこの 2 つのボタンを表示しない（`config-tool-web/code.js` 261 行目、`device.productName.includes("Bluetooth")`）。M1 の製品名は本家 USB 版を写した `HID Remapper XXXX`（WebHID の一覧では `HID Remapper GUI1`）なので、ボタンが出ない。本家 BLE 版は `HID Remapper Bluetooth`（`firmware-bluetooth/prj.conf` の `CONFIG_USB_DEVICE_PRODUCT`）。
- **A3：読み出しは合格。** 設定ツールで本体が見え（`HID Remapper GUI1`）、Load from device で既定の設定が表示された。保存と再起動後の確認はまだ。
- **A9：判定できない。** `SUM` の `heap_min` が `heap_free` より大きい（例：`heap_free=102036 heap_min=970560`）。最小値の取り方がおかしい。`heap_free` は BLE 入りで約 102〜108 KB（`1e37eea` では 218 KB）。

## 直してほしいこと

1. **USB の製品名を `HID Remapper Bluetooth` にする**（本家 BLE 版と同じ）。設定ツールの Pair new device / Forget all devices を出すため。後ろに識別の文字を付ける場合も `Bluetooth` を含める。
2. **`heap_min` を直す。** 今の値（97 万〜104 万）は、内部 RAM の空きより大きい。`esp_get_minimum_free_heap_size()` を使っているか、別の caps（PSRAM や全領域の合計など）の値を出していないかを確かめる。A9 は内部 RAM（`MALLOC_CAP_INTERNAL`）の最小値で判定したい。

## 時系列（PC の時刻、JST）

| 時刻 | 操作 | 結果 |
| --- | --- | --- |
| 22:59 | 書き込み → RST | `START`、`ble ready bonds=0`、`scan start (pairing)`、`usb mounted` |
| 23:00:03 | （機器側は Q31 の古いペアリングのまま） | MD600（`1f:64`）が接続 → `encryption failed status=0x505` → `report map read failed status=0x105` → `subscribed 0` → 相手が切断（`hci=0x13`） |
| 23:00:05 | 利用者が MD600 のペアリングをやり直す | MD600 が新しいアドレス `26:27` で接続 → `encryption on` → `report map 192 byte(s), hub_port=1` → `descriptor parsed` → `subscribed 4 input report(s)` → `scan start (bonded devices only)` |
| 23:01 | MD600 でキー入力 | PC に届いた。報告 194 件、`LAT` 最大 1.86 ms |
| 23:0x | 設定ツールで Open device → Load from device | `HID Remapper GUI1` が見え、既定の設定が表示された。Actions タブに Pair new device が出ない |

## 次に確かめること（製品名を直した版で）

A2 後半（Pair new device → meteorite40）、A3 保存、A4（Pair new device、Forget all devices、Flash firmware＝Reset to bootloader）、A7、A8（2 台）、A9、A10。

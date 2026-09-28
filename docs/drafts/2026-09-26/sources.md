---
status: draft
snapshot_date: 2026-09-26
finalized: false
---

# 出典と保存範囲

## この資料の根拠

1. 本セッションでのユーザーの指示と、設計・S3・GitHub・命名の検討。
2. `remapper_claude_handoff.md` / `.docx`（2026-09-25）。初期〜E1の詳細記録。
3. `remapper_design.zip`、単独PDF/HTML/プレビュー（取得時点の最新版）。
4. `remapper_bom_v0.3.xlsx`（元BOM）。
5. 2026-09-26に確認したGitHubリポジトリの状態。

原本は[成果物一覧](artifacts.md)を参照。SHA-256と保存先を`artifact-manifest.json`に記録しています。原本を新しい結論で上書きしません。

この整理は既存調査の記録です。外部仕様を今回すべて再調査したものではありません。動作・調達・ライセンス等を確定する際は対象版の資料を確認してください。

## 技術資料

- 本家HID Remapper: https://github.com/jfedor2/hid-remapper
- 本家ハードウェア: https://github.com/jfedor2/hid-remapper/blob/master/HARDWARE.md
- 本家Bluetooth版: https://github.com/jfedor2/hid-remapper/blob/master/BLUETOOTH.md
- M5Stack Glass2: https://docs.m5stack.com/en/unit/Glass2%20Unit
- Glass2機械図: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/757/U158BUnitGlass2-model-size.pdf
- M5Dial V1.1: https://docs.m5stack.com/en/core/M5Dial%20V1.1
- M5Dialライブラリ: https://github.com/m5stack/M5Dial
- ESP32-S3 USB Host: https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_host.html
- ESP32-S3 USB Device: https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_device.html

## GitHub管理の参照

- Forkと公開範囲: https://docs.github.com/en/pull-requests/reference/forks
- Projects: https://docs.github.com/en/issues/planning-and-tracking-with-projects/learning-about-projects/about-projects
- Releases: https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases
- Git LFS: https://docs.github.com/en/repositories/working-with-files/managing-large-files/about-git-large-file-storage

## 名称調査

2026-09-26の公開Web検索。確認対象と一次資料リンクは[naming.md](naming.md)に記載。完全一致が見つからなかったことを、名称の独占・商標使用可否の根拠にはしません。

## 未収録の過去資料

CLEAR/REFLEXの旧CADやPDF、A〜FおよびE2の比較画像は、今回取得した成果物一式には収録されていません。初期〜E1の経緯は当時の引き継ぎ文書から把握できますが、元画像の再構成や寸法の推定で穴埋めしていません。E1採用画像はZIP内の原本をこのリポジトリに保存しています。

## ESP-IDF のハブ対応（2026-09-26 確認）

いずれも最終確認日 2026-09-26。公式資料の記載の確認で、実機では試していない。

- esp-usb USB Host（ESP32-S3、latest）: https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_host.html
- ESP-IDF v5.5 USB Host（ESP32-S3）: https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/peripherals/usb_host.html
- ESP-IDF v5.4 USB Host（ESP32-S3）: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/api-reference/peripherals/usb_host.html
- esp-usb USB Host の変更履歴: https://github.com/espressif/esp-usb/blob/master/host/usb/CHANGELOG.md
- esp-usb HID Host ドライバー README: https://github.com/espressif/esp-usb/blob/master/host/class/hid/usb_host_hid/README.md

## M5Dial の回路図と技適（2026-09-26 確認）

いずれも最終確認日 2026-09-26。

- M5Dial 回路図: https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/499/Sch_M5Dial.pdf
- M5Stack Dial V1.1 公式資料: https://docs.m5stack.com/en/core/M5Dial%20V1.1
- beekeeb XIAO nRF52840 Plus（技適 222-257139）: https://shop.beekeeb.jp/products/seeed-studio-xiao-nrf52840-plus
- 秋月電子 XIAO BLE nRF52840（技適 211-220207）: https://akizukidenshi.com/catalog/g/g117341/
- スイッチサイエンス M5Stack Dial v1.1: https://www.switch-science.com/products/10302
- スイッチサイエンス M5StampS3A: https://www.switch-science.com/products/10377
- 秋月電子 M5Stamp S3A: https://akizukidenshi.com/catalog/g/g131758/
- マルツ M5Stack Dial v1.1: https://eleshop.jp/shop/g/gP4I31A/
- Elecrow CrowPanel 1.28": https://www.elecrow.com/wiki/CrowPanel_1.28inch-HMI_ESP32_Rotary_Display.html
- LilyGO T-Encoder Pro: https://github.com/Xinyuan-LilyGO/T-Encoder-Pro
- Waveshare ESP32-S3-Knob-Touch-LCD-1.8: https://www.waveshare.com/esp32-s3-knob-touch-lcd-1.8.htm
- VIEWE 2.1" Knob Display: https://viewedisplay.com/product/esp32-2-1-inch-480x480-round-tft-knob-display-rotary-encoder-arduino-lvgl/
- 秋月電子 M5Stamp S3（技適 219-229318。初代 M5Dial に搭載）: https://akizukidenshi.com/catalog/g/g118194/

## 類似プロジェクト（2026-09-26 確認）

いずれも最終確認日 2026-09-26。ソースコードを取得して読んだ時点のコミットを記す。

- ifritJP/bt-keyboard-remapper（コミット 19fb7c1、2022-02-24）: https://github.com/ifritJP/bt-keyboard-remapper
- 同 解説記事: https://ifritjp.github.io/blog2/public/posts/2022/2022-02-22-bt-keyboard-remapper/
- ZUENS2020/cardputer-kb（2026-08-08 更新）: https://github.com/ZUENS2020/cardputer-kb
- 同 M5Stack Community の投稿: https://community.m5stack.com/topic/8307/cardputer-kb-ble-hid-remapper-for-cardputer-adv-web-ui-on-device-wifi-ota
- WikDra/esp32-hid-gamepad-bridge（コミット 9053d9d、2026-09-13）: https://github.com/WikDra/esp32-hid-gamepad-bridge
- M5Stack Community（ロゴ使用の質問）: https://community.m5stack.com/topic/5236/use-of-the-m5-stack-logo-in-projects
- finger563/esp-usb-ble-hid（コミット e0a18ae、2026-09-18）: https://github.com/finger563/esp-usb-ble-hid
- esp32beans/BLE_HID_Client（コミット aab46c8、2022-09-19）: https://github.com/esp32beans/BLE_HID_Client
- ESP-IDF Multi-Connection Guide（ESP32-S3）: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/ble/ble-multiconnection-guide.html
- ESP32-S3 Series Datasheet v2.2: https://documentation.espressif.com/esp32-s3_datasheet_en.pdf
- nRF52840 製品ページ: https://www.nordicsemi.com/Products/nRF52840
- nRF52840 Product Specification（Key features）: https://docs.nordicsemi.com/bundle/ps_nrf52840/page/keyfeatures_html5.html
- ZMK Split Keyboards: https://zmk.dev/docs/features/split-keyboards
- HID Remapper Forum（Bluetooth Classic version testing, ESP32）: https://forum.remapper.org/t/bluetooth-classic-version-testing-esp32/18
- nRF Desktop の Bluetooth の説明（sdk-nrf、bluetooth.rst）: https://github.com/nrfconnect/sdk-nrf/blob/main/applications/nrf_desktop/bluetooth.rst
- SoftDevice Controller：Scheduling: https://nrfconnectdocs.nordicsemi.com/ncs/latest/nrfxlib/softdevice_controller/doc/scheduling.html
- Nordic DevZone（How to schedule multilink connect event?）: https://devzone.nordicsemi.com/f/nordic-q-a/88980/how-to-schedule-multilink-conect-event
- sdk-nrf のコントローラ Kconfig（イベント長の既定値）: https://github.com/nrfconnect/sdk-nrf/blob/main/subsys/bluetooth/controller/Kconfig
- Espressif FAQ（BLE / Bluetooth）: https://github.com/espressif/esp-faq/blob/a71fa4f/docs/en/software-framework/ble-bt.rst
- ESP-IDF ble_multi_conn_cent の README: https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/nimble/ble_multi_conn/ble_multi_conn_cent/README.md

## BLE の検出・接続の先行事例（2026-09-28 確認）

[ble-connect-prior-art.md](../2026-09-28/ble-connect-prior-art.md) の出典。いずれも最終確認日 2026-09-28（取得は 2026-09-27〜28）。ブランチ名だけのものは、その日の先端を読んだ。

仕様：

- Bluetooth SIG, HID over GATT Profile 1.0（2011-12-27 採択）: https://www.bluetooth.org/docman/handlers/downloaddoc.ashx?doc_id=245141
- Bluetooth SIG, HID over GATT Profile の仕様ページ: https://www.bluetooth.com/specifications/specs/hid-over-gatt-profile/
- Bluetooth SIG, Reconnection Configuration Service 1.0（Withdrawn）: https://www.bluetooth.com/specifications/specs/reconnection-configuration-service-1-0/
- Bluetooth Core 5.4 Vol 3 Part G（GATT）: https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/host/generic-attribute-profile--gatt-.html
- Apple Accessory Design Guidelines: https://developer.apple.com/accessories/Accessory-Design-Guidelines.pdf

OS とスタック：

- Linux `net/bluetooth/hci_sync.c`・`hci_event.c`（master）: https://github.com/torvalds/linux/blob/master/net/bluetooth/hci_sync.c
- BlueZ `src/shared/gatt-client.c`・`profiles/input/hog-lib.c`・`src/main.conf`（master）: https://github.com/bluez/bluez
- Android `BluetoothDevice`: https://developer.android.com/reference/android/bluetooth/BluetoothDevice
- Zephyr `include/zephyr/bluetooth/conn.h`・`subsys/bluetooth/host/scan.c`・`Kconfig.gatt`（main）: https://github.com/zephyrproject-rtos/zephyr
- apache/mynewt-nimble `nimble/host/include/host/ble_gap.h`（master）: https://github.com/apache/mynewt-nimble
- ESP-IDF v5.5.5 の `components/bt/host/nimble/`（手元のソースで確認）: https://github.com/espressif/esp-idf/tree/v5.5.5/components/bt/host/nimble

ファームウェア：

- jfedor2/hid-remapper `firmware-bluetooth/src/main.cc`（最終変更 61b3f74、2025-01-17）: https://github.com/jfedor2/hid-remapper/blob/master/firmware-bluetooth/src/main.cc
- 同 BLUETOOTH.md: https://github.com/jfedor2/hid-remapper/blob/master/BLUETOOTH.md
- 同 ブランチ `bluetooth-classic`（d4042f5、7df72c1、2026-08-29）: https://github.com/jfedor2/hid-remapper/tree/bluetooth-classic
- nrfconnect/sdk-nrf（main 5d1f559）の `applications/nrf_desktop/src/modules/ble_scan.c`・`ble_discovery.c`・`Kconfig.ble_scan`、`samples/bluetooth/central_hids/src/main.c`、`subsys/bluetooth/scan.c`・`Kconfig.scan`: https://github.com/nrfconnect/sdk-nrf
- zmkfirmware/zmk（main 5b51501）の `app/src/split/bluetooth/central.c`・`app/src/ble.c`: https://github.com/zmkfirmware/zmk
- RMK（手元のクローン、568770f、2026-09-04）の `rmk/src/split/ble/central.rs`・`rmk/src/dongle/mod.rs`: https://github.com/techmech-keeb/rmk
- embassy-rs/trouble（main b6043f5）の `host/src/central.rs`: https://github.com/embassy-rs/trouble
- darthcloud/BlueRetro（e1a9831、2025-12-14 アーカイブ）: https://github.com/darthcloud/BlueRetro
- espressif/esp-iot-solution `components/bluetooth/ble_conn_mgr`（v1.3.1、2026-09-09）: https://github.com/espressif/esp-iot-solution/tree/master/components/bluetooth/ble_conn_mgr
- h2zero/esp-nimble-cpp（master、2026-09-18）: https://github.com/h2zero/esp-nimble-cpp
- finger563/esp-usb-ble-hid: https://github.com/finger563/esp-usb-ble-hid
- esp32beans/BLE_HID_Client: https://github.com/esp32beans/BLE_HID_Client
- ncmro7/MI-RC003-ESP32-Bridge: https://github.com/ncmro7/MI-RC003-ESP32-Bridge
- ESPHome ESP32 BLE: https://esphome.io/components/esp32_ble/

ESP-IDF の issue（2026-09-28 時点の状態）：

- #19011（esp_hidh が Report Map を読まない）: https://github.com/espressif/esp-idf/issues/19011
- #11312（esp_hidh で 2 台目のデータが読めない、Open）: https://github.com/espressif/esp-idf/issues/11312
- #18893（S3 central で暗号化後に GATT が流れない、Open）: https://github.com/espressif/esp-idf/issues/18893
- #19057（S3 で暗号化前のデータ長変更で暗号化が時間切れ、Open）: https://github.com/espressif/esp-idf/issues/19057
- #19030（NimBLE の NVS 保存が上書きを保存しない、Open）: https://github.com/espressif/esp-idf/issues/19030
- #18635（S3 central の RPA、Open）: https://github.com/espressif/esp-idf/issues/18635
- #18063（C3 でスキャン中の接続で落ちる、Open）: https://github.com/espressif/esp-idf/issues/18063
- #14053（S3 NimBLE の 0x28 切断、Open）: https://github.com/espressif/esp-idf/issues/14053

市販品：

- Logitech, Logi Bolt white paper: https://www.logitech.com/content/dam/logitech/en/business/pdf/logi-bolt-white-paper.pdf
- Keychron, Wireless mouse and keyboard not working: https://www.keychron.com/blogs/news/keychron-wireless-mouse-and-keyboard-not-working

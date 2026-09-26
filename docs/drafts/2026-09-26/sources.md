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

# M1 の報告：版 `1e37eea`（`orbit-m1-1e37eea-A1-usb-only.bin`、2026-09-27）

利用者の PC（Windows 11）と初代 M5Dial で、m1-handoff.md §6.1（`f76ff3a` 版）を確かめた。PC 側の USB の見え方は PowerShell（`Win32_PnPEntity`）、ログは `System.IO.Ports.SerialPort`（115200 bps、DTR/RTS あり。ポートが消えたら開き直す）。`.bin` の SHA-256 は引き継ぎ文書の値と一致。**M5Dial は PC 本体のポートに直接つないだ。**

## 結論

- **§6.1 の 1（ログが届く）：合格。** `M1 START`（`app=1e37eea upstream=51ab8b3 config_size=2048 descriptor=0 vid=cafe pid=baf2 lvgl_reserve=ok`）と、1 秒ごとの `M1 SUM` が COM11 に届く。DTR を立てると `START` が出直す。ログの取りこぼしなし。**CDC の送信口を 0x84 以下に収めた直しで解決した。**
- **§6.1 の 2（ボタン＋RST で書き込みモード）：合格。** VID 303A / PID 1001（COM9、JTAG）として見えた。`3662016` と同じ動きに戻った。
- **§6.1 の 3（1200 bps で書き込みモード）：合格。** PC 側から COM11 を 1200 bps・DTR ありで開いて閉じるだけで、VID 303A に切り替わった。
- どの場合も RST でアプリに戻り、約 1 秒で VID CAFE の複合機器と COM11 が復帰し、ログが再開した（3 回）。
- §6.1 の 4（ハブ経由）は未実施。

## 時系列（PC の時刻、JST）

| 時刻 | 操作 | 結果 |
| --- | --- | --- |
| 22:28 | G0 を押しながら電源投入（`728105c` からの脱出。下記） | VID 303A（COM9）。ブラウザから `1e37eea` を `0x0` に書き込み |
| 22:32:25 | RST | VID CAFE、COM11。`START` → `config loaded from NVS err=0x1102 (non-zero: defaults)` → `usb mounted` → `START`（DTR）→ `SUM t=1…` |
| 22:34:52 | **ボタン＋RST** | COM11 が消え、VID 303A（COM9、JTAG）が出た |
| 22:35:28 | RST | アプリに復帰。ログ再開 |
| 22:35:50 | **COM11 を 1200 bps で開いて閉じる**（PC 側の操作） | 約 8 秒後に VID 303A（COM9、JTAG） |
| 22:39:25 | RST | アプリに復帰。ログ再開 |

`SUM` の例：`M1 SUM t=18 usb=mounted boot_protocol=0 heap_free=218576 heap_min=213788`。空きメモリは 218 KB、最小 213 KB（Bluetooth も本家コアの処理もまだ動いていない版。A9 の判定には使えない）。

## `728105c` からの脱出で分かったこと（引き継ぎ文書 §4.2 への追記）

`728105c`（`db94be3` 入り）では、**ボタン＋RST、1200 bps のどちらでも**書き込みモードが PC から見えなくなった。さらに：

- その状態で **RST だけ**を押しても見えないまま（画面の `DOWNLOAD MODE` は前の描画が残っているだけで、判定に使えない）。
- **電源を切り、画面のボタンを押しながら電源を入れる**（アプリの早期判定 → 書き込みモード）でも見えないまま。同じ処理を通るため。
- **M5StampS3 の G0 ボタンを押しながら電源を入れる**（M5Stack の公式手順。ROM が直接見るので、アプリの処理を通らない）で、VID 303A が見えて書き込めた。背面を開ける必要がある。
- 推測：`db94be3` の pull override（`usb_serial_jtag_ll_phy_enable_pull_override` で D+/D- をプルダウン）が、`disable_pull_override` の後も効いたまま、または RST で消えない領域に残る。電源を切ると消える。ソースでの裏付けは取っていない。

**今後、USB シリアルの状態をいじる直しを入れるときは、G0 に手が届く状態で試すこと。**

## 次

- 依頼書 4.2（Bluetooth の受信）と 4.6（`DEV`・`LAT`）へ進んでよい。ログの経路と書き込みモードへの戻り方は、この版で安定している。
- ハブ経由の問題（`3662016` で見つかったもの）は未解決のまま後回し。
- 設定ツールの「Reset to bootloader」（戻り方 1）は未確認。A3・A4 と一緒に確かめる。

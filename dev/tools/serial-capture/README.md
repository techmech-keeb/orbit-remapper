# シリアルのログ取り（Windows、PowerShell）

M5Dial のログを COM ポートから読んで、PC の時刻を付けてファイルに残す。Q31 の試験と M1 の試験で使った。Python や ESP-IDF は要らない（Windows 標準の PowerShell だけ）。

| スクリプト | 使うとき | 動き |
| --- | --- | --- |
| `capture.ps1` | USB Serial/JTAG のコンソール（Q31 の試験プログラムなど、TinyUSB を使わないファーム） | DTR を立てずに開く（開いただけで本体が再起動しないように）。`-Dtr` を付けると DTR/RTS を立てる |
| `capture-usb.ps1` | TinyUSB の CDC（M1 のファーム。Windows では `USB シリアル デバイス (COMxx)`） | DTR/RTS を立てて開く（本体は DTR を見てから送る）。**再起動や書き込みモードでポートが消えても、戻るまで開き直しをくり返す** |

```powershell
# M1 のファーム。COM 番号はデバイス マネージャーで確かめる
powershell -NoProfile -ExecutionPolicy Bypass -File .\capture-usb.ps1 -Port COM11 -Out C:\path\to\m1.log

# Q31 の試験プログラム
powershell -NoProfile -ExecutionPolicy Bypass -File .\capture.ps1 -Port COM9 -Out C:\path\to\q31.log
```

- 止めるときは Ctrl+C（別のウィンドウで起動したなら、そのプロセスを終了する）。
- 行の先頭に PC の時刻 `[HH:mm:ss.fff]` を付ける。`# port opened` / `# port lost` の行は、ポートが現れた・消えた時刻。
- **M1 のファームは、ログのポートを 1200 bps で開くと書き込みモードに入る**（`firmware/orbit/README.md` §2）。ログ取りは 115200 bps で開くので、そのままでは入らない。
- ログには本体の完全な MAC アドレス（起動時の `BLE_INIT` の行など）が入ることがある。人に渡す・リポジトリに入れる前に伏せる。`M1` で始まる行だけを抜き出せば、機器のアドレスは下位 2 バイトしか含まない。

PC 側の USB の見え方（書き込みモードに入ったか、複合機器として認識されたか）は、次で確かめられる。

```powershell
Get-CimInstance Win32_PnPEntity | Where-Object { $_.DeviceID -match 'VID_303A|VID_CAFE' -or $_.Name -match 'COM[0-9]+' } | Select-Object Name, DeviceID
```

`VID_303A&PID_1001` が書き込みモード（ESP32-S3 の ROM）、`VID_CAFE&PID_BAF2` が M1 のファーム（本家 HID Remapper と同じ ID）。

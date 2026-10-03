# はじめに

## 1. 用意するもの

- **M5Dial**（ESP32-S3）。動作を確かめたのは初代 M5Dial だけです。新しい版の M5Dial や、ほかの ESP32-S3 の機器で動くかは確かめていません（画面・ダイヤル・ボタンの端子の番号は、初代 M5Dial に合わせてあります）。いま売られているのは後継の M5Stack Dial v1.1（StampS3A 搭載。購入先の例：[スイッチサイエンス](https://www.switch-science.com/products/10302)）で、これも動作と技適（日本の電波法の認証）はまだ確かめていません。
- USB-C のケーブル（データ通信ができるもの）。
- Bluetooth LE（BLE）のキーボードやマウス。同時につなげるのは 2 台です。
- Chrome か、Chrome 系のブラウザ（書き込みと設定に使います）。

## 2. ファームウェアを用意する

[Releases](https://github.com/techmech-keeb/orbit-remapper/releases) から、使いたい版の `Orbit_Remapper_firmware_v<版>_M5Dial.bin` を取ります。ふつうは「Latest」と付いた最新の正式版を使います。「Pre-release」と付いたものはテスト版です。

ファイルが壊れていないかは、同じリリースの `SHA256SUMS.txt` と見比べて確かめられます。Windows なら PowerShell で `Get-FileHash <ファイル名>` を実行すると、SHA-256 が出ます。

自分でビルドすることもできます（ESP-IDF v5.5.5。手順は [firmware/orbit/README.md](../firmware/orbit/README.md#2-ビルドと書き込み)）。その場合は `build/merged-binary.bin` を使います。

## 3. 書き込む

1. M5Dial を PC につなぎます。Orbit がすでに動いているときは、先に下の表のどれかで **書き込みモード** にします。初めて書き込むときは、そのまま次へ進み、つながらなければ下の方法 C を使います。
2. Chrome で https://espressif.github.io/esptool-js/ を開き、「Connect」で ESP32-S3 のポートを選びます。
3. Flash Address を `0x0` にして、取ったファイル（`.bin`）を選び、「Program」を押します。
4. 終わったら、M5Dial の RST ボタンを押します。Orbit が起動します。

Orbit が動いている間は、書き込みツールから自動では書き込みモードに入れません。次のどれかで入ります。

| 方法 | やり方 |
| --- | --- |
| A | ダイヤルの中央を押し込んだまま、RST を押す |
| B | ログの COM ポートを、1200 bps で開いて閉じる |
| C | 背面を開け、M5StampS3 の G0 ボタンを押しながら電源を入れる（どんな状態からでも入れる） |

書き込みモードに入ると、画面に `DOWNLOAD MODE` と出ます。

**PC 本体の USB ポートにつないでください。** USB ハブ経由では、方法 A で書き込みモードに入れなかった例があります。

`0x0` への書き込みでは、ペアリングの情報と設定は消えません。書き込みツールの「Erase Flash」を押すと全部消えます。

## 4. 最初の機器をペアリングする

1. 初めて起動すると、Orbit は **ペアリング待ち** になります（リングが黄色、「Pairing: turn on a device」）。
2. キーボードやマウスを、ペアリング待ちにします（やり方は各機器の説明書を見てください）。
3. Orbit が見つけて、自分からつなぎます。つながると、機器のカードが出て、リングが緑になります。

**近くにほかの BLE 機器がペアリング待ちでいると、そちらにつないでしまいます。** ペアリングする機器だけをペアリング待ちにしてください。

2 台目の足し方は、[ふだんの使い方](using.md#機器を足す) にあります。

## 5. PC で使う

Orbit を PC につなぐと、USB のキーボードとマウスとして見えます。ドライバーは要りません。PC からは、本家と同じ「HID Remapper Bluetooth」という名前で見えます。

リマップの設定は [リマップの設定](configuration.md) を見てください。

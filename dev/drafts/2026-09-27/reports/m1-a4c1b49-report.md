# M1 の報告：版 `a4c1b49`（`orbit-m1-a4c1b49-ble.bin`、2026-09-30）

利用者の PC（Windows 11、USB ハブ経由）と初代 M5Dial で、`m1-handoff.md` §0 の手順（`a0d8f9e` ＋ A1〜A4 の直し）を行った。`.bin` の SHA-256 は一致（`90bcecdc…07d2`）。書き込みモードへは PC 側から 1200 bps で入れた。ログは COM11（115200 bps、DTR あり）で、起動の最初から取れた。`M1` の行は全部ファイルに残した（`m1-a4c1b49-M1-lines.log` 3,155 行。利用者の PC に保存、リポジトリには入れていない）。機器：IST Trackball（static `21:96`）、機器 B（public `71:2c`）。

## 結論

**A1・A2・A4 は直った。A3 と、機器 B の RST 後のつなぎ直しは不合格。**

| 項目 | 結果 | 要点 |
| --- | --- | --- |
| 1. 書き込み後 RST（IST） | **合格** | `encryption on (stored key)`。`(no key stored)` は 0 回。起動から 0.66 秒で `subscribed 1` |
| **2. 機器 B を Pair new device** | **合格** | `device started pairing itself` → `encryption on (new pairing) pairing mode` → `subscribed 6`。切れなかった。ペダルが PC で効いた（利用者が確認、ログでも入力 22 件） |
| **2. その後 RST して `(stored key)` でつながり直す** | **不合格** | IST は `(stored key)` でつながった。**機器 B は 3 回とも `no encryption after 5 s` で切れ、使えなかった**（下記 1） |
| **3. A4：古い鍵から抜け出す** | **合格** | Forget all devices なしで抜けた。12:14 は `no encryption after 5 s, forgetting the stored key (pairing mode)`、12:26 は同じ行に続けて `stored key failed status=0x23d, pairing again (pairing mode)` |
| **4. A3：片方の暗号化が進まないとき、もう片方が待たされない** | **不合格** | RST 後、IST は 2.6 秒でつながったが `security start failed rc=0x6, retrying` で待たされ、機器 B が切れた後の 7.78 秒で `subscribed`（a0d8f9e は 7.29 秒）（下記 2） |
| 5. `WARNING`、止まる不具合 | **合格** | どちらも 0 回 |

`broken 128-bit UUID`：0 回。BLE ホストのリセット：0 回。`discovery stalled`・`nothing subscribed`：0 回。A9：`heap_min` の最小 93,616。`lost=0`。A8：`LAT` 11,078 件、平均 0.90 ms、最大 3.02 ms、`unrelated=0`。

## 直してほしいこと・決めてほしいこと

### 1. 機器 B が保存した鍵でつながらない（決めてほしい）

- 新しくペアリングすると使える。ところが RST の後や、ペアリングモードでない時につなぐと、保存した鍵での暗号化に 5 秒答えず切れる（12:18:02、12:18:40、12:18:47）。そのあと 30 秒ごとにくり返す。
- 通常時につなぐと、機器 B は毎回 `device started pairing itself`（自分からペアリングを始める）。ペアリングモードの 12:26:20 は自分から始めず、M5Dial が保存した鍵で暗号化を始めたが、それにも答えなかった。
- 見立て（推測）：機器 B は鍵を覚えず、つなぐたびにペアリングし直す機器かもしれない。Windows などは、登録済みの機器からのペアリングし直しを黙って受け付けるので使えると思われる。M5Dial が保存した鍵を正しく使えていない可能性も残る。どちらかはログから区別できない。
- **この機器を使うには、ペアリングモード外でのペアリングし直しを受け付けるしかない。** これは 3c46caa・b8d689d で塞いだ穴そのもの。案：
  - 機器ごとに「ペアリングし直しを受け付ける」を利用者が選べるようにする（M2 の台帳の項目）。
  - または受け付けず、README の「使えない機器」に書く。
- 見分けるには、ペアリングで配られた鍵の種類（機器が鍵を配ったか、Secure Connections か Legacy か、bonding フラグ）をログに出すとよい。

### 2. A3 がこの場面では直っていない

- RST 直後、機器 B が先につながり（t=0.87）、保存した鍵での暗号化が進まないまま 5 秒待った。IST は t=2.6 でつながったが、`security start failed rc=0x6, retrying`（NimBLE の暗号化の処理が 1 つずつしか動かない）で待たされ、機器 B が切れた後の t=7.5 に `security started` → t=7.78 に `subscribed`。
- 案：暗号化が 1 台で詰まっているとき、もう 1 台が来たら詰まっている方を早めに切る。または暗号化の待ちを 5 秒より短くする。

### 3. 細かいこと

- ペアリングモードで古い鍵を消したとき、同時に 30 秒外していた（12:14:53 `forgetting the stored key` の直後に `dropping the link, ignoring the device for 30 s`）。つなぎ直しまで 30 秒待った。12:26 は外されず 4 秒でつなぎ直した。鍵を消した直後は外さない方が早い。
- 12:26:26 は、同じ失敗について 3 行出た（`forgetting the stored key`、`no encryption after 5 s, dropping the link`、`stored key failed ..., pairing again`）。そのあと `pairing start failed rc=0x6, reconnecting`。結果は正しかったが、ログが読みにくい。

## 気づいたこと

- 機器 B は、つながると接続の間隔を 15 ms に広げるよう求める。M5Dial の画面ではこの機器の行が黄色（7.50 ms 以外）になる。ペダルなので影響はない。
- IST Trackball のアドレスはこの版の間、変わらなかった（`21:96`）。

## 時系列（PC の時刻、JST）

| 時刻 | 操作 | 結果 |
| --- | --- | --- |
| 11:56:54 | 1200 bps で書き込みモード → 書き込み → RST | 12:11:57 `START app=a4c1b49`、`bonds=2`。IST が `(stored key)` → 0.66 秒で `subscribed 1` |
| 12:12:52〜12:14:17 | （通常時） | 機器 B（古い鍵）が 3 回つながり、暗号化できずに切れる |
| 12:14:29 | Pair new device | 12:14:53 `forgetting the stored key (pairing mode)` → 30 秒外す → 12:15:24 `(new pairing) pairing mode` → 12:15:25 `subscribed 6`。ペダルが効く |
| 12:18:01 | **M5Dial の RST** | IST は 7.78 秒で `(stored key)` → `subscribed 1`。機器 B は 3 回とも `no encryption after 5 s` |
| 12:26:08 | Pair new device | 12:26:26 `stored key failed ..., pairing again (pairing mode)` → 12:26:30 `(new pairing) pairing mode` → 12:26:31 `subscribed 6` |

## 変わらないこと・未確認

- MD600・meteorite40 では試していない。
- 補正が働く機器（ゼロ埋めの 128 ビット UUID）はまだ無い。
- A5（BIOS）は保留のまま。

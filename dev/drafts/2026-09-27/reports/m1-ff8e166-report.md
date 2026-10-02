# M1 の報告：版 `ff8e166`（`orbit-m1-ff8e166-ble.bin`、2026-09-30）

利用者の PC（Windows 11、USB ハブ経由）と初代 M5Dial で、`m1-handoff.md` §0 の手順（`a4c1b49` ＋ 鍵の種類のログ）を行った。`.bin` の SHA-256 は一致（`d5b55f70…6eb1`）。書き込みモードへは PC 側から 1200 bps で入れた。ログは COM11（115200 bps、DTR あり）で、起動の最初から取れた。`M1` の行は全部ファイルに残した（`m1-ff8e166-M1-lines.log` 3,433 行。利用者の PC に保存、リポジトリには入れていない）。機器：IST Trackball（static `21:96`）、機器 B（public `71:2c`）。

この版の前に `a4c1b49` も試した（報告は `m1-a4c1b49-report.md`）。結果は `ff8e166` と同じ傾向だった。

## 結論

**機器 B が保存した鍵に「答えない」のではなかった。** 5 秒で切った時点で、接続は保存した鍵で暗号化済み（`link enc=1 bonded=1 key_size=16`）だった。暗号化が済んだという知らせ（`BLE_GAP_EVENT_ENC_CHANGE`）が、アプリに 5 秒間届いていない。M5Dial（NimBLE）側の問題と見ている。

**A3 は悪くなった。** 機器 B の暗号化が詰まっている間に来た IST も、5 秒の見張りで切られた。

| 項目 | 結果 | 要点 |
| --- | --- | --- |
| 1. 書き込み後 RST、IST が `(stored key)` | **合格** | `encryption on (stored key)`。`(no key stored)` は 0 回 |
| **2. 機器 B を Pair new device** | **合格** | `device started pairing itself` → `encryption on (new pairing) pairing mode` → `subscribed 6`。切れなかった。ペダルが PC で効いた（利用者が確認） |
| **3. RST 後の 機器 B** | **（報告のみ）つながらない** | 起動のたびに `no answer: link enc=1 auth=0 bonded=1 key_size=16`。下記 1 |
| **4. A4** | **合格** | 電源を切り、Pair new device、電源を入れる → `no encryption after 5 s, forgetting the stored key (pairing mode)` → 1.5 秒後に `(new pairing) pairing mode` → `subscribed 6`。Forget all devices なし。押してから 11.1 秒 |
| **5. A3** | **不合格** | RST 後、IST が使えるまで 43.7 秒（`a0d8f9e` 7.29 秒、`a4c1b49` 7.78 秒）。下記 2 |
| 6. `WARNING`、止まる不具合 | **合格** | どちらも 0 回 |

`broken 128-bit UUID`：0 回。BLE ホストのリセット：0 回。`device asked to pair again`（`BLE_GAP_EVENT_REPEAT_PAIRING`）：0 回。A9：`heap_min` の最小 93,280。`lost=0`。A8：`LAT` 9,771 件、平均 0.90 ms、最大 2.57 ms、`unrelated=0`。

## 3 行の値（手順 2・3 で頼まれたもの）

### 機器 B、Pair new device でペアリングしたとき（13:36:44、13:47:10。2 回とも同じ）

```
after pairing: link enc=1 auth=0 bonded=1 key_size=16
after pairing: no stored peer keys
after pairing: no stored our keys
```

- `pairing complete` の時点では、鍵がまだ保存されていない。起動時には鍵がそろっている（下）ので、この後で保存されている。

### 機器 B、通常時に保存した鍵でつないだとき（11 回すべて同じ）

```
no answer: link enc=1 auth=0 bonded=1 key_size=16
no answer: stored peer keys ltk=1 irk=1 csrk=0 sc=0 auth=0 key_size=16 ediv_rand=set
no answer: stored our keys ltk=1 irk=1 csrk=0 sc=0 auth=0 key_size=16
```

- 5 秒で切った直後に、`pairing complete status=0x0`（`after pairing:` の 3 行は上と同じ値）、続けて `encryption failed status=0x23d`（`at failure:` の 3 行も同じ値）がまとめて届く。
- **`bonded=1`、`sc=0`（Legacy）、`ediv_rand=set`**。鍵は双方の分がそろっている。

### 機器 B、ペアリングモードで古い鍵が残っていたとき（2 回）

```
no answer: link enc=1 auth=0 bonded=0 key_size=0
```

- 機器が始めたペアリングの途中（鍵を配る前）で止まっていたと見ている。

### 参考：IST Trackball が保存した鍵でつないだとき

```
after pairing: link enc=1 auth=0 bonded=1 key_size=16
after pairing: stored peer keys ltk=1 irk=1 csrk=0 sc=0 auth=0 key_size=16 ediv_rand=set
after pairing: stored our keys ltk=0 irk=1 csrk=0 sc=0 auth=0 key_size=0
```

- IST では `security started` から 0.04 秒で `pairing complete` → `encryption on (stored key)` が届く。
- `pairing complete` は、保存した鍵での暗号化でも出る（新しいペアリングの印ではない）。
- IST は M5Dial 側の鍵（our keys の LTK）を持っていない。前の版で `(no key stored)` になった理由はこれと見ている。

## 直してほしいこと

### 1. 保存した鍵での暗号化が済んでいるのに、完了の知らせが届かない（機器 B）

- 事実：通常時、機器 B は保存した鍵で暗号化される（5 秒の時点で `enc=1 bonded=1 key_size=16`）。ところが `BLE_GAP_EVENT_ENC_CHANGE` と `PAIRING_COMPLETE` は、M5Dial が切るまで届かない。
- 機器 B は、つながるとすぐ自分からペアリングを始める機器（Security Request を送ると見ている）。M5Dial も暗号化を始めるので、2 つが重なって NimBLE の中の処理が終わらないのではないか（推測）。IST（自分からは始めない）では起きない。
- 確かめてほしい：NimBLE が、暗号化の途中で届いた Security Request をどう扱うか。機器 B の Security Request の中身（bonding、MITM、SC の要求）もログに出してほしい。
- 案：5 秒の見張りで切る前に接続の状態を見て、`enc=1 bonded=1 key_size=16` なら「保存した鍵で暗号化済み」として進める。保存した鍵で暗号化できるのは本物だけなので、ペアリングモード外で組み直す穴にはならない。ただし、その後に来る NimBLE の知らせとの整合は要確認。

### 2. 詰まった機器の待ちに巻き込まれて、健全な機器まで切られる（A3、悪化）

- 13:45:07 の RST：機器 B が t=1.1 でつながり、暗号化が詰まる。IST は t=2.24 でつながったが `security start failed rc=0x6, retrying`（NimBLE の暗号化の処理は 1 つずつ）。
- t=6.45 に 機器 B が 5 秒で切られ、IST は t=7.50 に `security started`。**ところが同時に IST の 5 秒の見張りも切れ**、`no answer: link enc=0 ...` → `no encryption after 5 s, dropping the link` → `encryption failed, dropping the link, ignoring the device for 30 s`。
- 30 秒明け（t=37.9）も 2 台がほぼ同時につながり、同じ流れで 2 台とも切られた。IST はその 0.5 秒後につながり直し、t=44.3 に `(stored key)`。**IST が使えるまで 43.7 秒。**
- 案：5 秒の見張りは、自分の暗号化を始めたとき（`security started`）から数える。`security start failed rc=0x6` で待っている間は数えない。1 を直せば 機器 B の詰まりも無くなるが、ほかの機器でも起きうるので両方直してほしい。

### 3. 細かいこと

- `pairing complete` と `after pairing:` は、保存した鍵での暗号化でも出る。新しいペアリングと紛らわしいので、行の名前を変えるとよい（例：`security done`）。
- 手順 4 のあと、5 秒で鍵を消したときは 30 秒外さず、1.5 秒でつなぎ直した。`a4c1b49` で見た「鍵を消した直後に 30 秒外す」は、この版では起きなかった。

## 時系列（PC の時刻、JST）

| 時刻 | 操作 | 結果 |
| --- | --- | --- |
| 13:28:34 | 1200 bps で書き込みモード → 書き込み → RST | 13:31:19 `START app=ff8e166`、`bonds=2` |
| 13:31:19〜13:35:59 | （通常時） | 機器 B（前の版でペアリングした鍵）が 8 回つながり、毎回 `no answer: link enc=1 bonded=1` で切られる |
| 13:32:46 | IST を起こす | 機器 B が切れるまで待たされ、13:32:48 に `(stored key)` → `subscribed 1` |
| 13:36:36 | Pair new device | 13:36:43 `forgetting the stored key (pairing mode)` → 13:36:44 `(new pairing) pairing mode` → 13:36:45 `subscribed 6`。ペダルが効く |
| 13:45:06 | **M5Dial の RST** | 機器 B は `no answer: link enc=1 bonded=1`。IST も巻き込まれて切られ、30 秒外される。IST は 13:45:50 に `(stored key)`（起動から 43.7 秒） |
| 13:47:00 | 機器 B の電源を切る → Pair new device → 電源を入れる | 13:47:08 `forgetting the stored key (pairing mode)` → 13:47:10 `(new pairing) pairing mode` → 13:47:11 `subscribed 6` |

## 変わらないこと・未確認

- MD600・meteorite40 では試していない。
- 補正が働く機器（ゼロ埋めの 128 ビット UUID）はまだ無い。
- A5（BIOS）は保留のまま。

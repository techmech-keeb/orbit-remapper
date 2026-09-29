# M1 の報告：版 `e951dbc`（`orbit-m1-e951dbc-ble.bin`、2026-09-29）

利用者の PC（Windows 11、USB ハブ経由）と初代 M5Dial で、`m1-handoff.md` §0 の手順（`1fd6c6a` ＋ 4 コミット）を行った。`.bin` の SHA-256 は一致（`bee72521…e866`）。書き込みモードへは PC 側から 1200 bps で入れた。ログは COM11（115200 bps、DTR あり）。`M1` の行は全部ファイルに残した（`m1-e951dbc-M1-lines.log` 4,371 行。利用者の PC に保存、リポジトリには入れていない）。機器：MD600（キーボード）、meteorite40（トラックボール付きキーボード）。

## 結論

**合格の基準をすべて満たした。** ただし手順 3 の後半で、暗号化の行が手順書と違った（下記 1）。

| 項目 | 結果 | 要点 |
| --- | --- | --- |
| 1. 書き込み後の接続 | **合格** | 2 台とも `encryption on (stored key)`。2 台とも眠っていたので、起こした時刻でつながった。`connecting via accept list` から `subscribed` まで MD600 0.53 秒、meteorite40 0.47 秒 |
| **2. M5Dial の RST 5 回** | **合格** | 5 回とも 10 秒以内（4.66〜5.20 秒）。5 回目に meteorite40 で `0x3E` が 1 回出たが、外されずに 1.5 秒後につながった |
| **3. 機器側だけ BT クリア（meteorite40）** | **合格** | `device lost the bond; press Pair new device ...` で切られ、2 回目で 30 秒外された。30 秒明けも同じ流れを 2 回くり返した。**`encryption on` には一度もならなかった**。`WARNING` 0 回 |
| 3. そのあと Pair new device | **合格（行が違う）** | 使えるようになった。ただし暗号化の行は `(new pairing) pairing mode` で、手順書の `(new pairing, replaced the stored key) pairing mode` ではなかった。1 回目は 5 秒で暗号化されずに切られ、2 回目で通った |
| 4. 機器の電源の入れ直し | **合格** | MD600：`connecting via accept list after 282.8 s` → 0.52 秒で `subscribed 4`。meteorite40：`after 12.1 s` → 0.49 秒で `subscribed 3`。どちらも `(stored key)` |
| 5. 3 の間の MD600 | **合格** | 切断 0。3 の最初の 63 秒で入力があった秒数は 38 秒 |
| MD600 側の再ペアリング（誤操作） | **設計どおり** | 下記「気づいたこと」 |
| A5 BIOS | 保留 | |

`WARNING`：0 回。止まる不具合（`connect attempt stuck`）・BLE ホストのリセット：0 回。`discovery stalled`・`nothing subscribed`：0 回。A9：`heap_min` の最小 91,576。`lost=0`。A8：`LAT` 785 件、平均 0.79 ms、最大 2.41 ms、`unrelated=0`。

### 手順 2 の詳細（M5Dial の RST 5 回、`ble ready` は各回 t≈0.29、`bonds=3`）

| 回 | meteorite40 `subscribed` | MD600 `subscribed` | `0x3E` |
| --- | --- | --- | --- |
| 1 | t=4.04 | t=4.66 | なし |
| 2 | t=4.18 | t=4.80 | なし |
| 3 | t=4.25 | t=4.87 | なし |
| 4 | t=4.25 | t=4.87 | なし |
| 5 | t=5.20 | t=4.32 | meteorite40 で t=3.71 に 1 回 → 外されずにやり直し |

`1fd6c6a` で 38.7 秒かかった場面（`0x3E` 2 回で 30 秒外される）は、この版では起きなかった。

### 手順 3 の詳細（meteorite40 だけ BT クリア）

| 時刻 | できごと |
| --- | --- |
| 22:21:19 | BT クリアで meteorite40 が切断（`hci=0x13`） |
| 22:21:22〜23 | `0x3E` が 3 回 → 数えずにやり直し |
| 22:21:23.7 | つながる → **`device lost the bond; press Pair new device to pair it again, dropping the link`** |
| 22:21:24.5 | もう一度つながる → 同じ行に **`, ignoring the device for 30 s`** |
| 22:21:57〜58 | 30 秒明けに同じ流れ（1 回目 → `0x3E` → 2 回目で 30 秒外す） |
| 22:22:31〜32 | もう一度同じ流れ |
| 22:23:02.4 | Pair new device → `accept list wait ended status=0x9` → `scan start (pairing)` |
| 22:23:02.8 | つながる → `device lost the bond, pairing again (pairing mode)` |
| 22:23:07.8 | `no encryption after 5 s, dropping the link` |
| 22:23:08.3 | もう一度 `scan start (pairing)` → つながる |
| 22:23:09.1 | **`encryption on (new pairing) pairing mode`** |
| 22:23:09.8 | `subscribed 3`（`hub_port=3`） |

## 直してほしいこと・確かめてほしいこと

### 1. 鍵を置き換えたのに `replaced the stored key` が出なかった

- meteorite40（`e4:f9`）は、M5Dial にペアリング情報が残ったままだった（起動時の `bond 2 addr=..:e4:f9`）。そこに Pair new device でペアリングし直したので、手順書では `(new pairing, replaced the stored key) pairing mode` になるはずだった。
- 実際は `(new pairing) pairing mode` だった。`device lost the bond, pairing again (pairing mode)` の段階で古い鍵を先に消していて、その後のペアリングが「新規」に見えている可能性がある（推測）。
- 割り当ては、これまでの `hub_port=2` から `hub_port=3` に変わった。設定ツールでポートごとの割り当てを作っていると、組み直しのたびに効かなくなるかもしれない。
- ペアリング情報が何件になったかは、次に起動するまで分からない（この試験の最後の起動では `bonds=3`：MD600 の古い `d2:1b`、meteorite40 `e4:f9`、MD600 の新しい `8f:d3`）。

### 2. ペアリングモードでの 1 回目が 5 秒待たされる

- `device lost the bond, pairing again (pairing mode)` のあと、5 秒たっても暗号化されずに切られた。2 回目は 0.4 秒で通った。
- meteorite40 がまだ前の接続の状態を引きずっていたのか、ペアリングの始め方の問題かは分からない。入力できるまで 7.4 秒で、使う上では困らない。

### 3. 細かいこと

- `connecting via accept list after N s of waiting` の N は、待つ台数が変わるたびに 0 に戻る。例：起動直後、meteorite40 がつながった 0.1 秒後に MD600 がつながると `after 0.1 s` になる。「その機器を待ち始めてから」ではない。電源の入れ直しのときは、切れてから現れるまでの時間と合っていた。
- 機器側で鍵を消したまま置いておくと、30 秒ごとに 2 回つないで切る。使えないままなので害はないが、30 秒ごとに無線を使う。

## 気づいたこと

- **MD600 側の再ペアリング（誤操作）**：22:10:03 に利用者が誤って MD600 の再ペアリングを押した。MD600 は切断し（`hci=0x13`）、新しいアドレスで広告した。M5Dial は許可リストで古いアドレスを待ったままで、新しいアドレスにはつながらなかった（ペアリングモード外なので正しい）。Pair new device を押すと、1.15 秒で `encryption on (new pairing) pairing mode`（新アドレス `8f:d3`）→ `subscribed 4` になった。
- **ペアリング情報が 3 件になった**：MD600 の古い `d2:1b` が残っている（M2 で扱う予定の件）。ESP-IDF の既定の上限が 3 件なら、次に新しい機器をペアリングするときに失敗するかもしれない（未確認）。

## 時系列（PC の時刻、JST）

| 時刻 | 操作 | 結果 |
| --- | --- | --- |
| 22:07:54 | 1200 bps で書き込みモード → 書き込み → RST | 22:08:37 `START app=e951dbc`、`bonds=2`、`waiting for 2 bonded device(s)` |
| 22:09:25〜33 | 2 台を起こす | MD600 → meteorite40 の順に `(stored key)` でつながる |
| 22:10:03 | MD600 の再ペアリング（誤操作） | MD600 が切断。新しいアドレスにはつながらない |
| 22:11:40 | Pair new device | MD600 が新アドレス `8f:d3` で `(new pairing) pairing mode` → 1.15 秒で `subscribed 4` |
| 22:12:23〜22:20:24 | **M5Dial の RST を 5 回** | 5 回とも 4.66〜5.20 秒で 2 台そろう |
| 22:21:19 | **meteorite40 だけ BT クリア** | `device lost the bond` で切る → 2 回目で 30 秒外す、をくり返す。MD600 は使えたまま |
| 22:23:02 | Pair new device | 1 回目は 5 秒で切れ、2 回目で `(new pairing) pairing mode` → 22:23:09.8 に `subscribed 3` |
| 22:24:04〜22:28:47 | MD600 の電源の入れ直し | `after 282.8 s` → 0.52 秒で `subscribed 4` |
| 22:30:00〜22:30:13 | meteorite40 の電源の入れ直し | `after 12.1 s` → 0.49 秒で `subscribed 3` |

## 変わらないこと・未確認

- A5（BIOS）は保留のまま。
- ペアリング情報の件数と上限（上記）は未確認。
- この版では、10 分の連続使用とスリープからの復帰は行っていない（`1fd6c6a` では合格）。

# M1 の報告：版 `66521d2`（`orbit-m1-66521d2-ble.bin`、2026-09-30）

利用者の PC（Windows 11、USB ハブ経由）と初代 M5Dial で、`m1-handoff.md` §0 の手順（`2e08ac7` ＋ 保存領域と機器の情報のログ、PR #22）を行った。`.bin` の SHA-256 は一致（`2fc099f6…8e30`）。書き込みモードへは PC 側から 1200 bps で入れた。ログは COM11（115200 bps、DTR あり）で、起動の最初から取れた。`M1` の行は全部ファイルに残した（`m1-66521d2-M1-lines.log` 1,984 行。利用者の PC に保存、リポジトリには入れていない）。機器：IST Trackball（static `21:96`）、Cube Turner PRO（public `71:2c`）。

## 結論

**合格の基準をすべて満たした。** 接続の処理に回帰は無く、機器の情報は 2 台とも全部読めた。情報の読み出しが入力を遅らせている様子は無い。

| 項目 | 結果 | 要点 |
| --- | --- | --- |
| 1. `M1 NVS` の 3 行 | 取れた | 下記。保存領域は 4,032 件中 168 件だけ使用。ペアリング情報 2 台分で 32 件 |
| 2. `info` の 5 行と `hash=` | 取れた | 下記。2 台とも 5 項目すべて読めた（`read failed` 0 回）。Cube Turner は PnP ID だけ `none` |
| 3. Pair new device で `adv_name=` | 取れた | Cube Turner の広告には名前も外観も無かった（`adv_name="" adv_appearance=0x0000`）。下記 1 |
| **4. RST 1 回** | **合格** | 起動から 4.26 秒で 2 台そろう。IST は `(stored key)`、Cube Turner は `(stored key), found by polling`。`WARNING`・`dropping the link` 0 回 |
| **5. 5 分の使用** | **合格** | 21:07:00〜21:12:00。切断 0、2 台とも 290 秒すべて接続、`lost=0`。入力は IST 178 秒、Cube Turner 17 秒。`LAT` 16,637 件、平均 0.70 ms、**最大 2.32 ms**、`unrelated=0` |
| 5. 情報の読み出しの影響 | **影響なし** | `subscribed` から `info pnp_id` の行まで 0.08〜0.15 秒。その直後の最初の `LAT` の最大は 1.08〜1.94 ms |

書き込み後の起動（21:03:30）では、2 台とも NimBLE の知らせが普通に届く `encryption on (stored key)` で、起動から 1.58 秒で 2 台そろった。

`WARNING`・`dropping the link`・`encryption changed again`・`could not start encryption`・`no answer`・止まる不具合・BLE ホストのリセット・`broken 128-bit UUID`：すべて 0 回。A9：`heap_min` の最小 93,200。`lost=0`。A8：この版全体で `LAT` 20,153 件、平均 0.72 ms、最大 2.32 ms、`unrelated=0`。

### `M1 NVS` の 3 行（起動時、そのまま）

```
M1 NVS t=0.204 err=0x0 entries used=168 free=3864 available=3738 total=4032 (32 B each) namespaces=3 max_bonds=4 accept_list_max=12 max_conns=2
M1 NVS t=0.206 namespace orbit: entries=67 config=1x2048B
M1 NVS t=0.210 namespace nimble_bond: entries=32 our_sec=2x88B peer_sec=2x88B rpa_rec=1x14B csfc_sec=2x8B local_irk=1x23B
```

- ペアリング情報 1 件あたり：`our_sec` 88 B ＋ `peer_sec` 88 B ＋ `csfc_sec` 8 B。`rpa_rec` と `local_irk` は全体で 1 件ずつ。
- 2 台分で 32 エントリなので、1 台あたり 16 エントリ前後。空き（3,738 エントリ）からは保存領域が上限にならない。許可リストの上限は 12 台。

### `info` の 5 行と `hash=`

IST Trackball（21:03:30、21:06:44 の 2 回とも同じ）：

```
report map 92 byte(s) hash=5c7471f4, hub_port=1
info name="IST TrackBall" (13 byte(s))
info appearance=0x03c2
info manufacturer=" PixArt Inc." (12 byte(s))
info model="MS 2822" (7 byte(s))
info pnp_id: vendor_source=2 vid=0x056e pid=0x0184 version=0x0078
```

Cube Turner PRO（21:03:31、21:05:00、21:06:41 の 3 回とも同じ）：

```
report map 297 byte(s) hash=b5fa2fcc, hub_port=2
info name="TurnerPro" (9 byte(s))
info appearance=0x0080
info manufacturer="sincoaudio" (10 byte(s))
info model="ble device" (10 byte(s))
info pnp_id: none
```

- `hash=` は接続ごとに変わらなかった。
- IST の製造者名は、先頭に空白が 1 つ入っている（機器が返した文字のまま）。表示に使うなら前後の空白を削るとよい。
- Cube Turner の外観 `0x0080` は「一般的なコンピューター」の区分で、HID ではない。種類の判定には記述子の方が頼りになる。
- 手順書には「Cube Turner は UUID が壊れている機器なので、`info` が `none` か `read failed` になるかもしれない」とあった。実際には `broken 128-bit UUID` の行は一度も出ず、5 項目とも読めた。Cube Turner は正しい 16 ビットの UUID を使っていると見ている。補正が働く機器は、今も手持ちに無い。

### Pair new device での `connecting` の行（Cube Turner、21:04:58）

```
connecting rssi=-43 adv_type=0 addr_kind=public adv_name="" adv_appearance=0x0000
```

## 確かめてほしいこと

### 1. 広告の名前が空になる

- Cube Turner の広告からは、名前も外観も取れなかった。接続後の GAP の Device Name からは `TurnerPro` が読めている。
- 利用者によると、Windows につないだときは名前が表示された。
- M5Dial の探索は `passive` の指定がないので、スキャン応答を求める形になっている。ただ、最初の広告を受けた時点でつなぎに行くので、あとから届くスキャン応答の名前は `adv_name` に入らないと見ている（推測）。
- 案：ペアリングの探索では、スキャン応答が届くまで少し待つか、広告とスキャン応答の中身を合わせて見る。つないだあとに読んだ名前を使うなら、今のままでも困らない。

## 時系列（PC の時刻、JST）

| 時刻 | 操作 | 結果 |
| --- | --- | --- |
| 21:02:06 | 1200 bps で書き込みモード → 書き込み → RST | 21:03:30 `START app=66521d2`、`M1 NVS` 3 行、`bonds=2`。1.58 秒で 2 台そろう。`info` を読む |
| 21:04:49 | Pair new device | 21:04:58 Cube Turner を探索でつかむ（`adv_name=""`）→ 保存した鍵で `pairing mode, found by polling` → 21:05:00 `subscribed 6`、`info` を読む |
| 21:06:39 | **RST** | 4.26 秒で 2 台そろう |
| 21:07:00〜21:12:00 | **5 分の使用** | 切断 0、`lost=0`、`LAT` 最大 2.32 ms |

## 変わらないこと・未確認

- Pair new device でのペアリングし直しは、Cube Turner が保存した鍵のまま通ったので、新しいペアリングにはならなかった（ペアリング情報の件数は 2 のまま）。
- Cube Turner の LED の点滅の意味（PC でも点滅する）。
- スリープからの復帰（Cube Turner は自動で眠らない）。
- 機器側で鍵を捨てたときの扱い（Cube Turner でペアリングを消す方法が分からない）。
- MD600・meteorite40 の回帰確認（手順書の「行う場合」）は行っていない。
- 補正が働く機器（ゼロ埋めの 128 ビット UUID）はまだ無い。
- A5（BIOS）は保留のまま。

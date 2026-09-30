# M2 の報告：版 `230e758`（`orbit-m2-230e758-ble.bin`、2026-10-01）

利用者の PC（Windows 11、USB ハブ経由）と初代 M5Dial で、`m1-handoff.md` §0 の確認（`cd199a3` ＋ 台帳を書く時機の条件の変更：期限 60 秒、期限後も入力が 200 ms 空くのを待つ、PR #27）を行った。`.bin` の SHA-256 は一致（`00c51265…ce4c`）。書き込みモードへは PC 側から 1200 bps で入れた。ログは COM11（115200 bps、DTR あり）で、起動の最初から取れた。命令は `102144d` と同じ方法で送った。IST を動かし続ける場面では、ログで入力が届いているのを確かめてから命令を送った。`M1` の行は全部ファイルに残した（`m2-230e758-M1-lines.log` 3,204 行。利用者の PC に保存、リポジトリには入れていない）。機器：IST Trackball（static `21:96`）、Cube Turner PRO（public `71:2c`）。

## 結論

**合格の基準を満たしていない。** 新しい条件（期限 60 秒、200 ms の切れ目を待つ）は設計どおりに動いた。ただし次の 3 点が見つかった。

1. **ペアリングモードで `WARNING: key replaced outside pairing mode` が 1 回出て、接続が切られた**（誤った警告。下記 1）。
2. **入力を続けている最中の期限後の保存で、`LAT` が 9.04 ms になった**（200 ms の切れ目では足りない。下記 2）。
3. **ペアリングの間、IST の入力が大きく減った**（1 回目は約 20 秒ほぼ 0、2 回目は 1〜2 秒 6〜9 件/秒。下記 3）。

| 項目 | 結果 | 要点 |
| --- | --- | --- |
| **1. IST を動かしながら `orbit alias 2 pedal`** | **待つ動きは合格、`LAT` は不合格** | 02:21:59 に命令。利用者は 65 秒以上止めずに動かし続け、命令から 64.7 秒後の 02:23:04 に `ledger saved (1860 B, 9.5 ms, why=set_alias)`（60 秒の期限を過ぎ、200 ms の切れ目で書いた）。命令から保存の前までの `LAT` は最大 2.16 ms、`unrelated=0`。**保存の秒の `LAT` は最大 9.04 ms**。`orbit alias 2`（入力なし）は命令の 1 ms 後に保存 |
| **2. 動かしながら `orbit forget 2` → `orbit pair`（1 回目）** | **不合格（`WARNING`）** | `forget` の保存は直後に 1 回（`forced`）。`orbit pair` → Cube Turner の 1 回目の接続で **`WARNING: key replaced outside pairing mode`** → 切断 → 2 回目の接続でポート 2（下記 1）。この間、IST の入力がほぼ届かなかった（下記 3） |
| 2. 同じ（2 回目、02:31:46） | **保存は分かれた、`LAT` は合格** | 入力が 1 秒に 80 件以上届いているのを確かめてから送った。`forget` の保存は直後に 1 回（`forced`、その秒の `LAT` 5.43 ms）。`orbit pair` → `encryption on (new pairing) pairing mode`（NimBLE の知らせが普通に届いた）→ ポート 2、`held` から `hub_port=2` まで 0.99 秒。保存は **2 回**（`add` 33.5 ms、`set_map` 9.3 ms）。ペアリングの間に IST の入力が 6〜9 件/秒に落ち、それを「手が止まった」と判断して途中で書いた。`forced` の秒を除く `LAT` は最大 2.52 ms |
| **3. 3 分の使用** | **合格** | 02:27:49〜02:30:49。切断 0、2 台とも 175 秒すべて接続、`lost=0`。`LAT` 8,183 件、平均 0.80 ms、**最大 2.71 ms**（3 ms を超える秒は 0）、`unrelated=0`。入力は IST 109 秒、Cube Turner 12 秒。この間の保存は 0 回 |

書き込み後の起動では、保存 2 回（`touch` 9.2 ms、`touch` 35.4 ms）とも入力の前だった。Cube Turner は起動から 12.7 秒後につながった。

`WARNING`：**1 回**。`ledger save failed`・`no answer`・止まる不具合・BLE ホストのリセット・`broken 128-bit UUID`：0 回。`dropping the link` は 4 回（`forgotten` 2 回、`key replaced outside pairing mode` 1 回、`nothing subscribed` 1 回）。`heap_min` の最小 87,488。A8：この版全体で `LAT` 27,621 件、平均 0.81 ms、最大 9.04 ms、`unrelated=0`。

### この版の保存（9 回すべて）

| 時刻 | 大きさ・時間・理由 | そのときの入力 |
| --- | --- | --- |
| 02:21:01.855 | 1860 B, 9.2 ms, `touch` | まだ無い（書き込み後の起動） |
| 02:21:13.582 | 1860 B, 35.4 ms, `touch` | まだ無い |
| 02:23:04.309 | 1860 B, 9.5 ms, `set_alias` | **動き続けていた（期限後の 200 ms の切れ目）** |
| 02:23:52.083 | 1860 B, 34.2 ms, `set_alias` | 止まっていた |
| 02:24:55.270 | 1860 B, 9.8 ms, `forget, forced` | 動いていた |
| 02:25:01.539 | 1860 B, 33.4 ms, `add` | IST の入力が届いていなかった |
| 02:31:46.518 | 1860 B, 9.7 ms, `forget, forced` | 動いていた |
| 02:31:50.134 | 1860 B, 33.5 ms, `add` | ペアリング中で IST の入力が 6 件/秒 |
| 02:31:54.986 | 1860 B, 9.3 ms, `set_map` | IST の入力が 6 件/秒 |

## 直してほしいこと

### 1. ペアリングモードで誤った `WARNING` が出て、接続が切られる

02:24:58 の流れ（Cube Turner、`orbit forget 2` の後の `orbit pair`）：

```
pair_new_device → scan start (pairing)
connecting ... adv_name="" adv_appearance=0x0000
connected ...
device started security itself (enc=0 bonded=0 key_size=0 at this moment)
waiting for encryption: enc=1 bonded=0 key_size=16
encryption on (no key stored) pairing mode, found by polling (NimBLE posted no event)
security done status=0x0
after security: link enc=1 auth=0 bonded=1 key_size=16
after security: no stored peer keys
after security: no stored our keys
WARNING: key replaced outside pairing mode
key replaced outside pairing mode, dropping the link
... nothing subscribed, dropping the link to retry
（0.6 秒後）connecting via accept list → encryption on (stored key), before the connect event → ledger: new device, port 2 → ... hub_port=2
```

- 毎秒の読み取りが、**新しいペアリングの途中**（`enc=1 bonded=0 key_size=16`、鍵を配る前）の接続を見つけ、`(no key stored) pairing mode` として受け入れた。
- 受け入れた時点でペアリング待ちが終わり、そのあとに届いた本当の完了（`security done`、鍵の保存）を「ペアリングモード外での鍵の置き換え」とみなして `WARNING` を出し、切った（推測）。
- 実害は無く、0.6 秒後のつなぎ直しでポート 2 になった。ただし `WARNING` の基準を満たさず、`da3c37f` の報告で心配した「新しいペアリングの途中を読み取りで受け入れる」形そのもの。
- 案：読み取りで受け入れるのは、保存した鍵がある接続（通常時の条件と同じ：登録済みで、鍵の指紋が変わっていない）だけにする。ペアリングモードで鍵が無い接続は、読み取りで受け入れず、`security done`／`ENC_CHANGE` を待つ。
- 2 回目（02:31:50）は同じ場面で、NimBLE の知らせが普通に届いた（`encryption on (new pairing) pairing mode`）。起きるかどうかは、Security Request と接続の知らせの前後関係で変わると見ている。

### 2. 入力を続けていると、期限後の 200 ms の切れ目でも入力と重なる

- 手順 1 では、利用者が 65 秒以上止めずに動かし続けた。60 秒の期限を過ぎた後、200 ms の切れ目で書いたが、書いている間（9.5 ms）に入力が再開し、その秒の `LAT` が 9.04 ms になった。
- 手を止めずに動かし続ける限り、切れ目の長さを変えても、書いている最中に入力が来る可能性は残る。
- 案：
  - 別名・名前・`last_used` のような、失っても困らない変更には期限を設けず、「1 秒の切れ目」でだけ書く。電源が落ちたときに失うのは、その間の変更だけ（行はペアリング情報と接続時の読み出しから戻る）。再起動・書き込みモード・`forget`・Forget all devices の前には必ず書く（今のまま）。
  - 期限を残すなら、「60 秒以上止めずに動かし続けた直後だけ、1 回、10 ms 前後の遅れが出うる」ことを仕様として認め、基準を「3 ms を超える秒が 1 分に 1 回以下」に合わせる。
- どちらでも、ふつうの使い方では問題にならない。前者の方が単純で、遅れが出ない。

### 3. ペアリングの間、IST の入力が大きく減る

| 場面 | IST の入力（件/秒） |
| --- | --- |
| 02:24:51〜02:24:55（ペアリングの前） | 60〜101 |
| 02:24:56〜02:25:23（`forget` → `pair` → 2 回の接続 → その後） | 0〜13（02:25:01〜02:25:19 は 0） |
| 02:31:47〜02:31:48（ペアリングの前） | 114〜120 |
| 02:31:49〜02:31:50（`orbit pair` → 新しいペアリング） | 6〜9 |
| 02:31:51〜02:31:53（`hub_port=2` の後） | 102〜115 |
| 02:31:54 | 6 |

- どちらの場面も、IST の接続は保たれていた（暗号化済み、`subs=1`、間隔 7.50 ms・latency 44 のまま）。
- 1 回目は約 20 秒ほぼ 0。利用者は「その間もカーソルは動いていた」と言う。IST は PC に直接つながっていなかった（PC の Bluetooth の機器一覧に無い）ので、カーソルは M5Dial 経由か、ほかのポインティング機器（PC には複数登録されている）で動いたことになる。どちらかはログから分からない。
- 2 回目は、入力が確かに届いている状態から始め、ペアリングの 1〜2 秒と 02:31:54 に 6〜9 件/秒に落ちた。
- 見立て（推測）：ペアリングの探索や新しい接続に無線の時間を取られ、IST との接続のやり取りが減っている。IST は slave latency 44 を使っているので、取りこぼすと次の機会まで長く待つのかもしれない。
- この入力の減りを「手が止まった」と判断して、ペアリングの保存が 2 回に分かれた（`add`、`set_map`）。
- 案：ペアリングの探索の窓と間隔（`scan start (pairing)` の duty）を、ほかの機器がつながっているときは下げる。ESP32-S3 の BLE の調停（scan と接続の優先度）の設定も確かめる。

## 時系列（PC の時刻、JST）

| 時刻 | 操作 | 結果 |
| --- | --- | --- |
| 02:20:13 | 1200 bps で書き込みモード → 書き込み → RST | 02:21:01 `START app=230e758`、`ledger loaded boot=10 rows=2`、保存 2 回（入力の前） |
| 02:21:59〜02:23:04 | IST を 65 秒以上動かしながら `orbit alias 2 pedal` | 64.7 秒後に保存、その秒の `LAT` 9.04 ms |
| 02:23:52 | `orbit alias 2`（入力なし） | すぐ保存 |
| 02:24:55〜02:25:01 | IST を動かしながら `orbit forget 2` → `orbit pair`（1 回目） | `forced` の保存 1 回。`WARNING` で 1 回切断、2 回目でポート 2。IST の入力がほぼ届かない |
| 02:27:49〜02:30:49 | **3 分の使用** | 切断 0、`lost=0`、`LAT` 最大 2.71 ms、保存 0 回 |
| 02:31:46〜02:31:55 | IST を動かしながら `orbit forget 2` → `orbit pair`（2 回目） | `forced` の保存 1 回、ペアリングの保存 2 回。`forced` の秒を除く `LAT` 最大 2.52 ms |

## 変わらないこと・未確認

- MD600 の B5（自動引き継ぎ）・B6（組み直しの許可）は行っていない。
- Cube Turner の LED の点滅の意味、スリープからの復帰、機器側で鍵を捨てたときの扱い。
- 補正が働く機器（ゼロ埋めの 128 ビット UUID）はまだ無い。
- A5（BIOS）は保留のまま。

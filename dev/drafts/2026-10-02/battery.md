# 接続した機器の電池の残量（検討と段階 0）

状態：段階 0（調べる）の試験待ち。2026-10-02 作成。

## 1. 何を作るか

機器を PC に直接ペアリングすると、PC が機器の Battery Service（0x180F）を読んで残量を出す。Orbit を挟むと PC からは USB のキーボード・マウスに見え、残量は Orbit で止まる。そこで Orbit が機器の Battery Service を読み、Orbit の画面と、設定ツール向けの命令に出す。

PC の OS に電池として見せる案（USB の HID 記述子に Battery Strength を足す）は見送る。記述子は本家コアのもので、足すと本家の改造になる。Windows が USB の機器の電池をこの方法で表示するかも確かめていない。

## 2. 決めたこと（2026-10-02）

| 項目 | 決定 |
| --- | --- |
| 読み方 | HID の入力が届くようにし、機器の情報を読み終えてから、Battery Service を探す。一度読み、通知に対応していれば登録する。通知の無い機器は 10 分ごとに読み直す（入力が 1 秒途切れたときだけ） |
| 値の置き場 | メモリだけ。NVS には書かない。切れた機器は表示を消す |
| 画面 | カードの 1 行目、名前の右に電池の印と %。Battery Service の無い機器は何も出さない |
| 色 | 30% 以上はふつう、11〜29% は黄、10% 以下は赤 |
| 少ないとき | 10% 以下になった時に 1 回だけ、状態の行に `Port N battery low (8%)`。リングの色は変えない。消えている画面はつけない |
| PC から | `GET_STATE`（0x80）の後ろに枠ごとの残量を足す（不明は 255）。設定のページで表示する |
| 順番 | 段階 0 を 2b の前にやる。段階 1 は段階 0 の結果を見て決める |

## 3. 段階 0：調べる

### 作ったもの

`firmware/orbit/main/ble.c` に、Battery Service を調べる処理を足した。画面にはまだ出さない。ログだけ。

- 機器の情報（名前・製造者・型番・PnP ID）を読み終えたら、1 回だけ動く。
- Battery Service を探し、その中の Battery Level（0x2A19）を最大 4 つまで集める。
- それぞれについて、記述子（CCCD 0x2902、User Description 0x2901、Presentation Format 0x2904）を探し、値と、User Description と Presentation Format を読む。
- 通知に対応していれば登録し、届いた通知をログに出す。**電池の通知は入力として本家コアに渡さない**（今までは届いた通知をすべて入力として渡していた）。

ログの行（どれも `M1 EVT ... battery` で始まる）：

| 行 | 意味 |
| --- | --- |
| `battery: N Battery Service(s)` | Battery Service の数。0 なら、その機器は残量を出さない |
| `battery: N Battery Level characteristic(s)` | 残量の値の数。分割キーボードは 2 つあるかもしれない |
| `battery K: handle=... read=1 notify=1 cccd=1 user_desc=0 presentation=1` | 値ごとの性質 |
| `battery K: level=80% (1 byte(s))` | 読んだ残量 |
| `battery K: user description="..."` | 値の説明（分割キーボードの左右など） |
| `battery K: presentation format=... description=0x....` | 値の説明（番号で表すもの） |
| `battery K: notifications on` | 通知を登録できた |
| `battery K notify level=79% (1 byte(s)) after 612.3 s` | 機器から届いた通知。つないでからの秒数つき |
| `battery: probe done` | 調べ終わった |

### 試験の手順

機器：IST Trackball、Mistel MD600、機器 B（手元にあれば、ほかの BLE 機器も）。

1. 書き込んで起動し、ログを `dev/tools/serial-capture/` で残す。
2. 3 台をつなぐ（同時に 2 台まで。1 台ずつ入れ替えてよい）。機器ごとに、上の `battery` の行を全部抜き出す。
3. 読めた残量と、機器の側の表示（あれば。メーカーのアプリ、LED、PC に直接つないだときの値）を見比べる。
4. 1 台はつないだまま 30 分以上使い、通知（`notify`）が届くか、どのくらいの間隔か、値がどう変わるかを見る。
5. 電池を調べる処理の前後で、入力が遅れないか。`LAT` の行で、3 ms を超える秒が増えていないか。調べる処理は、つないだ直後の 1 秒以内に終わる見込み（推測）。
6. 調べる処理のあとも、入力・切断・つなぎ直しが今までどおりか。

### 報告に書くこと

- 機器ごとに：Battery Service の数、Battery Level の数、read／notify、説明の有無と中身、読めた値、機器の側の値。
- 通知が届いた機器：最初の通知までの時間、間隔、値の変わり方。
- `LAT` の様子（調べる処理の前後）。
- 何かおかしなこと（ログの抜粋）。

報告は `dev/drafts/2026-09-27/reports/bat0-<版>-report.md` に置く。機器のアドレスは下位 2 バイトだけにする。

## 4. 段階 1 以降（予定）

段階 0 の結果を見て、段階 1 で次を作る：通知の無い機器の読み直し、画面のカードの表示と色、少ないときの知らせ、ログの `orbit list` の `battery=`、`GET_STATE` の残量。値が 2 つある機器の出し方（低いほうだけか、両方か）は、段階 0 の結果で決める。

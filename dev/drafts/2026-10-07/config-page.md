# 設定のページを開く（Open config page）

状態：作成済み、実機の試験待ち。2026-10-07 作成。

## 1. 何を作るか

本家の Web 設定ツール（https://www.remapper.org/config/ ）を開くには、PC で Chrome を開いてアドレスを打つ必要がある。Orbit は PC からキーボードに見えているので、そのアドレスを Orbit に打たせる。PC 側にソフトを入れずに済み、Orbit を挟んでいる人がアドレスを覚えなくてよい。

利用者の案（2026-10-07）：メニューで選ぶ → 画面に手順が出る → PC の Chrome でアドレス欄をクリック → M5Dial を一定時間タッチ → Orbit が URL を打つ。検証の結果、タッチをボタンに、URL は `https://` なしに変えた（§2）。

## 2. 決めたこと（2026-10-07）

| 項目 | 決定 | 理由 |
| --- | --- | --- |
| 打つ文字 | `www.remapper.org/config/` と Enter。`https://` は付けない | `:` は US 配列（Shift + `;`）と日本語配列（`:` のキー）で位置が違う。英小文字・`.`・`/`・`-` は両方で同じキー。ブラウザが `https://` を補う（`http://www.remapper.org/config/` は 301 で `https://` に転送されることを 2026-10-07 に確認） |
| 対象の配列 | US と日本語 | 上の理由で、この 2 つは同じ結果になる。フランス語配列などは文字の位置が違うので対象外と文書に書く |
| 出し方 | 本家のコアを通さず、USB の HID 0 に直接レポートを送る（`typist.cc`） | コアを通すと、利用者のリマップ（キーの入れ替え、レイヤー）がアドレスの文字にも効く |
| レポートの形 | 本家の記述子 0（kb_mouse）・1（absolute）：report ID 2、修飾キー 1 バイト＋キーのビット列（0x04〜0x73）14 バイト＋1 バイト。ブートプロトコル：8 バイト、ID なし。ゲームパッドの記述子（2〜5）：キーボードが無いので打たず、画面に「No keyboard」 | 本家 `our_descriptor.cc` の記述子のとおり |
| 速さ | 12 ms ごとに 1 レポート（押す・離すで 2 レポート）。25 文字で約 0.6 秒 | 1 文字ずつ押して離す。速すぎると PC 側で取りこぼすことがある |
| 操作 | メニュー「Open config page」→ 確認の画面（手順 3 行）→ 中央を押すと打つ。回すとやめる。20 秒放置でやめる | 機器が勝手に文字を打つ仕組みなので、利用者が選んで押したときだけ動かす。タッチは今の画面で使っておらず（M3 の方針）、ドライバーを足す作業が別に要る。確認の画面＋押す動作で、タッチの「一手間」と同じ効きになる |
| アドレス欄の選択 | 利用者がクリックする | Ctrl+L を Orbit が送る手もあるが、Mac は Cmd+L で OS に依る。最初は手で。あとで設定にするか考える |
| 試験用の命令 | ログの命令 `orbit openconfig` で同じことを打つ | 画面なしでも試せる |

これで、前からの質問「既定の OS は Windows か」は、ほぼ気にしなくてよくなった（US・日本語配列なら OS を問わない）。「ページの置き場（GitHub Pages）」は、本家の設定ツールをそのまま使う今は要らない。Orbit 独自の設定ページ（台帳の整理など）を作るときに改めて考える。

## 3. 作ったもの

| 場所 | 中身 |
| --- | --- |
| `main/typist.cc`・`typist.h` | 文字をキーのレポートにして HID 0 に送る。主ループから毎回 `orbit_typist_poll()` を呼ぶ。ログ：`typist: typing the config page address (24 characters, report protocol)` → `typist: done`。打てないときは `typist: no keyboard in USB descriptor N` |
| `main/ui.c` | メニューの項目「Open config page」（Forget の下、Rotate の上）。確認の画面：題「Open config page」、「In Chrome on the PC, / click the address bar, / then press the dial.」、「Turn the dial to cancel」。押すと「Typing...」が約 2 秒。記述子にキーボードが無いときは「No keyboard」が 3 秒。ログ：`screen: open config page, waiting for the press` → `typing` / `cancelled` / `timed out` |
| `main/commands.cc` | `orbit openconfig` |
| `docs/using.md`・`docs/configuration.md`・`CHANGELOG.md` | 使い方 |

## 4. 試験の手順

機器：IST Trackball（入力が続いている間の影響を見るため）。PC：Windows 11、Chrome。

1. 書き込んで起動し、ログを残す。IST をつなぐ。
2. メニューに「Open config page」が Forget の下・Rotate の上に出ること。選ぶと確認の画面（題、3 行、下に「Turn the dial to cancel」）が出ること。文字が丸い画面に収まっているか（写真）。
3. やめ方：ダイヤルを回す → 元の画面に戻る（ログ `cancelled`）。もう一度選んで 20 秒放置 → 戻る（`timed out`）。
4. 本番：Chrome で新しいタブを開き、アドレス欄をクリック → メニューで選び、確認の画面で中央を押す → 「Typing...」のあと、Chrome に `www.remapper.org/config/` が入り、設定ツールが開くこと。ログ `typist: typing ...` から `done` までの秒数。
5. 文字の確認：メモ帳などの文字を打てる場所をクリックしてから同じ操作 → `www.remapper.org/config/` と改行が、欠けも化けもなく打たれること。**Windows のキーボード配列を日本語と US の両方で**（切り替えられれば）。
6. 入力中の影響：IST のボールを回し続けながら 4 をやる → 入力が止まらないこと。前後の `LAT`（3 ms 超の秒が増えていないか）。
7. `orbit openconfig` を打つ → メモ帳に同じ文字が打たれること（画面なしの道）。
8. 画面が消えている間に中央を押す → 画面がつくだけで、メニューも打ち込みも起きないこと。
9. （できれば）本家の設定ツールで USB の記述子をゲームパッド（HORIPAD など）に変えて保存 → メニューで選んで押す → 「No keyboard」が 3 秒出て、何も打たれないこと。記述子を元（Keyboard + Mouse）に戻す。ログ `typist: no keyboard in USB descriptor 2`。

## 5. 報告に書くこと

- 2・4・5 の結果（写真、打たれた文字そのもの、配列ごと）。
- 4 の所要時間（ログの `typing` → `done`）。
- 6 の `LAT`。
- 何かおかしなこと（ログの抜粋）。

報告は `dev/drafts/2026-09-27/reports/cfg-<版>-report.md` に置く。PC の名前・ユーザー名・パスを書かない。

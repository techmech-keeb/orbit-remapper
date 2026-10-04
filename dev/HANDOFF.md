# 引き継ぎ（最新）

新しいセッションは、まずこの文書を読む。2026-10-04 時点。状態が変わったら、この文書を書き換える（古い引き継ぎの文書は残すが、正本はここ）。

## 1. 今の状態

| 項目 | 状態 |
| --- | --- |
| リポジトリ | `techmech-keeb/orbit-remapper`（public、2026-10-02 公開）。`main` は `a336e10` |
| リリース | `v0.1.0`（2026-10-02、コミット `a47ccc0`）。添付 5 つ、照合済み |
| 開いている PR | techmech-keeb/orbit-remapper#5：電池の残量の段階 0・1（ブランチ `claude/laughing-volta-r6lpn4`、先頭 `cf3a98d`、CI は成功）。IST Trackball で段階 1 は合格（`bat1-0f4c53d`・`bat1-36eb937` の報告）。**残りは `cf3a98d` の確認だけ**（下の「待っている報告」①） |
| 最近マージした PR | techmech-keeb/orbit-remapper#8：README と「はじめに」に M5Dial の購入先（スイッチサイエンスの v1.1）を、動作・技適とも未確認と書いて載せた。techmech-keeb/orbit-remapper#9：M5Dial v1.1 と Waveshare のダイヤル機を資料で確かめた結果（`porting.md`、Q02） |
| 待っている報告 | ① `bat1-cf3a98d-report.md`：電池の知らせが、機器が切れたら消えるか（手順は `battery.md` §4「再試験の結果と、切断したときの知らせ」。`.bin` は `orbit_hid-remapper_v0.1.0_20261002-cf3a98d.bin`、SHA-256 `38a11d2e5cc462c73ff624760885146286d943ea1eefdf1a4514d23096c6100c`）② Mistel MD600・機器 B の電池の段階 0（機器が手元に戻ってから。`bat0-8153e8b-report.md` に書き足す）③ `bd6895d` の試験（古い鍵での繰り返しの直しと、画面なしでの `LAT` の切り分け。[m3-screen.md](drafts/2026-10-01/m3-screen.md) §6） |
| 利用者の答え待ち | ①「設定のページを開く」機能：既定の OS は Windows か、ページの置き場（GitHub Pages を使えるか）、第一段の中身 ② 本家を取り込み直すとき `git subtree pull --squash` にするか（Contributors を増やさないため） ③ ボードの層の整理（[porting.md](drafts/2026-10-02/porting.md) §1 の 1）を 2b の前にやるか |
| 対応機種 | 動作を確かめたのは初代 M5Dial だけ。M5Dial v1.1 は、資料の上では Orbit の使う端子がすべて初代と同じで、今の `.bin` のまま動く見込み（実機・技適は未確認）。Waveshare ESP32-S3-Knob-Touch-LCD-1.8 は移植が要り、**今の `.bin` を書き込んではいけない**（[porting.md](drafts/2026-10-02/porting.md) §2 A・B'） |

`battery.md`（`dev/drafts/2026-10-02/battery.md`）と電池の報告は、PR #5 のブランチにある。マージすると `main` にも入る。

## 2. 次にやること（利用者と合意した順番）

1. 電池：`cf3a98d` の確認の報告を読み、問題がなければ PR #5 をマージしてよいか利用者に聞く。
2. 2b：機器ごとのレイヤーと、切断時の押下の解除。本家コアの唯一の改造になる予定（`// ORBIT:` の印を付ける）。依頼書は [m2-brief.md](drafts/2026-09-30/m2-brief.md)。
3. 画面にレイヤーを表示する。
4. 2c：設定セットと LED の送り返し。
5. 画面に設定セットを表示する。
6. 設定ツール（別のリポジトリの予定）。`GET_STATE` は PR #5 で電池の残量を足し、`PROTOCOL_VERSION` が 2 になる。

あとで：Mistel MD600・機器 B の電池の試験（戻ったら）、分割キーボードでの電池の出し方の見直し、M5Dial v1.1 の実機での試験と技適のラベルの確認（手に入れたら。Q02・Q24）、探索の割合（何もつながっていないときは 30/60 ms）、E1 の生成物の Release（旧リポジトリの Issue #3）、LVGL の設定を絞ってイメージを小さくする（急がない）、ほかの機器への展開（[porting.md](drafts/2026-10-02/porting.md)）。

## 3. 進め方

- **作業の分け方**：コードを書くことと実機の試験は、利用者の PC のローカルのセッション（Claude デスクトップアプリ）でもよい。ビルドはクラウドのセッションか GitHub Actions でする。利用者の PC には ESP-IDF を入れない。
- **GitHub Actions**：`build.yml` が PR ごとにビルドし、`.bin` と `BUILD-INFO.json` を 14 日間残す（Artifact `orbit-hid-remapper-firmware`）。ローカルのセッションは `gh run download <run id> -R techmech-keeb/orbit-remapper -n orbit-hid-remapper-firmware` で取れる。PR のビルドは、PR を `main` に合わせた状態（merge commit）で作られるので、起動の行の版は PR の先頭のコミットと違うことがある。
- **クラウドでのビルド**：ESP-IDF v5.5.5 は、新しいセッションには入っていないことがある。無ければ `git clone --depth 1 --branch v5.5.5 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git /root/esp/esp-idf-v5.5.5` のあと `./install.sh esp32s3`（数分かかる）。Docker の本体は動かなかった。ビルドは `IDF_PATH=/root/esp/esp-idf-v5.5.5 dev/tools/build/build-firmware.sh <ビルドの場所>`。
- **利用者に渡す `.bin` は、ビルドの場所を消してから作る。** 前のビルドの場所を使い回すと、起動の行の版が古いまま残る（2026-10-02、`d98757d-dirty` と出た）。渡すときは、ファイル名・SHA-256・起動の行（`app=<版>`）を一緒に伝え、ローカルのセッションに書き込む前に照合させる。
- **試験の流れ**：作る → 利用者に渡す（**`.bin` はコミットしない**）→ 手順書を書く（`battery.md` のように、作業の文書に節を足す）→ 利用者がローカルのセッションに試験を頼む（貼り付けて使える指示文を渡す）→ ローカルのセッションが報告を `dev/drafts/2026-09-27/reports/` に書き、利用者の OK のあとで作業用ブランチに push する → 報告を読んで次へ。クラウドとローカルのセッションは直接やり取りできない。利用者が仲立ちする。
- **試験用の命令**：PR #5 から、ログのポートで `orbit battery <ポート> <%>` を打つと、その機器の残量を、次に本当の値を読むまで指定の値にできる（色と知らせの確認用）。
- **版**：試験は「M2 ＋画面」の 1 本の版で、画面は 180° の向き。
- **遅れの基準（LAT）**：3 ms を超える秒が 1 分に 1 回以下、かつ 5 ms 以内。新しい機器をペアリングするときの探索の 1〜2 秒は除く。
- **PR**：1 テーマ 1 PR。各コミットを単独でビルドしてから出す。マージは、利用者の明示の指示があったときだけ、CI が成功していることを確かめてからする（マージコミットの形）。作業用ブランチは、電池が `claude/laughing-volta-r6lpn4`（PR #5）、文書などそのほかが `claude/charming-mendel-x8qek6`。後者はマージのたびに `main` から切り直す。開いている PR とテーマが違うときは、利用者に断ってから別のブランチを使う。
- **リリース**：`version.txt` と `CHANGELOG.md` を同じ PR で上げる → マージ → Actions の「Release firmware」を試走（`dry_run: true`）→ ノートと添付を確かめる → 本番。タグの push はこの環境から通らないので、ワークフローを手で実行する（[dev/README.md](README.md)「版とリリース」）。設定ツールとのやりとりの形（`PROTOCOL_VERSION`）が変わったら CHANGELOG に書く。
- **コミットの作者**：`techmech <88352328+techmech-keeb@users.noreply.github.com>`。

## 4. 守ること

- 本家のコア（`firmware/hid-remapper/`）を改造しない。やむを得ないときは `// ORBIT:` の印。BTstack 由来のコードを持ち込まない。他人のコードはライセンスを確かめ、出典を書く。
- ログに、機器の完全なアドレス、ペアリングの鍵、M5Dial の MAC アドレスを出さない（アドレスは下位 2 バイトだけ）。
- `.bin`、ビルドの生成物（`build/`、`sdkconfig`、`managed_components/`）、ログの全文をリポジトリに入れない。
- 報告と文書に、PC の名前、ユーザー名、`C:\Users\...` のパス、個人名・社名を書かない。
- 試験機の 1 台は「機器 B」と書く。製品名・機器の名前・製造者・型番は書かない。
- `main` に直接 push しない。ほかの人が使うブランチの履歴を書き換えない。
- PR・Issue の番号は、リポジトリ名を付けて書く（このリポジトリの PR と、公開前の旧リポジトリの PR は番号が重なる）。
- 同じアドレスの 2 台は規格外として扱わない。
- 写真を公開するときは、EXIF・GPS と写り込みを確かめる。
- 確かめていない機種を「対応」と書かない。M5Dial v1.1 は、実機で試すまで「動作・技適とも未確認」と書く。M5Dial 以外の機器に今の `.bin` を書き込ませない。

## 5. 記録の場所

| 何 | どこ |
| --- | --- |
| 開発者向けの入口 | [dev/README.md](README.md) |
| 要件と決定、未解決事項、経緯 | [requirements.md](drafts/2026-09-26/requirements.md)、[open-questions.md](drafts/2026-09-26/open-questions.md)、[history.md](drafts/2026-09-26/history.md)（経緯の表は行 58 まで） |
| 段階ごとの依頼書と結果 | dev/README.md の「記録の読み方」の表 |
| 画面（M3） | [m3-screen.md](drafts/2026-10-01/m3-screen.md) |
| 電池の残量 | `dev/drafts/2026-10-02/battery.md`（PR #5 のブランチ。マージ後は `main`） |
| ほかの機器への展開 | [porting.md](drafts/2026-10-02/porting.md)（M5Dial v1.1、Waveshare のダイヤル機を含む） |
| 試験の報告 | `dev/drafts/2026-09-27/reports/`（電池は `bat0-*`・`bat1-*`、PR #5 のブランチ） |
| 使う人向けの文書 | [docs/](../docs/README.md) |

## 6. リポジトリの外にあるもの

- **旧リポジトリ** `techmech-keeb/orbit-remapper-dev`（private）：公開前の PR #1〜#40、試験用のブランチ、Issue #3 が残っている。消さない。`dev/drafts/` に出てくる「PR #16」などは、この旧リポジトリの番号。
- **E1 の生成物と非公開の原本**：旧リポジトリの PR #1 の参照にある。
- **ローカルのセッション**：利用者の PC で、書き込みと試験とログの採取をする。コードを書く作業もここでしてよい（§3）。

<!-- from ai-agent-playbook@72f3d44 (2026-10-22): tools/claude-code/CLAUDE.md 雛形（ポインタと重大な注意だけ） -->
<!-- 確認環境: Claude Code クラウド版（パターンA）/ 2026-10-22 時点。ローカルのセッション・Codex は未確認 -->

# CLAUDE.md (orbit-remapper)

M5Dial（ESP32-S3）1 台で動く卓上リマッパー。**公開リポジトリ**（ソフト MIT / ハード CERN-OHL-P-2.0 /
文書 CC BY 4.0）。新しいセッションは、まず `dev/HANDOFF.md`（今の状態・次にやること・進め方・守ること）を
読む。開発者向けの入口は `dev/README.md`、使う人向けの文書は `docs/`。

## 守ること（詳細は `dev/HANDOFF.md` §3〜§4）

- 日本語で書く。平易な言葉で、造語を避ける（既存の文書の文体に合わせる）。
- 本家 HID Remapper のコア（`firmware/hid-remapper/`、git subtree）を改造しない。やむを得ないときは
  `// ORBIT:` の印を付ける。他人のコードはライセンスを確かめ、出典を書く。
- `.bin`、ビルドの生成物（`build/`、`sdkconfig`、`managed_components/`）、ログの全文をコミットしない。
- 公開リポジトリなので、PC 名・ユーザー名・`C:\Users\...` のパス・個人名・社名、機器の完全なアドレス、
  ペアリングの鍵を文書・報告・ログに書かない。
- 実機で確かめていないことを「動作確認済み」と書かない。試験は利用者がローカルで行い、報告は
  `dev/drafts/<日付>/reports/` に置く。クラウドとローカルのセッションは直接やり取りできない。
- PR・Issue の番号はリポジトリ名を付けて書く（公開前の旧リポジトリと番号が重なる）。
- `main` に直接 push しない。履歴を書き換えない。マージは利用者が決める。

## ビルド

- ESP-IDF v5.5.5。`firmware/orbit/` で `idf.py build merge-bin`。CI と同じ名前付きの `.bin` と
  `BUILD-INFO.json` は、リポジトリのルートで `dev/tools/build/build-firmware.sh`
  （`firmware/orbit/README.md` §2、`dev/README.md`「ビルド」）。
- 各コミットを単独でビルドしてから PR にする。

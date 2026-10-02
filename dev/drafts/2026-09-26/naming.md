---
status: draft
snapshot_date: 2026-09-26
finalized: false
---

# 名称検討の記録

目的は本家HID Remapperへの敬意と、独自製品としての差別化を両立することです。

| 候補 | 検討理由・懸念 |
| --- | --- |
| Orbit Remapper | Orbital Podと円形UI、入力の中心というイメージ。独立製品名として推奨した。 |
| Remapper Orbit | 本家との連続性を先に伝えるが、公式シリーズのモデル名に見える可能性。 |
| Arc Remapper | 曲面と接続を表現。固有の物語はOrbitより弱いという評価。 |
| Luma Remapper | 表示による可視化を表現。照明製品にも見えうる。 |
| HID Orbit | 入力機器全般へ拡張しやすいが、リマップ機能と本家との関係が弱くなる。 |

上記は候補比較であり、Orbit以外の名称重複まで調査済みではありません。

## 2026-09-26の公開名称調査

`Orbit Remapper`、`orbit-remapper`、`OrbitRemapper`、`Remapper Orbit`、`remapper-orbit`、日本語表記について、公開Web検索で完全一致の既存製品・プロジェクトは確認できませんでした。これは不在の保証ではありません。今回作られたユーザーのリポジトリより前の調査結果として記録します。

近接分野で確認した例:

- [Orbit firmware](https://github.com/orbit-firmware/orbit): Rust製のキーボードファームウェア。分野の近さが最も気になる例。
- [Orbit Keyboard](https://github.com/ai03-2725/Orbit): 分割エルゴノミクスキーボード。
- [HackMan3D Orbit Controller](https://github.com/HackMan3D/HackMan3D-Orbit-Controller): 3Dプリント可能なUSB入力コントローラー。
- [Kensington Orbit](https://www.kensington.com/p/products/electronic-control-solutions/trackball-products/orbit-wireless-trackball-with-scroll-ring/): Orbit®表記のトラックボール。

調査前は筐体へORBIT単独を推奨しましたが、調査後は見直し、採用するならOrbit Remapperを一体で表記する案へ修正しました。Remapper Orbitに語順を変えても近い名称との関係は大きく変わりません。

## 現在の状態

ユーザーが`orbit-remapper`リポジトリを作成したため、資料のプロジェクト名として使用します。正式ブランド、ロゴ、商標の確認・確定を済ませた記録はありません。

本家へのクレジット案: `Built on HID Remapper by jfedor2.`
本家の公式製品・公認派生であると表現しません。

調査範囲は公開WebとGitHub公開情報。非公開・未収載の案件、商標登録の有無や使用可否の判断は含みません。

## 名前に「M5Stack」を入れない（2026-09-26）

M5Dial を使う構成になったため「M5Stack Remapper」への改名を検討したが、入れない方針とした（会話での提案。ユーザーの確定は未記録）。

- M5Stack は社名・ブランド名で、公式製品や公認の派生品に見える。本家 HID Remapper に対して「公式と誤解されない表記」とした方針と同じ考え方。
- M5Stack 社が他者による名前の使用について指針を公開しているかは確認できなかった。
- 機材が変われば名前が実態と合わなくなる。
- 対応機種は「Orbit Remapper for M5Stack Dial」のように説明で示し、「M5Stack 社とは無関係」と添える。
- 「Orbit」は E1 の Orbital Pod に由来するが、丸い画面の周りを回る M5Dial の見た目にも合う。商標の確認（Q18）は引き続き必要。

名前の調査中に見つけた、M5Stack で BLE キーボードのキー割り当てを変える先行例の分析は [prior-art.md](prior-art.md)。


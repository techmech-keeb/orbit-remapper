# 開発者向けガイド

Orbit Remapperの公開リポジトリは、ファームウェアの実装、再現可能なビルド、配布、試験用プログラム、利用者向け文書を管理します。利用案内は [docs/](../docs/README.md) を参照してください。

## 公開ソースと再現可能な成果物

| 場所 | 内容 |
| --- | --- |
| [`firmware/orbit/`](../firmware/orbit/README.md) | ESP-IDFファームウェア本体、ハードウェア依存部分、設定、ビルド手順 |
| [`firmware/hid-remapper/`](../firmware/hid-remapper/UPSTREAM.md) | 本家HID Remapperのgit subtree、取り込み元とライセンス |
| [`dev/tools/build/`](tools/build/build-firmware.sh) | CI/リリースと共通のビルド、第三者ライセンス一覧 |
| [`dev/tools/serial-capture/`](tools/serial-capture/README.md) | Windowsでログを取得するツール |
| [`dev/experiments/q31-s3-two-ble/`](experiments/q31-s3-two-ble/README.md) | BLE 2台・7.5 ms接続の独立検証プログラム |
| [`docs/`](../docs/README.md) | ユーザー向けの導入・利用・リマップ設定・トラブル対応 |

## ビルド

ESP-IDF **v5.5.5**を利用し、`firmware/orbit/`で次を実行します。

```sh
idf.py build merge-bin
```

CIと同じ生成物名・来歴を確認するには、リポジトリのルートで：

```sh
dev/tools/build/build-firmware.sh
```

書き込みは [初回導入ガイド](../docs/getting-started.md) と [ファームウェアの説明](../firmware/orbit/README.md) を参照してください。生成された`.bin`、ビルド中間生成物、取得したログ全文はコミットしません。

## CI・リリース

- [build.yml](../.github/workflows/build.yml) はPRとmainでファームウェアをビルドし、生成物を保存します。
- [release.yml](../.github/workflows/release.yml) は手動実行で版・変更履歴・SBOM・第三者通知・SHA-256・来歴を検証してリリースします。
- 版番号の正本は `firmware/orbit/version.txt`、変更履歴は [CHANGELOG.md](../CHANGELOG.md)、公開リリースは [Releases](https://github.com/techmech-keeb/orbit-remapper/releases)。
- リリースの試行は `dry_run: true` とし、生成された資料と添付を確認してから本番を実行してください。
- 本家コアへの変更は原則として避け、必要なら根拠と変更箇所を明記してください。

## 安全性と品質

- 公開前にビルド、プロトコル互換性、ライセンス、個人情報混入を点検します。
- MAC/BLEアドレス全文、鍵、個人を特定するPCパスを公開しないでください。
- 動作確認済みと机上互換性評価は区別してください。
- コードの変更はテーマごとのPRとし、mainへの直接pushは避けてください。

# [Remote Smart Alarm]

## 📂 ディレクトリ構成

```text
3rd_Seisaku/
├── index.js              # サーバーのエントリーポイント
├── package.json          # 依存ライブラリ・プロジェクト管理ファイル
├── .env                  # 環境変数設定ファイル
├── public/               # ブラウザから直接アクセス可能な静的ファイル
│   ├── css/              # スタイルシート
│   ├── js/               # クライアントサイドJavaScript
│   └── images/           # 画像リソース
├── views/                # 画面表示用テンプレートファイル（EJS等）
├── routes/               # ルーティング（パスごとの処理分割）
└── models/               # データベース操作・データ構造定義

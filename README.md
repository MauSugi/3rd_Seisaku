# Remote Smart Alarm
3E制作実習「遠隔で止める目覚まし時計」\n
alarm unit と button unit の二つに分かれています。

### 使用した部品
・ESP32-DevKitC-VE * 2\n
・DFPlayer Mini\n
・Speaker\n
・switch\n
・7seg 4digit display\n
etc...\n

### ディレクトリ構成
```text
3rd_Seisaku/
├── 3rd_Seisaku_hardware/        # ハードウェア関連
│   ├── 3rd_Seisaku_3Ddata       # GitHub上でプレビュー可能なstlファイル
│   ├── 3rd_Seisaku_Circuit      # KiCadで作成した回路図及びpcbデータ(両unit共通)
│   └── 3rd_Seisaku_Enclosure    # Fusionで作成した筐体のcadデータ
└── 3rd_Seisaku_software/        # ソフトウェア関連
    ├── 3rd_Seisaku_alarm        # alarm unitのプログラム
    └── 3rd_Seisaku_button       # button unitのプログラム

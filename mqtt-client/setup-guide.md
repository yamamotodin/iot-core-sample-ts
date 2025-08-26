# Eclipse Paho C MQTT Client セットアップガイド

AWS IoT CoreでEclipse Paho C Client Libraryを使用するための完全なセットアップガイドです。

## 📋 前提条件

- Linux/macOS環境
- GCC コンパイラ
- CMake 3.5以上
- OpenSSL開発ライブラリ
- JSON-C開発ライブラリ

## 🚀 クイックスタート

### 1. プロジェクトのセットアップ

```bash
# プロジェクトディレクトリ作成
mkdir aws-iot-paho-c
cd aws-iot-paho-c

# ファイルをダウンロード・配置
# - paho_c_mqtt_client.c
# - Makefile

# 証明書ディレクトリ作成
make setup-certs
```

### 2. 依存関係のインストール

#### Ubuntu/Debian の場合
```bash
make install-deps-ubuntu
```

#### CentOS/RHEL/Fedora の場合
```bash
make install-deps-rhel
make build-paho
```

#### macOS の場合
```bash
brew install json-c
make install-deps-macos
make build-paho
```

### 3. 証明書の配置

```bash
# Amazon Root CA証明書をダウンロード
make download-root-ca

# AWS IoTコンソールから以下をダウンロードして配置:
# certificates/certificate.pem.crt - デバイス証明書
# certificates/private.pem.key     - 秘密鍵
```

### 4. 設定の更新

`paho_c_mqtt_client.c` ファイル内の以下を更新：

```c
#define ADDRESS     "ssl://YOUR_ACCOUNT_ID.iot.ap-northeast-1.amazonaws.com:8883"
#define CLIENTID    "custom-device-001"
#define THING_NAME  "custom-device-001"
```

### 5. ビルドと実行

```bash
# 設定確認
make check-config

# ビルド
make

# 実行
make run
```

---

## 🔧 詳細セットアップ

### 手動でのPaho MQTT Cライブラリインストール

システムの依存関係でPaho MQTTライブラリが提供されていない場合：

```bash
# ソースコードをクローン
git clone https://github.com/eclipse-paho/paho.mqtt.c.git
cd paho.mqtt.c

# ビルドディレクトリ作成
mkdir build
cd build

# CMake設定（SSL有効）
cmake .. \
  -DPAHO_ENABLE_TESTING=OFF \
  -DPAHO_BUILD_STATIC=OFF \
  -DPAHO_WITH_SSL=ON \
  -DPAHO_HIGH_PERFORMANCE=ON

# ビルド
make -j$(nproc)

# インストール (root権限必要)
sudo make install

# ライブラリパス更新
sudo ldconfig
```

### 証明書の詳細な取得方法

#### AWS CLIを使用した証明書取得

```bash
# Certificate ID を環境変数に設定 (CDK出力から取得)
export CERT_ID="your-certificate-id-here"

# 証明書本体を取得
aws iot describe-certificate \
  --certificate-id $CERT_ID \
  --query 'certificateDescription.certificatePem' \
  --output text > certificates/certificate.pem.crt

# 注意: 秘密鍵は証明書作成時にのみ取得可能
# 独自CSRを使用した場合は、作成時の秘密鍵を使用
```

#### AWS コンソールから取得

1. AWS コンソール → IoT Core
2. セキュリティ → 証明書
3. 対象の証明書を選択
4. 「ダウンロード」ボタンから各ファイルを取得

---

## 🏗️ ビルドオプション

### デバッグビルド

```bash
make debug
gdb ./iot_mqtt_client
```

### 静的リンクビルド

```bash
# Makefileを編集して静的リンクを追加
LDFLAGS = -lpaho-mqtt3cs -ljson-c -lssl -lcrypto -static
```

### クロスコンパイル（ARM用）

```bash
# ARM用のクロスコンパイラを使用
CC=arm-linux-gnueabihf-gcc make
```

---

## 📊 プログラムの機能

### 実装済み機能

1. **MQTT接続**
    - SSL/TLS暗号化
    - X.509証明書認証
    - 自動再接続

2. **メッセージング**
    - センサーデータの定期送信 (10秒間隔)
    - デバイスステータス送信
    - コマンド受信と処理

3. **JSON処理**
    - センサーデータのJSON形式変換
    - コマンドメッセージのJSON解析

4. **エラーハンドリング**
    - 接続エラーの検出と報告
    - メッセージ配信確認
    - 適切なリソース解放

### サポートするトピック

- **送信**
    - `device/{THING_NAME}/data` - センサーデータ
    - `device/{THING_NAME}/status` - デバイスステータス

- **受信**
    - `device/{THING_NAME}/commands` - コマンド受信

### サポートするコマンド

```json
// Ping応答
{"action": "ping"}

// リスタート
{"action": "restart"}

// ステータス取得
{"action": "get_status"}

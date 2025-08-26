# Makefile for Eclipse Paho C MQTT Client

BREW_PREFIX=/opt/homebrew

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -I$(BREW_PREFIX)/include
#LDFLAGS =-ljson-c -lssl -lcrypto
LDFLAGS = -L$(BREW_PREFIX)/lib -L/usr/local/lib -lpaho-mqtt3cs -ljson-c
# -lpaho-mqtt3cs
# /opt/homebrew/Cellar/json-c/0.18/lib/libjson-c.a

# ターゲット
TARGET = iot_mqtt_client
SOURCES = paho_c_mqtt_client.c
OBJECTS = $(SOURCES:.c=.o)

# デフォルトターゲット
all: $(TARGET)

# 実行ファイル作成
$(TARGET): $(OBJECTS)
	@echo "🔨 実行ファイル作成中..."
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "✅ ビルド完了: $(TARGET)"

# オブジェクトファイル作成
%.o: %.c
	@echo "🔧 コンパイル中: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# インストール (Ubuntu/Debian用)
install-deps-ubuntu:
	@echo "📦 依存関係をインストール中 (Ubuntu/Debian)..."
	sudo apt update
	sudo apt install -y build-essential cmake git
	sudo apt install -y libssl-dev libpaho-mqtt-dev libjson-c-dev
	@echo "✅ 依存関係のインストール完了"

# インストール (CentOS/RHEL/Fedora用)
install-deps-rhel:
	@echo "📦 依存関係をインストール中 (CentOS/RHEL/Fedora)..."
	sudo yum groupinstall -y "Development Tools"
	sudo yum install -y cmake git openssl-devel json-c-devel
	@echo "⚠️  Paho MQTTライブラリを手動でビルドしてください"
	@echo "   詳細は README を参照"

# インストール (macOS用)
install-deps-macos:
	@echo "📦 依存関係をインストール中 (macOS)..."
	@if ! command -v brew >/dev/null 2>&1; then \
		echo "❌ Homebrew が必要です"; \
		echo "   /bin/bash -c \"\$$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""; \
		exit 1; \
	fi
	brew install cmake git openssl json-c libpaho-mqtt
	@echo "⚠️  Paho MQTTライブラリを手動でビルドしてください"
	@echo "   詳細は README を参照"

# Paho MQTT Cライブラリのビルド（手動インストール用）
build-paho:
	@echo "🚀 Paho MQTT C ライブラリをビルド中..."
	@if [ ! -d "paho.mqtt.c" ]; then \
		git clone https://github.com/eclipse-paho/paho.mqtt.c.git; \
	fi
	cd paho.mqtt.c && \
	cmake -Bbuild -H. -DPAHO_ENABLE_TESTING=OFF -DPAHO_BUILD_STATIC=OFF \
	-DPAHO_WITH_SSL=ON -DPAHO_HIGH_PERFORMANCE=ON && \
	cmake --build build/ --target install
	@echo "✅ Paho MQTT ライブラリのビルド完了"

# 実行
run: $(TARGET)
	@echo "🚀 MQTT クライアント実行中..."
	@echo "⚠️  証明書ファイルと設定を確認してください"
	./$(TARGET)

# 証明書ディレクトリ作成
setup-certs:
	@echo "📁 証明書ディレクトリを作成中..."
	mkdir -p certificates
	@echo "✅ certificates/ ディレクトリを作成しました"
	@echo ""
	@echo "📝 必要なファイル:"
	@echo "  certificates/certificate.pem.crt  - デバイス証明書"
	@echo "  certificates/private.pem.key      - 秘密鍵"
	@echo "  certificates/AmazonRootCA1.pem    - ルート証明書"
	@echo ""
	@echo "🌐 AmazonRootCA1.pem のダウンロード:"
	@echo "  curl -o certificates/AmazonRootCA1.pem https://www.amazontrust.com/repository/AmazonRootCA1.pem"

# ルート証明書のダウンロード
download-root-ca:
	@echo "📥 Amazon Root CA 証明書をダウンロード中..."
	curl -o certificates/AmazonRootCA1.pem https://www.amazontrust.com/repository/AmazonRootCA1.pem
	@echo "✅ AmazonRootCA1.pem をダウンロードしました"

# 設定確認
check-config:
	@echo "🔍 設定と証明書を確認中..."
	@echo ""
	@echo "📂 証明書ファイル:"
	@for file in certificates/certificate.pem.crt certificates/private.pem.key certificates/AmazonRootCA1.pem; do \
		if [ -f "$$file" ]; then \
			echo "  ✅ $$file"; \
		else \
			echo "  ❌ $$file (見つかりません)"; \
		fi \
	done
	@echo ""
	@echo "⚙️  設定確認:"
	@echo "  エンドポイント: YOUR_ACCOUNT_ID.iot.ap-northeast-1.amazonaws.com を実際の値に変更してください"
	@echo "  Thing名: custom-device-001"
	@echo "  クライアントID: custom-device-001"

# クリーンアップ
clean:
	@echo "🧹 クリーンアップ中..."
	rm -f $(OBJECTS) $(TARGET)
	@echo "✅ クリーンアップ完了"

# 完全クリーンアップ
clean-all: clean
	@echo "🗑️  完全クリーンアップ中..."
	rm -rf paho.mqtt.c
	@echo "✅ 完全クリーンアップ完了"

# デバッグビルド
debug: CFLAGS += -DDEBUG -g
debug: $(TARGET)

# テスト用のモック実行
test-mock:
	@echo "🧪 モックテスト実行..."
	@echo "   実際のAWS IoTに接続せずにテスト"
	$(CC) $(CFLAGS) -DMOCK_TEST -o $(TARGET)_mock $(SOURCES) $(LDFLAGS)
	./$(TARGET)_mock

# ヘルプ
help:
	@echo "🔧 使用可能なコマンド:"
	@echo ""
	@echo "  make                    - プログラムをビルド"
	@echo "  make install-deps-ubuntu - Ubuntu/Debian用依存関係インストール"
	@echo "  make install-deps-rhel  - CentOS/RHEL/Fedora用依存関係インストール"
	@echo "  make install-deps-macos - macOS用依存関係インストール"
	@echo "  make build-paho         - Paho MQTT Cライブラリを手動ビルド"
	@echo "  make setup-certs        - 証明書ディレクトリを作成"
	@echo "  make download-root-ca   - Amazon Root CA証明書をダウンロード"
	@echo "  make check-config       - 設定と証明書を確認"
	@echo "  make run                - プログラムを実行"
	@echo "  make clean              - ビルド成果物をクリーンアップ"
	@echo "  make clean-all          - 完全クリーンアップ"
	@echo "  make debug              - デバッグ版をビルド"
	@echo "  make help               - このヘルプを表示"

.PHONY: all clean clean-all install-deps-ubuntu install-deps-rhel install-deps-macos build-paho setup-certs download-root-ca check-config run debug test-mock help

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <json-c/json.h>
#include "MQTTClient.h"

// 設定定数
#define ADDRESS     "ssl://aedec8i40hibd-ats.iot.ap-northeast-1.amazonaws.com:8883"
#define CLIENT_ID    "iotconsole-78eb4736-8f73-4325-8e2c-6bb304b1b177"
#define THING_NAME  "sensor-device-001"
#define QOS         1
#define TIMEOUT     10000L

// 証明書ファイルのパス
#define CERT_FILE   "./certificates/certificate.pem.crt"
#define KEY_FILE    "./certificates/private.pem.key"
#define CA_FILE     "./certificates/AmazonRootCA1.pem"

// トピック定義
#define TOPIC_DATA     "device/" THING_NAME "/data"
#define TOPIC_STATUS   "device/" THING_NAME "/status"
#define TOPIC_COMMANDS "device/" THING_NAME "/commands"

// グローバル変数
MQTTClient client;
volatile int finished = 0;
volatile int connected = 0;

// 関数プロトタイプ
void signal_handler(int sig);
int messageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message);
void connectionLost(void *context, char *cause);
void deliveryComplete(void *context, MQTTClient_deliveryToken token);
int connect_mqtt_client(void);
int publish_sensor_data(void);
int publish_status(const char *status);
void handle_command(const char *command_json);
char* create_sensor_data_json(void);
char* create_status_json(const char *status);
double get_random_double(double min, double max);

// シグナルハンドラー
void signal_handler(int sig) {
    printf("\n📡 終了処理中...\n");
    if (connected) {
        publish_status("offline");
        usleep(500000); // 0.5秒待機
    }
    finished = 1;
}

// メッセージ受信コールバック
int messageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    printf("📨 メッセージ受信: %s\n", topicName);
    printf("   内容: %.*s\n", message->payloadlen, (char*)message->payload);

    // コマンドトピックの処理
    if (strstr(topicName, "/commands") != NULL) {
        char *payload = malloc(message->payloadlen + 1);
        memcpy(payload, message->payload, message->payloadlen);
        payload[message->payloadlen] = '\0';

        handle_command(payload);
        free(payload);
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

// 接続切断コールバック
void connectionLost(void *context, char *cause) {
    printf("🔌 接続が切断されました: %s\n", cause ? cause : "不明な理由");
    connected = 0;
}

// 配信完了コールバック
void deliveryComplete(void *context, MQTTClient_deliveryToken token) {
    printf("📤 メッセージ配信完了 (token: %d)\n", token);
}

// MQTT接続
int connect_mqtt_client(void) {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
    int rc;

    printf("🚀 AWS IoT Core への接続を開始...\n");

    // クライアント作成
    if ((rc = MQTTClient_create(&client, ADDRESS, CLIENT_ID,
            MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("❌ クライアント作成失敗, return code %d\n", rc);
        return rc;
    }

    // コールバック設定
    if ((rc = MQTTClient_setCallbacks(client, NULL, connectionLost,
            messageArrived, deliveryComplete)) != MQTTCLIENT_SUCCESS) {
        printf("❌ コールバック設定失敗, return code %d\n", rc);
        return rc;
    }

    // SSL/TLS設定
    ssl_opts.trustStore = CA_FILE;
    ssl_opts.keyStore = CERT_FILE;
    ssl_opts.privateKey = KEY_FILE;
    ssl_opts.enableServerCertAuth = 0;

    // 接続オプション設定
    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;
    conn_opts.ssl = &ssl_opts;

    // 接続実行
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("❌ 接続失敗, return code %d, errno=%d\n", rc, errno);
        return rc;
    }

    printf("✅ AWS IoT Core に接続しました!\n");
    printf("📡 エンドポイント: %s\n", ADDRESS);
    printf("🏷️  クライアントID: %s\n", CLIENT_ID);

    connected = 1;

    // コマンドトピックを購読
    if ((rc = MQTTClient_subscribe(client, TOPIC_COMMANDS, QOS)) != MQTTCLIENT_SUCCESS) {
        printf("❌ 購読失敗: %s, return code %d\n", TOPIC_COMMANDS, rc);
        return rc;
    }

    printf("🎯 トピック購読: %s\n", TOPIC_COMMANDS);

    // 接続通知
    publish_status("online");

    return MQTTCLIENT_SUCCESS;
}

// センサーデータの送信
int publish_sensor_data(void) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;

    char *payload = create_sensor_data_json();
    if (!payload) {
        return MQTTCLIENT_FAILURE;
    }

    pubmsg.payload = payload;
    pubmsg.payloadlen = strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    if ((rc = MQTTClient_publishMessage(client, TOPIC_DATA, &pubmsg, &token)) != MQTTCLIENT_SUCCESS) {
        printf("❌ センサーデータ送信失敗, return code %d\n", rc);
        free(payload);
        return rc;
    }

    printf("📊 センサーデータ送信: %s\n", TOPIC_DATA);
    printf("   %s\n", payload);

    // 配信確認を待機
    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    free(payload);

    return rc;
}

// ステータスの送信
int publish_status(const char *status) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;

    char *payload = create_status_json(status);
    if (!payload) {
        return MQTTCLIENT_FAILURE;
    }

    pubmsg.payload = payload;
    pubmsg.payloadlen = strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    if ((rc = MQTTClient_publishMessage(client, TOPIC_STATUS, &pubmsg, &token)) != MQTTCLIENT_SUCCESS) {
        printf("❌ ステータス送信失敗, return code %d\n", rc);
        free(payload);
        return rc;
    }

    printf("📈 ステータス送信: %s (%s)\n", status, TOPIC_STATUS);

    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    free(payload);

    return rc;
}

// コマンド処理
void handle_command(const char *command_json) {
    json_object *root, *action_obj;
    const char *action;

    root = json_tokener_parse(command_json);
    if (!root) {
        printf("❌ 無効なJSON: %s\n", command_json);
        return;
    }

    if (json_object_object_get_ex(root, "action", &action_obj)) {
        action = json_object_get_string(action_obj);
        printf("⚙️  コマンド処理: %s\n", action);

        if (strcmp(action, "ping") == 0) {
            // ping応答
            MQTTClient_message pubmsg = MQTTClient_message_initializer;
            MQTTClient_deliveryToken token;

            char response[] = "{\"type\":\"pong\",\"message\":\"Device is alive!\"}";
            pubmsg.payload = response;
            pubmsg.payloadlen = strlen(response);
            pubmsg.qos = QOS;
            pubmsg.retained = 0;

            MQTTClient_publishMessage(client, TOPIC_DATA, &pubmsg, &token);
            printf("🏓 Pong応答を送信\n");

        } else if (strcmp(action, "restart") == 0) {
            printf("🔄 リスタートコマンド受信\n");
            publish_status("restarting");

        } else if (strcmp(action, "get_status") == 0) {
            printf("📊 ステータス要求受信\n");
            publish_status("online");

        } else {
            printf("❓ 不明なコマンド: %s\n", action);
        }
    }

    json_object_put(root);
}

// センサーデータJSONの作成
char* create_sensor_data_json(void) {
    json_object *root = json_object_new_object();
    json_object *device_id = json_object_new_string(THING_NAME);
    json_object *temperature = json_object_new_double(get_random_double(10.0, 40.0));
    json_object *humidity = json_object_new_double(get_random_double(30.0, 70.0));
    json_object *battery = json_object_new_int(rand() % 81 + 20); // 20-100
    json_object *timestamp = json_object_new_int64(time(NULL) * 1000);

    json_object_object_add(root, "deviceId", device_id);
    json_object_object_add(root, "temperature", temperature);
    json_object_object_add(root, "humidity", humidity);
    json_object_object_add(root, "battery", battery);
    json_object_object_add(root, "timestamp", timestamp);

    const char *json_string = json_object_to_json_string(root);
    char *result = strdup(json_string);

    json_object_put(root);
    return result;
}

// ステータスJSONの作成
char* create_status_json(const char *status) {
    json_object *root = json_object_new_object();
    json_object *device_id = json_object_new_string(THING_NAME);
    json_object *status_obj = json_object_new_string(status);
    json_object *version = json_object_new_string("1.0.0");
    json_object *timestamp = json_object_new_int64(time(NULL) * 1000);

    json_object_object_add(root, "deviceId", device_id);
    json_object_object_add(root, "status", status_obj);
    json_object_object_add(root, "version", version);
    json_object_object_add(root, "timestamp", timestamp);

    const char *json_string = json_object_to_json_string(root);
    char *result = strdup(json_string);

    json_object_put(root);
    return result;
}

// ランダムな小数点数を生成
double get_random_double(double min, double max) {
    return min + (max - min) * ((double)rand() / RAND_MAX);
}

// メイン関数
int main(int argc, char* argv[]) {
    int rc;
    errno = 0;
    printf("🔧 Eclipse Paho C MQTT クライアント for AWS IoT Core\n");
    printf("================================================\n");

    // シグナルハンドラー設定
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // 乱数シード初期化
    srand(time(NULL));

    // MQTT接続
    if ((rc = connect_mqtt_client()) != MQTTCLIENT_SUCCESS) {
        printf("❌ 接続失敗 (code: %d, errno: %d)\n", rc, errno);
        exit(EXIT_FAILURE);
    }

    printf("\n🌡️  センサーデータの送信を開始...\n");
    printf("Ctrl+C で終了\n\n");

    // メインループ
    time_t last_sensor_time = 0;
    while (!finished) {
        time_t current_time = time(NULL);

        // 10秒間隔でセンサーデータ送信
        if (connected && (current_time - last_sensor_time >= 10)) {
            if (publish_sensor_data() == MQTTCLIENT_SUCCESS) {
                last_sensor_time = current_time;
            }
        }

        // 1秒待機
        usleep(1000000);
    }

    // クリーンアップ
    if (connected) {
        MQTTClient_unsubscribe(client, TOPIC_COMMANDS);
        MQTTClient_disconnect(client, 10000);
    }
    MQTTClient_destroy(&client);

    printf("👋 プログラム終了\n");
    return rc;
}

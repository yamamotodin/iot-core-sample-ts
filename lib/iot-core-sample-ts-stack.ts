import * as cdk from 'aws-cdk-lib';
import * as iot from 'aws-cdk-lib/aws-iot';
import * as iam from 'aws-cdk-lib/aws-iam';
import * as lambda from 'aws-cdk-lib/aws-lambda';
import * as logs from 'aws-cdk-lib/aws-logs';
import { Construct } from 'constructs';

export class IotCoreSampleTsStack extends cdk.Stack {
  constructor(scope: Construct, id: string, props?: cdk.StackProps) {
    super(scope, id, props);

    // IoT Policy - デバイスが実行できるアクションを定義
    const devicePolicy = new iot.CfnPolicy(this, 'DevicePolicy', {
      policyName: 'MyDevicePolicy',
      policyDocument: {
        Version: '2012-10-17',
        Statement: [
          {
            Effect: 'Allow',
            Action: [
              'iot:Connect'
            ],
            Resource: [
              `arn:aws:iot:${this.region}:${this.account}:client/*`
            ]
          },
          {
            Effect: 'Allow',
            Action: [
              'iot:Publish'
            ],
            Resource: [
              `arn:aws:iot:${this.region}:${this.account}:topic/device/+/data`,
              `arn:aws:iot:${this.region}:${this.account}:topic/device/+/status`
            ]
          },
          {
            Effect: 'Allow',
            Action: [
              'iot:Subscribe'
            ],
            Resource: [
              `arn:aws:iot:${this.region}:${this.account}:topicfilter/device/+/commands`,
              `arn:aws:iot:${this.region}:${this.account}:topicfilter/broadcast/*`
            ]
          },
          {
            Effect: 'Allow',
            Action: [
              'iot:Receive'
            ],
            Resource: [
              `arn:aws:iot:${this.region}:${this.account}:topic/device/+/commands`,
              `arn:aws:iot:${this.region}:${this.account}:topic/broadcast/*`
            ]
          }
        ]
      }
    });

    // Thing Type - デバイスタイプの定義
    const thingType = new iot.CfnThingType(this, 'MyThingType', {
      thingTypeName: 'SensorDevice',
      thingTypeProperties: {
        searchableAttributes: ['deviceModel', 'location']
      }
    });

    // Thing - 実際のIoTデバイス
    const thing = new iot.CfnThing(this, 'MyThing', {
      thingName: 'sensor-device-001',
      attributePayload: {
        attributes: {
          deviceModel: 'ESP32',
          location: 'factory-floor-1',
          version: '1.0'
        }
      }
    });

    // Certificate - デバイス認証用の証明書
    // const certificate = new iot.CfnCertificate(this, 'DeviceCertificate', {
    //   status: 'ACTIVE',
    //   certificateMode: 'DEFAULT'
    // });
    // オプション2: 独自CSRを使用した証明書（コメントアウト解除して使用）

    const certificate = new iot.CfnCertificate(this, 'CustomCertificate', {
      status: 'ACTIVE',
      certificateMode: 'DEFAULT',
      certificateSigningRequest: `-----BEGIN CERTIFICATE REQUEST-----
MIICXjCCAUYCAQAwGTEXMBUGA1UEAwwOTXlJb1REZXZpY2UwMTCCASIwDQYJKoZI
hvcNAQEBBQADggEPADCCAQoCggEBAL... // あなたのCSRをここに貼り付け
-----END CERTIFICATE REQUEST-----`
    });


    // Policy Attachment - 証明書にポリシーをアタッチ
    new iot.CfnPolicyPrincipalAttachment(this, 'PolicyPrincipalAttachment', {
      policyName: devicePolicy.policyName!,
      principal: certificate.attrArn
    });

    // Thing Principal Attachment - ThingにCertificateをアタッチ
    new iot.CfnThingPrincipalAttachment(this, 'ThingPrincipalAttachment', {
      thingName: thing.thingName!,
      principal: certificate.attrArn
    });

    // Lambda function - IoTメッセージを処理
    const messageProcessor = new lambda.Function(this, 'MessageProcessor', {
      runtime: lambda.Runtime.NODEJS_18_X,
      handler: 'index.handler',
      code: lambda.Code.fromInline(`
        exports.handler = async (event) => {
          console.log('Received IoT message:', JSON.stringify(event, null, 2));
          
          // メッセージの処理ロジックをここに追加
          const topic = event.topic;
          const message = event;
          
          if (topic.includes('/data')) {
            console.log('Processing sensor data:', message);
            // センサーデータの処理
          } else if (topic.includes('/status')) {
            console.log('Processing device status:', message);
            // デバイスステータスの処理
          }
          
          return {
            statusCode: 200,
            body: 'Message processed successfully'
          };
        };
      `),
      timeout: cdk.Duration.seconds(30)
    });

    // IoT Rule - 特定のトピックのメッセージをLambdaに転送
    const iotRule = new iot.CfnTopicRule(this, 'DeviceDataRule', {
      ruleName: 'ProcessDeviceMessages',
      topicRulePayload: {
        description: 'Process messages from IoT devices',
        sql: "SELECT *, topic() as topic, timestamp() as timestamp FROM 'device/+/data'",
        actions: [
          {
            lambda: {
              functionArn: messageProcessor.functionArn
            }
          }
        ],
        awsIotSqlVersion: '2016-03-23',
        ruleDisabled: false
      }
    });

    // Lambda permission - IoT CoreからLambdaを呼び出す権限
    messageProcessor.addPermission('AllowIoTInvoke', {
      principal: new iam.ServicePrincipal('iot.amazonaws.com'),
      sourceArn: iotRule.attrArn
    });

    // CloudWatch Logs for IoT Rule errors
    const logGroup = new logs.LogGroup(this, 'IoTRuleLogGroup', {
      logGroupName: '/aws/iot/rules/ProcessDeviceMessages',
      retention: logs.RetentionDays.ONE_WEEK,
      removalPolicy: cdk.RemovalPolicy.DESTROY
    });

    // Error action for IoT Rule
    const errorRule = new iot.CfnTopicRule(this, 'ErrorHandlingRule', {
      ruleName: 'HandleDeviceErrors',
      topicRulePayload: {
        description: 'Handle device message errors',
        sql: "SELECT * FROM '$aws/rules/ProcessDeviceMessages/error'",
        actions: [
          {
            cloudwatchLogs: {
              logGroupName: logGroup.logGroupName,
              roleArn: this.createLogsRole().roleArn
            }
          }
        ],
        awsIotSqlVersion: '2016-03-23',
        ruleDisabled: false
      }
    });

    // Outputs
    new cdk.CfnOutput(this, 'IoTEndpoint', {
      description: 'IoT Core endpoint for MQTT connections',
      value: `https://iot.${this.region}.amazonaws.com`
    });

    new cdk.CfnOutput(this, 'ThingName', {
      description: 'IoT Thing name',
      value: thing.thingName!
    });

    new cdk.CfnOutput(this, 'CertificateArn', {
      description: 'Device certificate ARN',
      value: certificate.attrArn
    });

    new cdk.CfnOutput(this, 'CertificateId', {
      description: 'Device certificate ID',
      value: certificate.attrId
    });
  }

  private createLogsRole(): iam.Role {
    return new iam.Role(this, 'IoTLogsRole', {
      assumedBy: new iam.ServicePrincipal('iot.amazonaws.com'),
      inlinePolicies: {
        LogsPolicy: new iam.PolicyDocument({
          statements: [
            new iam.PolicyStatement({
              effect: iam.Effect.ALLOW,
              actions: [
                'logs:CreateLogGroup',
                'logs:CreateLogStream',
                'logs:PutLogEvents',
                'logs:PutMetricFilter',
                'logs:PutRetentionPolicy'
              ],
              resources: ['*']
            })
          ]
        })
      }
    });
  }
}

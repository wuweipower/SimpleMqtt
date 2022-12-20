// 由 陈章韶 编写并做注释讲解
// ESP8266 温湿度传感器MQTT通信
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <EasyNTPClient.h>
#include <WiFiManager.h>
#include <TimeLib.h>
#include <DHT.h>
#include <DHT_U.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "SparkFun_SHTC3.h"

// 如果使用DHT系列的温湿度传感器，请取消注释这个 #define DHTPIN 并按实际硬件连接设定数据引脚
// #define DHTPIN 0 // Digital pin connected to the DHT sensor
#ifdef DHTPIN
// Uncomment the type of sensor in use:
#define DHTTYPE DHT11 // DHT 11
//#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//#define DHTTYPE    DHT21     // DHT 21 (AM2301)
DHT_Unified dht(DHTPIN, DHTTYPE);
#endif
// 如果使用SHTC3系列的温湿度传感器，请取消注释这个 #define SHTC3_ADDR 0x70 并按实际硬件连接设定I2C的SDA引脚和SCL引脚选用哪一个
#define SHTC3_ADDR 0x70
#ifdef SHTC3_ADDR
#define SHTC3_SDA 4
#define SHTC3_SCL 5
bool hasSHTC3 = false;
SHTC3 mySHTC3; // Declare an instance of the SHTC3 class
#endif

// 用于扫描是否存在SHTC3等I2C设备
void scanI2Cdevice(TwoWire wire, int sda, int scl)
{
    wire.begin(sda, scl);
    byte error, address, nDevices = 0;

    // 串口输出调试信息
    Serial.println("Scanning I2C Devices....");
    for (address = 1; address < 127; address++)
    {
        // 发送1次从机地址
        wire.beginTransmission(address);
        // 等待从机响应
        error = wire.endTransmission();
        if (error == 0)
        {
            Serial.print("I2C device found at address 0x");
            // 显示当前查询的从机地址
            if (address < 16)
            {
                Serial.print("0");
            }
            if (address == SHTC3_ADDR)
            {
                hasSHTC3 = true;
            }
            Serial.println(address, HEX);
            // 找到的从机数量自增
            nDevices++;
        }
        else if (error == 4)
        {
            Serial.print("Unknow error at address 0x");
            if (address < 16)
                Serial.print("0");
            Serial.println(address, HEX);
        }
    }
    // 所有从机地址遍历完毕
    if (nDevices == 0)
        Serial.println("No I2C devices found\n");
    else
        Serial.printf("Found %d I2C device(s)\r\n", nDevices);
    return;
}

// 这里定义一个udp客户端，并定义一个NTP客户端用于联网对时，获取东8区北京时间
WiFiUDP udp;
EasyNTPClient ntpClient(udp, "ntp1.aliyun.com", 60 * 60 * 8);

// 该函数用于联网获取时间
time_t getNowTimestamp()
{
    Serial.printf("In getNowTimeStamp SSID = %s, STA IP = %s\r\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    time_t tt = ntpClient.getUnixTime();
    Serial.printf("now getUnixTime = %ld\r\n", tt);
    return tt;
}

bool isWiFiConfigMode; // 标记是否处在WiFi配置模式
WiFiManager wm;        // 用于WiFi配置的管理器
unsigned long lastRefresh = 0;
// WiFi配置的回调函数，在这里判断联网是否成功
void configModeCallback(WiFiManager *myWiFiManager)
{
    if (millis() - lastRefresh > 500)
    {
        if (isWiFiConfigMode)
        {
            /** 如果联网没有成功，输出模块自己发射的热点的名字和密码
             * 注意，这是处在配网模式，电脑首先用下面的WiFi名字和密码连上热点
             * 然后打开浏览器访问下面输出的IP地址进行在线配网。配网完成后会自动重启模块联网
             */
            Serial.printf("SSID  %s", (myWiFiManager->getConfigPortalSSID()).c_str());
            Serial.printf("PW  %s", (myWiFiManager->getConfigPortalPassword()).c_str());
            Serial.printf("Browse  %s ", WiFi.softAPIP().toString().c_str());
        }
        else
        {
            // 若配网完成，输出模块当前连接的WiFi热点的名字和获取到的IP地址，方便之后进行调试
            // 注意，在这里请记住模块获取到的IP地址，后续TCP通信需要它
            Serial.printf("SSID %s", WiFi.SSID().c_str());
            Serial.printf("IP %s", WiFi.localIP().toString().c_str());
            // 正常连接WiFi 显示MQTT地址信息
        }
        lastRefresh = millis();
    }
}
// 初始化WiFi连接
void setupWiFi()
{
    // wm.setConfigPortalBlocking(false);
    wm.setAPCallback(configModeCallback); // 设置WiFi配网的回调函数

    //fetches ssid and pass and tries to connect
    //if it does not connect it starts an access point with the specified name
    //here  "AutoConnectAP"
    //and goes into a blocking loop awaiting configuration
    if (!wm.autoConnect())
    {
        Serial.println("failed to connect and hit timeout");
        //reset and try again, or maybe put it to deep sleep
        isWiFiConfigMode = true;
    }
    else
    {
        isWiFiConfigMode = false;
    }
}

// 保存记录的温度
float temperature = 0.0f;
float humidity = 0.0f;

#ifdef DHTPIN
void readDHTInfo()
{

    Serial.println(F("DHTxx Unified Sensor Example"));
    // Print temperature sensor details.
    sensor_t sensor;
    dht.temperature().getSensor(&sensor);
    Serial.println(F("------------------------------------"));
    Serial.println(F("Temperature Sensor"));
    Serial.print(F("Sensor Type: "));
    Serial.println(sensor.name);
    Serial.print(F("Driver Ver:  "));
    Serial.println(sensor.version);
    Serial.print(F("Unique ID:   "));
    Serial.println(sensor.sensor_id);
    Serial.print(F("Max Value:   "));
    Serial.print(sensor.max_value);
    Serial.println(F("°C"));
    Serial.print(F("Min Value:   "));
    Serial.print(sensor.min_value);
    Serial.println(F("°C"));
    Serial.print(F("Resolution:  "));
    Serial.print(sensor.resolution);
    Serial.println(F("°C"));
    Serial.println(F("------------------------------------"));
    // Print humidity sensor details.
    dht.humidity().getSensor(&sensor);
    Serial.println(F("Humidity Sensor"));
    Serial.print(F("Sensor Type: "));
    Serial.println(sensor.name);
    Serial.print(F("Driver Ver:  "));
    Serial.println(sensor.version);
    Serial.print(F("Unique ID:   "));
    Serial.println(sensor.sensor_id);
    Serial.print(F("Max Value:   "));
    Serial.print(sensor.max_value);
    Serial.println(F("%"));
    Serial.print(F("Min Value:   "));
    Serial.print(sensor.min_value);
    Serial.println(F("%"));
    Serial.print(F("Resolution:  "));
    Serial.print(sensor.resolution);
    Serial.println(F("%"));
    Serial.println(F("------------------------------------"));
}

void readDHTData()
{
    // Get temperature event and print its value.
    sensors_event_t event;
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature))
    {
        Serial.println(F("Error reading temperature!"));
    }
    else
    {
        Serial.print(F("Temperature: "));
        Serial.print(event.temperature);
        temperature = event.temperature;
        Serial.println(F("°C"));
    }
    // Get humidity event and print its value.
    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity))
    {
        Serial.println(F("Error reading humidity!"));
    }
    else
    {
        Serial.print(F("Humidity: "));
        Serial.print(event.relative_humidity);
        humidity = event.relative_humidity;
        Serial.println(F("%"));
    }
}
#endif

#ifdef SHTC3_ADDR
void readSHTC3Data()
{
    if (hasSHTC3)
    {
        mySHTC3.update();                               // Call "update()" to command a measurement, wait for measurement to complete, and update the RH and T members of the object
        if (mySHTC3.lastStatus == SHTC3_Status_Nominal) // You can also assess the status of the last command by checking the ".lastStatus" member of the object
        {
            temperature = mySHTC3.toDegC();
            humidity = mySHTC3.toPercent();
        }
    }
}
#endif

// // 注意看这里，这里开始定义TCP服务端，端口号是9999
// #define SERVER_PORT 9999
// WiFiServer server(SERVER_PORT);
#define BUFFER_SIZE 1024
char buffer[BUFFER_SIZE]; // 用于接收TCP客户端发来的信息的缓冲区
// int recv_length = 0;// 记录接收到的数据的大小

#define mqtt_server "192.168.96.155"
#define mqtt_port 1883
#define up_topic "v1/devices/me/telemetry"
#define username "7iPxb1oMvX7xm2yiwUQK"
#define password ""
WiFiClient espClient;
PubSubClient client(espClient);

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();
}

void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        // Create a random client ID
        String clientId = "ESP8266Client-";
        clientId += String(random(0xffff), HEX);
        // Attempt to connect
        if (client.connect(clientId.c_str(), username, password))
        {
            Serial.println("connected");
            // Once connected, publish an announcement...
            client.publish("outTopic", "hello world");
            // ... and resubscribe
            client.subscribe("inTopic");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

// 整个系统的初始化函数，所有初始化操作在这个函数执行
void setup()
{
    // 初始化串口用于调试输出
    Serial.begin(115200);
#ifdef DHTPIN
    dht.begin();
    readDHTInfo();
#endif

#ifdef SHTC3_ADDR
    scanI2Cdevice(Wire, SHTC3_SDA, SHTC3_SCL);
    Wire.begin(SHTC3_SDA, SHTC3_SCL);
    mySHTC3.begin(Wire);
#endif
    // 初始化WiFi连接
    setupWiFi();
    // 设置自动联网校时获取当前时间戳的函数
    setSyncProvider(getNowTimestamp);
    // 设置为每1800秒校时一次
    setSyncInterval(1800);
    // 系统上电首先联网获取到正确的时间并设置
    setTime(getNowTimestamp());
    Serial.printf("Now date and time:\r\n");
    Serial.printf("%02d-%02d-%02d\r\n", year(), month(), day());
    Serial.printf("%02d:%02d:%02d\r\n", hour(), minute(), second());
    
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
}
unsigned long lastMsg = 0;
// 整个系统的循环函数，当系统初始化完成后，会不断调用这个函数，在这里编写系统的主要工作
void loop()
{
    if (!client.connected())
    {
        // 注意 如果没有连接mqtt服务器，这里会一直尝试连接直到连接上
        reconnect();
    }
    client.loop();

    if (millis() - lastMsg > 1000)
    {
#ifdef DHTPIN
        readDHTData();
#endif
#ifdef SHTC3_ADDR
        readSHTC3Data();
#endif
        lastMsg = millis();
        // 只有连接上MQTT服务器后，才能发送温湿度信息
        sprintf(buffer, "{\"temperature\": %.2f,\"humidity\": %.2f}", temperature, humidity);
        Serial.print("Publish message: ");
        Serial.println(buffer);
        client.publish(up_topic, buffer);
    }
}



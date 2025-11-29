#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>

#define RX_PIN 16
#define TX_PIN 17

// ---------------- Sensor Values ----------------
float tempC = 0;
float tdsRaw = 0;
float tdsComp = 0;
float phValue = 0;
float turbidity = 0;

// ---------------- Wi-Fi Credentials ----------------
const char* ssidAP = "ESP32_SensorAP";
const char* passwordAP = "12345678";

const char* ssidSTA = "PLDTHOMEFIBRj8cGb"; // Online Wi-Fi
const char* passwordSTA = "PLDTWIFI55kU2";

// ---------------- MQTT ----------------
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "esp32/sensors";

WiFiClient espClient;
PubSubClient client(espClient);

// ---------------- Async Web Server ----------------
AsyncWebServer server(80);

// ---------------- Serial Reading ----------------
String readSerialLine() {
    static String line = "";
    while (Serial2.available()) {
        char c = Serial2.read();
        if (c == '\n') {
            String temp = line;
            line = "";
            return temp;
        } else if (c != '\r') {
            line += c;
        }
    }
    return "";
}

// ---------------- MQTT Functions ----------------
void reconnectMQTT() {
    while (!client.connected() && WiFi.status() == WL_CONNECTED) {
        Serial.print("Connecting MQTT...");
        if (client.connect("ESP32SensorClient")) {
            Serial.println("MQTT connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" — retry in 5s");
            delay(5000);
        }
    }
}

void sendMQTT() {
    if (!client.connected() && WiFi.status() == WL_CONNECTED) reconnectMQTT();
    client.loop();

    if (client.connected()) {
        String payload = "{\"tempC\":" + String(tempC, 2) +
                         ",\"tdsRaw\":" + String(tdsRaw, 2) +
                         ",\"tdsComp\":" + String(tdsComp, 2) +
                         ",\"ph\":" + String(phValue, 2) +
                         ",\"turbidity\":" + String(turbidity, 2) + "}";
        client.publish(mqtt_topic, payload.c_str());
        Serial.println("MQTT Published: " + payload);
    }
}

// ---------------- Setup ----------------
void setup() {
    Serial.begin(115200);
    Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

    // Start Access Point
    WiFi.softAP(ssidAP, passwordAP);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

    // Connect to Online Wi-Fi
    WiFi.begin(ssidSTA, passwordSTA);
    Serial.print("Connecting to WiFi STA");
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nSTA connected: " + WiFi.localIP().toString());
    } else {
        Serial.println("\nSTA connection failed, continuing offline.");
    }

    // Set MQTT Server
    client.setServer(mqtt_server, mqtt_port);

    // ---------------- Async Server Routes ----------------
    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
        String payload = String(tempC) + "," + String(tdsRaw) + "," + String(tdsComp) + "," + String(phValue) + "," + String(turbidity);
        request->send(200, "text/plain", payload);
    });

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = R"rawliteral(
        <!DOCTYPE html>
        <html>
        <head>
            <title>ESP32 Freshwater Monitor</title>
            <style>
                body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background:#e0f7fa; text-align:center; margin:0; padding:0; }
                h1 { color:#00796b; margin-top:40px; font-size:36px; }
                .container { display:flex; flex-wrap:wrap; justify-content:center; margin-top:40px; }
                .card { background:#ffffff; border-radius:20px; padding:40px 30px; margin:20px; width:280px; box-shadow:0 8px 16px rgba(0,0,0,0.3); transition: transform 0.2s; }
                .card:hover { transform: translateY(-10px); box-shadow:0 12px 24px rgba(0,0,0,0.4); }
                .title { font-size:24px; font-weight:bold; color:#00796b; }
                .value { font-size:32px; margin-top:15px; color:#004d40; }
            </style>
            <script>
                async function updateData() {
                    const response = await fetch('/data');
                    const text = await response.text();
                    const p = text.split(',');
                    document.getElementById('temp').innerHTML = parseFloat(p[0]).toFixed(2) + ' Celsius';
                    document.getElementById('tdsRaw').innerHTML = parseFloat(p[1]).toFixed(2) + ' ppm';
                    document.getElementById('tdsComp').innerHTML = parseFloat(p[2]).toFixed(2) + ' ppm';
                    document.getElementById('ph').innerHTML = parseFloat(p[3]).toFixed(2);
                    document.getElementById('turbidity').innerHTML = parseFloat(p[4]).toFixed(2) + ' NTU';
                }
                setInterval(updateData, 1500);
                window.onload = updateData;
            </script>
        </head>
        <body>
            <h1>ESP32 Live Sensor Monitor</h1>
            <div class='container'>
                <div class='card'><div class='title'>Temperature</div><div class='value' id='temp'>--</div></div>
                <div class='card'><div class='title'>TDS Raw</div><div class='value' id='tdsRaw'>--</div></div>
                <div class='card'><div class='title'>TDS Comp</div><div class='value' id='tdsComp'>--</div></div>
                <div class='card'><div class='title'>pH</div><div class='value' id='ph'>--</div></div>
                <div class='card'><div class='title'>Turbidity</div><div class='value' id='turbidity'>--</div></div>
            </div>
        </body>
        </html>
        )rawliteral";
        request->send(200, "text/html", html);
    });

    server.begin();  // Start AsyncWebServer
}

// ---------------- Loop ----------------
void loop() {
    // 1️⃣ Read Serial Data
    String data = readSerialLine();
    if (data.length() > 0) {
        data.trim();
        int parsed = sscanf(data.c_str(), "%f,%f,%f,%f,%f", &tempC, &tdsRaw, &tdsComp, &phValue, &turbidity);
        if (parsed != 5)
            Serial.println("Failed to parse: " + data);
        else {
            Serial.println("Parsed: " + data);
            sendMQTT(); // Publish immediately
        }
    }

    // 2️⃣ MQTT Loop
    if (WiFi.status() == WL_CONNECTED)
        client.loop();
}

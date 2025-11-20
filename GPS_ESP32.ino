#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

// WiFi
#define WIFI_SSID "TIGO-21D9"
#define WIFI_PASSWORD "PQREKGYJ1234"

// Firebase
#define API_KEY "AIzaSyCwYMYdSWzHCsvWPK1o1kE8DHBy2QjinSc"
#define DATABASE_URL "https://okakoro-8d8eb-default-rtdb.asia-southeast1.firebasedatabase.app/"

// Objetos Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// GPS (por UART2)
HardwareSerial GPS_Serial(2);  // UART2
TinyGPSPlus gps;

// Relé
#define RELAY_PIN 23

// Control de tiempo
unsigned long sendDataPrevMillis = 0;
const long timerDelay = 1000;
bool signupOK = false;

// Token Firebase
void tokenStatusCallback(TokenInfo info) {
  Serial.print("Token actualizado. Estado: ");
  Serial.println(info.status);
}

void setup() {
  Serial.begin(115200);
  GPS_Serial.begin(9600, SERIAL_8N1, 16, 17);  // RX=16, TX=17

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Por defecto apagado

  // Conectar a WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("🔌 Conectando a WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi conectado");
  Serial.print("📡 IP: "); Serial.println(WiFi.localIP());

  // Configuración Firebase
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("✅ Registro Firebase exitoso");
    signupOK = true;
  } else {
    Serial.printf("❌ Error Firebase: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  static unsigned long lastCheck = 0;
  static bool gpsConnected = false;

  while (GPS_Serial.available() > 0) {
    gps.encode(GPS_Serial.read());
    gpsConnected = true;
  }

  if (millis() - lastCheck > 5000) {
    if (!gpsConnected) {
      Serial.println("❌ No se detecta comunicación del GPS.");
    }
    gpsConnected = false;
    lastCheck = millis();
  }

  if (gps.location.isUpdated()) {
    double lat = gps.location.lat();
    double lng = gps.location.lng();

    Serial.println("\n📍 Coordenadas GPS actualizadas:");
    Serial.print("🌐 Latitud : "); Serial.println(lat, 6);
    Serial.print("🌐 Longitud: "); Serial.println(lng, 6);
    Serial.println("🗺️ Enlace Google Maps: https://www.google.com/maps?q=" + String(lat, 6) + "," + String(lng, 6));

    if (Firebase.ready() && signupOK && millis() - sendDataPrevMillis > timerDelay) {
      sendDataPrevMillis = millis();

      // Enviar coordenadas
      if (Firebase.RTDB.setDouble(&fbdo, "gps/latitud", lat)) {
        Serial.println("✅ Latitud actualizada");
      } else {
        Serial.println("❌ Error al enviar latitud: " + fbdo.errorReason());
      }

      if (Firebase.RTDB.setDouble(&fbdo, "gps/longitud", lng)) {
        Serial.println("✅ Longitud actualizada");
      } else {
        Serial.println("❌ Error al enviar longitud: " + fbdo.errorReason());
      }

      // Leer estado del relé
      if (Firebase.RTDB.getString(&fbdo, "relay/estado")) {
        String estadoRelay = fbdo.stringData();
        if (estadoRelay == "encender") {
          digitalWrite(RELAY_PIN, HIGH);
          Serial.println("🔔 Relay ENCENDIDO");
        } else if (estadoRelay == "apagar") {
          digitalWrite(RELAY_PIN, LOW);
          Serial.println("🔔 Relay APAGADO");
        } else {
          Serial.println("ℹ️ Estado del relay no reconocido: " + estadoRelay);
        }
      } else {
        Serial.println("⚠️ Error al leer relay: " + fbdo.errorReason());
      }
    }
  } else {
    Serial.println("🔄 Esperando señal GPS válida...");
    delay(2000);
  }
}

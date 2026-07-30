// =============================================================================
// Exemplo: ESP8266 + Relé via App Vollen
// =============================================================================
// Controle remoto de um relé pelo aplicativo Vollen.
//
// Esquema de ligacao:
//   - Relé no pino fisico que voce escolher (configurado pelo app)
//
// O pino GPIO e definido DINAMICAMENTE pelo app Vollen:
//   No app, va em "PINO GPIO" e escolha o pino desejado.
//   O ESP recebe a configuracao via MQTT e assume o pino escolhido.
//
// Instale as bibliotecas necessarias no Arduino IDE:
//   - PubSubClient    (Nick O'Leary)
//   - ArduinoJson     (Benoit Blanchon)
//   - VollenIoT       (Link no aplicativo Vollen IoT)
// =============================================================================

#include <ESP8266WiFi.h>
#include <VollenIoT.h>

VollenIoT vollen;

// ─── ID do dispositivo (gerado no app Vollen) ────────────────────────────────
const char *MEU_DISPOSITIVO = "vollen-xxxx-xxxx-xxxx-xxxx";

// ─── Pino do Botão Físico (opcional, descomente se quiser usar) ─────────────
// const uint8_t PIN_BOTAO = D2; // GPIO4
// const int DEBOUNCE_MS = 50;

// ─── Controle de estado ──────────────────────────────────────────────────────
bool wifiJaConectou = false;
unsigned long ultimoPublishStatus = 0;

// ─── Configura dispositivos ──────────────────────────────────────────────────
void setupDevices() {
  vollen.addDevice(MEU_DISPOSITIVO, "Meu Rele");

  // Pino padrao (D1 = GPIO5).
  // Voce pode mudar pelo app em "PINO GPIO".
  // Se escolher "Nenhum" no app, volta a usar este pino.
  vollen.setDevicePin(MEU_DISPOSITIVO, D1);

  vollen.setDeviceCommand(MEU_DISPOSITIVO, "0", "1");

  // ─── Botão físico (opcional) ──────────────────────────────────────────
  // pinMode(PIN_BOTAO, INPUT_PULLUP);
}

// ─── Setup
// ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);

  setupDevices();
  vollen.begin();
}

// ─── Loop
// ─────────────────────────────────────────────────────────────────────
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long ultimaTentativaWiFi = 0;
    if (ultimaTentativaWiFi == 0 || millis() - ultimaTentativaWiFi > 10000) {
      ultimaTentativaWiFi = millis();
      WiFi.begin();
    }
    wifiJaConectou = false;
  } else {
    if (!wifiJaConectou) {
      wifiJaConectou = true;
      Serial.printf("Conectado! IP: %s\n", WiFi.localIP().toString().c_str());
      vollen.connect();
      vollen.publishStatus(MEU_DISPOSITIVO, "online");
    }

    vollen.loop();

    // Publica status "online" a cada 60s para manter visivel no app
    if (millis() - ultimoPublishStatus > 60000) {
      ultimoPublishStatus = millis();
      vollen.publishStatus(MEU_DISPOSITIVO, "online");
    }

    // ─── Leitura do botão físico (descomente para usar) ──────────────
    /*
    static bool ultimoEstadoBotao = HIGH;
    static unsigned long ultimoDebounce = 0;

    bool leitura = digitalRead(PIN_BOTAO);
    if (leitura != ultimoEstadoBotao) {
        ultimoDebounce = millis();
        ultimoEstadoBotao = leitura;
    }
    if ((millis() - ultimoDebounce) > DEBOUNCE_MS && leitura == LOW) {
        vollen.toggleDevice(MEU_DISPOSITIVO);
        while (digitalRead(PIN_BOTAO) == LOW) {
            yield();
        }
    }
    */
  }
}

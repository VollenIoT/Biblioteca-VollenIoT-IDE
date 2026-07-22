// =============================================================================
// Exemplo: BotaoPadrao
// =============================================================================
// Sketch mínimo para controle de dispositivos Vollen IoT.
//
// Cada dispositivo recebe comandos do app e controla um rele.
// O par de comandos padrao e 0/1 (OFF/ON).
//
// O WiFi e o servidor nao travam o ESP — se desconectar, o botao fisico continua
// funcionando e a conexao e restaurada em segundo plano.
// =============================================================================

#include <VollenIoT.h>
#include <ESP8266WiFi.h>

VollenIoT vollen;

// ─── Credenciais WiFi ───────────────────────────────────────────────────────
const char* WIFI_SSID = "REDE-WIFI";
const char* WIFI_PASS = "SENHA-WIFI";

// ─── IDs do dispositivo (o mesmo cadastrado no app) ─────────────
const char* Dispositivo1 = "vollen-xxxx-xxxx-xxxx-xxxx";

// ─── Pinos fisicos ──────────────────────────────────────────────────────────
const uint8_t PIN_RELE1  = D5;
const uint8_t PIN_BOTAO1 = D1;

// ─── Debounce ───────────────────────────────────────────────────────────────
unsigned long ultimoDebounce = 0;
bool estadoBotaoAnterior = HIGH;
bool wifiJaConectou = false;

// ─── Configura dispositivos ─────────────────────────────────────────────────
void setupDevices() {
    vollen.addDevice(Dispositivo1, "Dispositivo1");

    vollen.setDevicePin(Dispositivo1, PIN_RELE1);
    vollen.setDeviceCommand(Dispositivo1, "0", "1");
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    pinMode(PIN_BOTAO1, INPUT_PULLUP);

    WiFi.mode(WIFI_STA);

    setupDevices();
    vollen.begin();
}

// ─── Loop ────────────────────────────────────────────────────────────────────
void loop() {
    // ─── WiFi: tenta conectar sem travar ────────────────────────────────
    if (WiFi.status() != WL_CONNECTED) {
        static unsigned long ultimaTentativaWiFi = 0;
        if (ultimaTentativaWiFi == 0 || millis() - ultimaTentativaWiFi > 10000) {
            ultimaTentativaWiFi = millis();
            WiFi.begin(WIFI_SSID, WIFI_PASS);
        }
        wifiJaConectou = false;
    } else {
        if (!wifiJaConectou) {
            wifiJaConectou = true;
            Serial.printf("WiFi: %s | IP: %s\n", WIFI_SSID, WiFi.localIP().toString().c_str());
            vollen.connect();
        }
        vollen.loop();
    }

    // ─── Botao fisico (funciona sempre, mesmo sem WiFi) ─────────────────
    bool leitura = digitalRead(PIN_BOTAO1);
    if (leitura != estadoBotaoAnterior) {
        ultimoDebounce = millis();
        estadoBotaoAnterior = leitura;
    }

    if ((millis() - ultimoDebounce) > 50 && leitura == LOW) {
        vollen.toggleDevice(Dispositivo1);
        Serial.printf("Dispositivo1 %s\n",
                      vollen.getDeviceState(Dispositivo1) ? "Ligado" : "Desligado");
        while (digitalRead(PIN_BOTAO1) == LOW) {
            delay(10);
        }
        ultimoDebounce = millis();
        estadoBotaoAnterior = HIGH;
    }
}

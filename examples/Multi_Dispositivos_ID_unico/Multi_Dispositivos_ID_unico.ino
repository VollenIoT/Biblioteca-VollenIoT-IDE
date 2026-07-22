// =============================================================================
// Exemplo: MultiDevice
// =============================================================================
// Demonstra o uso de Multi-Device ID: varios ESPs compartilham o mesmo ID,
// cada um responde apenas ao seu par de comandos (0/1, 2/3, 4/5...).
//
// Fluxo no App (conta desenvolvedor):
//   1. Crie um botao (comandos padrao 0/1)
//   2. Em "Editar Painel", clique em "Clonar" para gerar clones com pares
//      sequenciais (2/3, 4/5...). Todos os clones tem o MESMO ID.
//   3. Configure cada ESP com um par diferente via setDeviceCommand().
// =============================================================================
//
// Configuracao dos ESPs:
//   ESP1 (pino D1): setDeviceCommand(id, "0", "1")  → responde a 0/1
//   ESP2 (pino D2): setDeviceCommand(id, "2", "3")  → responde a 2/3
//   ESP3 (pino D3): setDeviceCommand(id, "4", "5")  → responde a 4/5
//
// IMPORTANTE: Se outro ESP com o mesmo ID estiver online, uma mensagem
// de aviso aparecera no Monitor Serial indicando ID duplicado.
// =============================================================================

#include <VollenIoT.h>
#include <ESP8266WiFi.h>

VollenIoT vollen;

// ─── Credenciais WiFi ───────────────────────────────────────────────────────
const char* WIFI_SSID = "REDE-WIFI";
const char* WIFI_PASS = "SENHA-WIFI";

// ─── ID do dispositivo (MESMO ID em todos os ESPs que compartilham o rele) ──
const char* MeuRele = "vollen-xxxx-xxxx-xxxx-xxxx";

// ─── Pino fisico deste ESP ─────────────────────────────────────────────────
const uint8_t PIN_RELE  = D1;
const uint8_t PIN_BOTAO = D5;

// ─── PAR DE COMANDOS DESTE ESP ─────────────────────────────────────────────
// Altere para o par desejado:
//   0/1 → primeiro rele
//   2/3 → segundo rele
//   4/5 → terceiro rele
//   6/7 → quarto rele
//   8/9 → quinto rele
const char* CMD_OFF = "0";
const char* CMD_ON  = "1";

// ─── Debounce ───────────────────────────────────────────────────────────────
unsigned long ultimoDebounce = 0;
bool estadoBotaoAnterior = HIGH;
bool wifiJaConectou = false;

// ─── Configura dispositivos ─────────────────────────────────────────────────
void setupDevices() {
    vollen.addDevice(MeuRele, "Meu Rele");
    vollen.setDevicePin(MeuRele, PIN_RELE);
    // Define o par de comandos que este ESP deve atender
    vollen.setDeviceCommand(MeuRele, CMD_OFF, CMD_ON);
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    pinMode(PIN_BOTAO, INPUT_PULLUP);

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

    // ─── Botao fisico (funciona mesmo sem WiFi) ─────────────────────────
    bool leitura = digitalRead(PIN_BOTAO);
    if (leitura != estadoBotaoAnterior) {
        ultimoDebounce = millis();
        estadoBotaoAnterior = leitura;
    }

    if ((millis() - ultimoDebounce) > 50 && leitura == LOW) {
        vollen.toggleDevice(MeuRele);
        Serial.printf("%s %s\n", MeuRele,
                      vollen.getDeviceState(MeuRele) ? "Ligado" : "Desligado");
        while (digitalRead(PIN_BOTAO) == LOW) delay(10);
        ultimoDebounce = millis();
        estadoBotaoAnterior = HIGH;
    }
}

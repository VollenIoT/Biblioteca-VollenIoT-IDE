# Biblioteca Vollen IoT para Arduino IDE

Esta é a biblioteca oficial do ecossistema **Vollen IoT** para microcontroladores compatíveis com o ecossistema Arduino (como **ESP8266** e **ESP32**). Com ela, você pode integrar relés físicos, interruptores, lâmpadas e sensores ao aplicativo do celular de forma extremamente simples, estável e não bloqueante.

---

## 🚀 Principais Recursos

* **Comunicação Não Bloqueante:** A conexão WiFi e MQTT roda em segundo plano. Se a conexão cair, os botões e interruptores físicos do seu circuito continuam funcionando normalmente.
* **Sincronização Dinâmica de Comandos (OTA):** A biblioteca aprende dinamicamente a ordem lógica dos comandos configurados no seu aplicativo celular (`0/1`, `1/0`, `2/3`, `4/5`, etc.) e inverte a saída do pino físico do relé automaticamente na memória RAM, sem que você precise regravar a Sketch.
* **Temporizadores Embutidos:** Suporta configuração de timers direto pelo aplicativo para ligar/desligar com contagem regressiva em minutos e segundos de forma nativa.
* **Proteção contra ID Duplicado:** Evita que duas placas rodem o mesmo ID ao mesmo tempo no broker MQTT.

---

## 📦 Dependências da Biblioteca

Para compilar os projetos com a VollenIoT, você precisa instalar as seguintes dependências no gerenciador de bibliotecas da sua Arduino IDE:
1. **PubSubClient** (de Nick O'Leary) - Para comunicação MQTT.
2. **ArduinoJson** (de Benoit Blanchon) - Para processar temporizadores e configurações via JSON.

---

## 🛠️ Como Instalar a Biblioteca

1. Baixe o arquivo ZIP deste repositório (Clique no botão verde **Code** -> **Download ZIP**).
2. Na Arduino IDE, vá no menu **Rascunho** -> **Incluir Biblioteca** -> **Adicionar Biblioteca .ZIP**.
3. Selecione o arquivo ZIP baixado.
4. Pronto! Os exemplos de código já estarão disponíveis em **Arquivo** -> **Exemplos** -> **VollenIoT**.

---

## 💻 Exemplo de Código Mínimo

Abaixo está um exemplo básico de como usar a biblioteca para controlar um relé físico com um botão físico (usando debounce):

```cpp
#include <VollenIoT.h>
#include <ESP8266WiFi.h>

VollenIoT vollen;

// ─── Credenciais do seu WiFi ───────────────────────────────────────────────
const char* WIFI_SSID = "NOME_DO_SEU_WIFI";
const char* WIFI_PASS = "SENHA_DO_SEU_WIFI";

// ─── ID do Dispositivo (copie do painel do seu Aplicativo Vollen IoT) ───────
const char* DispositivoID = "vollen-xxxx-xxxx-xxxx-xxxx";

// ─── Configurações de Pinos ────────────────────────────────────────────────
const uint8_t PIN_RELE  = D4; // Pino onde o relé está conectado
const uint8_t PIN_BOTAO = 0;  // Pino do botão físico no circuito

// Controle de Debounce do botão físico
unsigned long ultimoDebounce = 0;
bool estadoBotaoAnterior = HIGH;
bool wifiConectadoAnterior = false;

void setupDevices() {
    // Adiciona o dispositivo à biblioteca
    vollen.addDevice(DispositivoID, "Dispositivo 1");
    
    // Vincula o pino do relé ao ID do dispositivo
    vollen.setDevicePin(DispositivoID, PIN_RELE);
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_BOTAO, INPUT_PULLUP);
    WiFi.mode(WIFI_STA);

    setupDevices();
    vollen.begin(); // Inicializa as configurações da VollenIoT
}

void loop() {
    // Tenta conectar ao WiFi de forma não bloqueante
    if (WiFi.status() != WL_CONNECTED) {
        static unsigned long ultimaTentativaWiFi = 0;
        if (ultimaTentativaWiFi == 0 || millis() - ultimaTentativaWiFi > 10000) {
            ultimaTentativaWiFi = millis();
            WiFi.begin(WIFI_SSID, WIFI_PASS);
        }
        wifiConectadoAnterior = false;
    } else {
        if (!wifiConectadoAnterior) {
            wifiConectadoAnterior = true;
            Serial.printf("WiFi Conectado! IP: %s\n", WiFi.localIP().toString().c_str());
            vollen.connect(); // Conecta ao servidor MQTT da Vollen IoT
        }
        vollen.loop(); // Mantém viva a comunicação MQTT em background
    }

    // ─── Leitura do Botão Físico no pino ──────────────────────────────────
    bool leitura = digitalRead(PIN_BOTAO);
    if (leitura != estadoBotaoAnterior) {
        ultimoDebounce = millis();
        estadoBotaoAnterior = leitura;
    }

    if ((millis() - ultimoDebounce) > 50 && leitura == LOW) {
        // Altera o estado do dispositivo na biblioteca e envia para o app celular
        vollen.toggleDevice(DispositivoID);
        
        Serial.printf("Dispositivo alterado fisicamente para: %s\n",
                      vollen.getDeviceState(DispositivoID) ? "LIGADO" : "DESLIGADO");
                      
        while (digitalRead(PIN_BOTAO) == LOW) { delay(10); } // Aguarda soltar o botão
        ultimoDebounce = millis();
        estadoBotaoAnterior = HIGH;
    }
}
```

---

## ⚙️ Sincronização e Inversão Dinâmica de Comandos (Active Low / Active High)

A biblioteca detecta de forma totalmente inteligente as ordens de comandos numéricos e textuais configurados no painel do celular:
* **Par de Comandos Padrão (`0/1` ou `2/3`...):** Ao ligar no app, o ESP recebe o maior valor e coloca o pino físico em nível lógico alto (`HIGH`). Ao desligar, recebe o menor valor e coloca em `LOW`.
* **Par de Comandos Invertido (`1/0` ou `3/2`...):** Se você alterar no aplicativo para ligar com `0` (útil para relés de lógica invertida **Active Low**), a placa detecta automaticamente o menor valor como ativo e coloca o pino em `LOW` para ligar e `HIGH` para desligar.

Isso significa que você tem controle absoluto da lógica elétrica do seu hardware direto pelo celular, **sem precisar reprogramar a sua placa física**!

---

## 📄 Licença

Este projeto está sob a licença **MIT** - consulte o arquivo [LICENSE](LICENSE) para obter mais detalhes.

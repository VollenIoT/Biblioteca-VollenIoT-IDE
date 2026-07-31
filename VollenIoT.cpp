// =============================================================================
// VollenIoT.cpp - Implementação da biblioteca VollenIoT
// =============================================================================
//
// ATENÇÃO: As credenciais MQTT abaixo são do desenvolvedor e ficam
// embutidas na biblioteca. O usuário final não precisa configurá-las.
// =============================================================================

#include "VollenIoT.h"

// ─── Credenciais MQTT (embutidas — invisíveis para o usuário final) ─────────
static const char *MQTT_HOST = "k7115118.ala.us-east-1.emqxsl.com";
static const uint16_t MQTT_PORT = 8883;
static const char *MQTT_USER = "volleniot";
static const char *MQTT_PASS = "s4ULdxieG6Rbxu7";

VollenIoT *VollenIoT::_instance = nullptr;

// -----------------------------------------------------------------------------
// Construtor
// -----------------------------------------------------------------------------
VollenIoT::VollenIoT()
    : _mqttPort(MQTT_PORT), _deviceCount(0), _onStateChange(nullptr),
      _lastReconnectAttempt(0), _lastTimerCheck(0), _lastHeartbeat(0),
      _heartbeatInterval(30000) // 30 segundos default
      ,
      _mqttClient(_wifiClient) {
  _instance = this;
  _mqttClient.setCallback(_mqttCallback);
  _mqttClient.setBufferSize(512);

  strncpy(_mqttHost, MQTT_HOST, sizeof(_mqttHost) - 1);
  strncpy(_mqttUser, MQTT_USER, sizeof(_mqttUser) - 1);
  strncpy(_mqttPass, MQTT_PASS, sizeof(_mqttPass) - 1);

  _bootId = 0;
  _duplicateDetected = false;
  _developerMode = false;

  for (uint8_t i = 0; i < MAX_DEVICES; i++) {
    _devices[i].active = false;
    _devices[i].state = false;
    _devices[i].pin = Vollen_NO_PIN;
    _devices[i].id = nullptr;
    _devices[i].name[0] = '\0';
    _devices[i].timerEnd = 0;
    _devices[i].timerAction = false;
    _devices[i].pendingMinutes = 0;
    _devices[i].pendingAction = false;
  }
}

// -----------------------------------------------------------------------------
// Utilitário
// -----------------------------------------------------------------------------
const char *VollenIoT::_deviceLabel(int idx) const {
  return (_devices[idx].name[0] != '\0') ? _devices[idx].name
                                         : _devices[idx].id;
}

// -----------------------------------------------------------------------------
// Gerenciamento de dispositivos
// -----------------------------------------------------------------------------
bool VollenIoT::addDevice(const char *id, const char *name) {
  return _addDeviceInternal(id, name, Vollen_NO_PIN, false);
}

bool VollenIoT::setDevicePin(const char *id, uint8_t pin) {
  int idx = _findDevice(id);
  if (idx < 0) {
    Serial.printf("[VollenIoT] Dispositivo nao encontrado: %s\n", id);
    return false;
  }
  _devices[idx].pin = pin;
  _devices[idx].defaultPin = pin;
  pinMode(pin, OUTPUT);
  bool isInverted = isDeviceCommandInverted(id);
  int pinVal = (_devices[idx].state ^ isInverted) ? HIGH : LOW;
  digitalWrite(pin, pinVal);
  Serial.printf("[VollenIoT] Pino %d associado a %s\n", pin, _deviceLabel(idx));
  return true;
}

bool VollenIoT::setDeviceName(const char *id, const char *name) {
  int idx = _findDevice(id);
  if (idx < 0)
    return false;
  _devices[idx].name[0] = '\0';
  if (name) {
    strncat(_devices[idx].name, name, sizeof(_devices[idx].name) - 1);
  }
  return true;
}

bool VollenIoT::_addDeviceInternal(const char *id, const char *name,
                                   uint8_t pin, bool state) {
  if (_deviceCount >= MAX_DEVICES) {
    Serial.println("[VollenIoT] ERRO: Numero maximo de dispositivos atingido.");
    return false;
  }
  if (_findDevice(id) >= 0) {
    Serial.print("[VollenIoT] Aviso: Dispositivo ja existe: ");
    Serial.println(id);
    return false;
  }
  _devices[_deviceCount].id = strdup(id);
  _devices[_deviceCount].pin = pin;
  _devices[_deviceCount].defaultPin = pin;
  _devices[_deviceCount].state = state;
  _devices[_deviceCount].active = true;
  _devices[_deviceCount].name[0] = '\0';
  _devices[_deviceCount].timerEnd = 0;
  _devices[_deviceCount].timerAction = false;
  _devices[_deviceCount].pendingMinutes = 0;
  _devices[_deviceCount].pendingSeconds = 0;
  _devices[_deviceCount].pendingAction = false;
  strncpy(_devices[_deviceCount].comandoOff, "0",
          sizeof(_devices[_deviceCount].comandoOff) - 1);
  strncpy(_devices[_deviceCount].comandoOn, "1",
          sizeof(_devices[_deviceCount].comandoOn) - 1);
  if (name) {
    strncat(_devices[_deviceCount].name, name,
            sizeof(_devices[_deviceCount].name) - 1);
  }

  if (pin != Vollen_NO_PIN) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, state ? HIGH : LOW);
  }

  Serial.printf("[VollenIoT] Dispositivo adicionado: %s\n",
                _deviceLabel(_deviceCount));
  _deviceCount++;
  return true;
}

void VollenIoT::removeDevice(const char *id) {
  int idx = _findDevice(id);
  if (idx < 0)
    return;

  if (_devices[idx].id) {
    free(const_cast<char *>(_devices[idx].id));
  }
  for (uint8_t i = idx; i < _deviceCount - 1; i++) {
    _devices[i] = _devices[i + 1];
  }
  _deviceCount--;
  _devices[_deviceCount].active = false;
  _devices[_deviceCount].id = nullptr;
}

bool VollenIoT::setDeviceCommand(const char *id, const char *comandoOff,
                                 const char *comandoOn) {
  int idx = _findDevice(id);
  if (idx < 0) {
    Serial.printf("[VollenIoT] Dispositivo nao encontrado: %s\n", id);
    return false;
  }
  strncpy(_devices[idx].comandoOff, comandoOff,
          sizeof(_devices[idx].comandoOff) - 1);
  strncpy(_devices[idx].comandoOn, comandoOn,
          sizeof(_devices[idx].comandoOn) - 1);
  Serial.printf("%s Comandos: %s (OFF) / %s (ON)\n", _deviceLabel(idx),
                _devices[idx].comandoOff, _devices[idx].comandoOn);
  return true;
}

void VollenIoT::setDeveloperMode(bool enable) {
  _developerMode = enable;
  if (enable) {
    Serial.println(
        "[VollenIoT] Modo Desenvolvedor ATIVADO. Multi-device ID permitido.");
  }
}

int VollenIoT::_findDevice(const char *id) const {
  for (uint8_t i = 0; i < _deviceCount; i++) {
    if (_devices[i].active && strcmp(_devices[i].id, id) == 0) {
      return i;
    }
  }
  return -1;
}

// -----------------------------------------------------------------------------
// Leitura e escrita de estado
// -----------------------------------------------------------------------------
bool VollenIoT::getDeviceState(const char *id) const {
  int idx = _findDevice(id);
  return (idx >= 0) ? _devices[idx].state : false;
}

void VollenIoT::setDeviceState(const char *id, bool newState) {
  int idx = _findDevice(id);
  if (idx < 0)
    return;

  bool mudou = (_devices[idx].state != newState);
  _devices[idx].state = newState;

  if (mudou &&
      (_devices[idx].pendingMinutes > 0 || _devices[idx].pendingSeconds > 0)) {
    if (newState != _devices[idx].pendingAction) {
      unsigned long totalMs =
          ((unsigned long)_devices[idx].pendingMinutes * 60000UL) +
          ((unsigned long)_devices[idx].pendingSeconds * 1000UL);
      _devices[idx].timerEnd = millis() + totalMs;
      _devices[idx].timerAction = _devices[idx].pendingAction;
      Serial.printf("%s Timer iniciado: %d min e %d seg para %s\n",
                    _deviceLabel(idx), _devices[idx].pendingMinutes,
                    _devices[idx].pendingSeconds,
                    _devices[idx].pendingAction ? "Ligar" : "Desligar");
    } else {
      _devices[idx].timerEnd = 0;
      Serial.printf("%s Timer cancelado\n", _deviceLabel(idx));
    }
  }

  if (_devices[idx].pin != Vollen_NO_PIN) {
    bool isInverted = isDeviceCommandInverted(id);
    int pinVal = (newState ^ isInverted) ? HIGH : LOW;
    digitalWrite(_devices[idx].pin, pinVal);
  }
  publishState(id, newState);

  if (_onStateChange) {
    _onStateChange(id, newState);
  }
}

void VollenIoT::toggleDevice(const char *id) {
  int idx = _findDevice(id);
  if (idx < 0)
    return;
  setDeviceState(id, !_devices[idx].state);
}

bool VollenIoT::isDeviceCommandInverted(const char *id) {
  int idx = _findDevice(id);
  if (idx < 0)
    return false;

  // Se comandoOn for "0" ou "OFF" ou "false", é invertido
  if (strcmp(_devices[idx].comandoOn, "0") == 0 ||
      strcasecmp(_devices[idx].comandoOn, "OFF") == 0 ||
      strcasecmp(_devices[idx].comandoOn, "false") == 0) {
    return true;
  }

  int valOn = atoi(_devices[idx].comandoOn);
  int valOff = atoi(_devices[idx].comandoOff);
  if (valOn != 0 || valOff != 0) {
    if (valOn < valOff)
      return true;
  }

  return false;
}

// -----------------------------------------------------------------------------
// Callback
// -----------------------------------------------------------------------------
void VollenIoT::onStateChange(void (*callback)(const char *, bool)) {
  _onStateChange = callback;
}

void VollenIoT::setHeartbeatInterval(unsigned long intervalMs) {
  _heartbeatInterval = intervalMs;
}

// -----------------------------------------------------------------------------
// Ciclo de vida
// -----------------------------------------------------------------------------
void VollenIoT::begin() {
  Serial.println();
  Serial.println("==============================");
  Serial.println("  Vollen IoT - ESP8266");
  Serial.println("==============================");

  randomSeed(ESP.getCycleCount());
  _bootId = random(100000, 999999);

  _setupPins();
  delay(100);
}

void VollenIoT::_setupPins() {
  for (uint8_t i = 0; i < _deviceCount; i++) {
    if (_devices[i].active && _devices[i].pin != Vollen_NO_PIN) {
      pinMode(_devices[i].pin, OUTPUT);
      bool isInverted = isDeviceCommandInverted(_devices[i].id);
      int pinVal = (_devices[i].state ^ isInverted) ? HIGH : LOW;
      digitalWrite(_devices[i].pin, pinVal);
    }
  }
}

void VollenIoT::connect() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(
        "[VollenIoT] Wi-Fi nao conectado. Chame WiFi.begin() no seu sketch.");
    return;
  }
  _connectMQTT();
}

void VollenIoT::loop() {
  unsigned long agora = millis();
  if (agora - _lastTimerCheck >= 500) {
    _lastTimerCheck = agora;
    _checkTimers();
  }

  // Heartbeat periódico: publica "online" para todos os dispositivos ativos
  if (agora - _lastHeartbeat >= _heartbeatInterval) {
    _lastHeartbeat = agora;
    if (_mqttClient.connected()) {
      for (uint8_t i = 0; i < _deviceCount; i++) {
        if (_devices[i].active) {
          publishStatus(_devices[i].id, "online");
        }
      }
    }
  }

  if (WiFi.status() != WL_CONNECTED)
    return;

  if (!_mqttClient.connected()) {
    if (agora - _lastReconnectAttempt > 5000) {
      _lastReconnectAttempt = agora;
      _connectMQTT();
    }
  } else {
    _mqttClient.loop();
  }
}

// -----------------------------------------------------------------------------
// Temporizador
// -----------------------------------------------------------------------------
void VollenIoT::_checkTimers() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < _deviceCount; i++) {
    if (!_devices[i].active || _devices[i].timerEnd == 0)
      continue;
    if (now >= _devices[i].timerEnd) {
      bool alvo = _devices[i].timerAction;
      _devices[i].timerEnd = 0;
      Serial.printf("%s Temporizador: %s\n", _deviceLabel(i),
                    alvo ? "Ligado" : "Desligado");
      setDeviceState(_devices[i].id, alvo);
    }
  }
}

// -----------------------------------------------------------------------------
// MQTT
// -----------------------------------------------------------------------------
void VollenIoT::_connectMQTT() {
  if (_mqttClient.connected())
    return;

  _wifiClient.setInsecure();
  _wifiClient.setTimeout(5);
  _mqttClient.setServer(_mqttHost, _mqttPort);

  char clientId[32];
  snprintf(clientId, sizeof(clientId), "VollenIoT_ESP_%06X", ESP.getChipId());

  bool conectado = false;
  char willTopic[128] = "";
  if (_deviceCount > 0 && _devices[0].active) {
    snprintf(willTopic, sizeof(willTopic), "%s/status", _devices[0].id);
    conectado = _mqttClient.connect(clientId, _mqttUser, _mqttPass, willTopic,
                                    1, true, "offline");
  } else {
    conectado = _mqttClient.connect(clientId, _mqttUser, _mqttPass);
  }

  if (conectado) {
    Serial.println("[VollenIoT] Dispositivo conectado ao servidor.");
    _duplicateDetected = false;

    publishAllStatus("online");
    for (uint8_t i = 0; i < _deviceCount; i++) {
      if (_devices[i].active) {
        publishState(_devices[i].id, _devices[i].state);
      }
    }

    for (uint8_t i = 0; i < _deviceCount; i++) {
      if (_devices[i].active) {
        char statusTopic[128];
        snprintf(statusTopic, sizeof(statusTopic), "%s/status", _devices[i].id);
        _mqttClient.subscribe(_devices[i].id, 1);
        _mqttClient.subscribe(statusTopic, 1);
      }
    }
  }
}

// -----------------------------------------------------------------------------
// Publicação
// -----------------------------------------------------------------------------
void VollenIoT::publishStatus(const char *deviceId, const char *status) {
  if (!_mqttClient.connected())
    return;
  char topic[128];
  snprintf(topic, sizeof(topic), "%s/status", deviceId);
  char payload[64];
  if (strcmp(status, "online") == 0) {
    snprintf(payload, sizeof(payload), "online:%lu", _bootId);
  } else {
    snprintf(payload, sizeof(payload), "%s", status);
  }
  _mqttClient.publish(topic, payload, true);
}

void VollenIoT::publishState(const char *deviceId, bool state) {
  if (!_mqttClient.connected())
    return;
  char topic[128];
  snprintf(topic, sizeof(topic), "%s/state", deviceId);
  _mqttClient.publish(topic, state ? "ON" : "OFF", true);
}

void VollenIoT::publishAllStatus(const char *status) {
  for (uint8_t i = 0; i < _deviceCount; i++) {
    if (_devices[i].active) {
      publishStatus(_devices[i].id, status);
    }
  }
}

void VollenIoT::publishAllState(bool state) {
  for (uint8_t i = 0; i < _deviceCount; i++) {
    if (_devices[i].active) {
      publishState(_devices[i].id, state);
    }
  }
}

// -----------------------------------------------------------------------------
// Status
// -----------------------------------------------------------------------------
bool VollenIoT::isWiFiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool VollenIoT::isMQTTConnected() { return _mqttClient.connected(); }

// -----------------------------------------------------------------------------
// Processamento de mensagens MQTT
// -----------------------------------------------------------------------------
bool VollenIoT::_processMessage(const char *topic, const uint8_t *payload,
                                size_t length) {
  char message[256];
  size_t len = (length < sizeof(message) - 1) ? length : sizeof(message) - 1;
  memcpy(message, payload, len);
  message[len] = '\0';

  // Verifica se é uma mensagem de status (deviceId/status)
  const char *slash = strchr(topic, '/');
  if (slash) {
    char deviceId[128];
    size_t idLen = slash - topic;
    if (idLen >= sizeof(deviceId))
      idLen = sizeof(deviceId) - 1;
    memcpy(deviceId, topic, idLen);
    deviceId[idLen] = '\0';

    if (strcmp(slash + 1, "status") == 0) {
      int idx = _findDevice(deviceId);
      if (idx >= 0) {
        // Detecta outro dispositivo com o mesmo ID (bootId diferente)
        unsigned long otherBootId;
        if (sscanf(message, "online:%lu", &otherBootId) == 1 &&
            otherBootId != _bootId) {
          if (!_developerMode) {
            Serial.printf("\n[VollenIoT] ERRO: ID duplicado detectado para %s! "
                          "Desconectando do MQTT...\n\n",
                          deviceId);
            _mqttClient.disconnect();
            _duplicateDetected = true;
          } else {
            if (!_duplicateDetected) {
              _duplicateDetected = true;
              Serial.printf("\n*** AVISO: ID duplicado detectado (Modo "
                            "Desenvolvedor Ativo)! ***\n");
              Serial.printf(
                  "*** Dispositivo %s ja esta online em outro ESP ***\n",
                  deviceId);
              Serial.printf(
                  "*** Ambos responderao aos mesmos comandos. ***\n\n");
            }
          }
        }
      }
      return true;
    }
    return false;
  }

  char topicLower[128];
  strncpy(topicLower, topic, sizeof(topicLower) - 1);
  topicLower[sizeof(topicLower) - 1] = '\0';
  for (int i = 0; topicLower[i]; i++) {
    topicLower[i] = tolower(topicLower[i]);
  }

  if (message[0] == '{') {
    StaticJsonDocument<192> doc;
    DeserializationError err = deserializeJson(doc, message);
    if (err) {
      Serial.printf("[VollenIoT] Erro JSON: %s\n", err.c_str());
      return false;
    }

    // Verifica se é configuração dinâmica de comandos
    JsonObject config = doc["config"];
    if (!config.isNull()) {
      const char *cmdOn = config["on"];
      const char *cmdOff = config["off"];
      int idx = _findDevice(topicLower);
      if (idx < 0)
        idx = _findDevice(topic);
      if (idx >= 0) {
        if (cmdOn)
          strncpy(_devices[idx].comandoOn, cmdOn,
                  sizeof(_devices[idx].comandoOn) - 1);
        if (cmdOff)
          strncpy(_devices[idx].comandoOff, cmdOff,
                  sizeof(_devices[idx].comandoOff) - 1);
        if (config.containsKey("pin")) {
          int newPin = config["pin"];
          uint8_t oldPin = _devices[idx].pin;

          if (newPin >= 0) {
            if (oldPin != Vollen_NO_PIN && oldPin != (uint8_t)newPin) {
              bool oldInverted = isDeviceCommandInverted(topic);
              digitalWrite(oldPin, oldInverted ? HIGH : LOW);
              pinMode(oldPin, INPUT);
              Serial.printf("%s Pino antigo GPIO%d desligado e liberado.\n",
                            _deviceLabel(idx), oldPin);
            }
            _devices[idx].pin = (uint8_t)newPin;
            pinMode(_devices[idx].pin, OUTPUT);
            bool isInverted = isDeviceCommandInverted(topic);
            int pinVal = (_devices[idx].state ^ isInverted) ? HIGH : LOW;
            digitalWrite(_devices[idx].pin, pinVal);
            Serial.printf("%s Pino GPIO reconfigurado via MQTT para: GPIO%d\n",
                          _deviceLabel(idx), _devices[idx].pin);
          } else {
            if (oldPin != Vollen_NO_PIN && oldPin != _devices[idx].defaultPin) {
              bool oldInverted = isDeviceCommandInverted(topic);
              digitalWrite(oldPin, oldInverted ? HIGH : LOW);
              pinMode(oldPin, INPUT);
              Serial.printf("%s Pino antigo GPIO%d desligado e liberado.\n",
                            _deviceLabel(idx), oldPin);
            }
            _devices[idx].pin = _devices[idx].defaultPin;
            if (_devices[idx].pin != Vollen_NO_PIN) {
              pinMode(_devices[idx].pin, OUTPUT);
              bool isInverted = isDeviceCommandInverted(topic);
              int pinVal = (_devices[idx].state ^ isInverted) ? HIGH : LOW;
              digitalWrite(_devices[idx].pin, pinVal);
              Serial.printf(
                  "%s Pino GPIO restaurado para o padrao do Sketch: GPIO%d\n",
                  _deviceLabel(idx), _devices[idx].pin);
            } else {
              Serial.printf("%s Pino GPIO redefinido para Padrao (Auto / "
                            "Sketch sem pino associado)\n",
                            _deviceLabel(idx));
            }
          }
        }
        Serial.printf("%s Comandos atualizados via MQTT: %s (OFF) / %s (ON)\n",
                      _deviceLabel(idx), _devices[idx].comandoOff,
                      _devices[idx].comandoOn);
      }
      return true;
    }

    JsonObject timer = doc["timer"];
    if (timer.isNull()) {
      Serial.println("[VollenIoT] JSON recebido sem campo 'timer' ou 'config'");
      return false;
    }

    const char *action = timer["action"];
    int minutes = timer["minutes"] | 0;
    int seconds = timer["seconds"] | 0;

    int idx = _findDevice(topicLower);
    if (idx < 0)
      idx = _findDevice(topic);
    if (idx < 0)
      return false;

    // Cancela timer
    if (minutes <= 0 && seconds <= 0) {
      _devices[idx].timerEnd = 0;
      _devices[idx].pendingMinutes = 0;
      _devices[idx].pendingSeconds = 0;
      Serial.printf("%s Timer cancelado\n", _deviceLabel(idx));
      return true;
    }

    bool alvo = (strcmp(action, "ON") == 0);

    _devices[idx].pendingMinutes = minutes;
    _devices[idx].pendingSeconds = seconds;
    _devices[idx].pendingAction = alvo;

    unsigned long totalMs =
        ((unsigned long)minutes * 60000UL) + ((unsigned long)seconds * 1000UL);

    if (_devices[idx].state != alvo) {
      _devices[idx].timerEnd = millis() + totalMs;
      _devices[idx].timerAction = alvo;
      Serial.printf("%s Temporizador ativo: %d min e %d seg para %s\n",
                    _deviceLabel(idx), minutes, seconds,
                    alvo ? "Ligar" : "Desligar");
    } else {
      _devices[idx].timerEnd = 0;
      Serial.printf("%s Temporizador pendente: %d min e %d seg para %s "
                    "(iniciara assim que o estado mudar para %s)\n",
                    _deviceLabel(idx), minutes, seconds,
                    alvo ? "Ligar" : "Desligar", alvo ? "DESLIGADO" : "LIGADO");
    }
    return true;
  }

  int idx = _findDevice(topicLower);
  if (idx < 0)
    idx = _findDevice(topic);
  if (idx < 0) {
    Serial.printf("[VollenIoT] Dispositivo nao encontrado: %s\n", topic);
    return false;
  }

  // Verifica se o comando recebido corresponde ao par configurado para este
  // dispositivo
  bool newState = false;
  bool matched = false;

  if (strcmp(message, _devices[idx].comandoOn) == 0) {
    newState = true;
    matched = true;
  } else if (strcmp(message, _devices[idx].comandoOff) == 0) {
    newState = false;
    matched = true;
  }

  if (!matched) {
    // Comando nao pertence a este dispositivo — ignora silenciosamente
    return true;
  }

  setDeviceState(_devices[idx].id, newState);
  Serial.printf("%s %s\n", _deviceLabel(idx),
                newState ? "Ligado" : "Desligado");
  return true;
}

// -----------------------------------------------------------------------------
// Callback MQTT estático
// -----------------------------------------------------------------------------
void VollenIoT::_mqttCallback(char *topic, uint8_t *payload,
                              unsigned int length) {
  if (_instance) {
    _instance->_processMessage(topic, payload, length);
  }
}

// =============================================================================
// VollenIoT.h - Biblioteca para controle de dispositivos Vollen IoT via MQTT
// =============================================================================
//
// Uso:
//   1. Conecte o Wi-Fi com WiFi.begin() no seu sketch
//   2. Adicione dispositivos com addDevice()
//   3. Associe um pino (opcional) com setDevicePin()
//   4. Chame begin() e connect()
//   5. No loop(), chame vollen.loop()
//
// Dependências:
//   - PubSubClient  (Nick O'Leary)
//   - ArduinoJson   (Benoit Blanchon)
// =============================================================================

#ifndef VollenIoT_h
#define VollenIoT_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define Vollen_NO_PIN 255

struct VollenSchedule {
  bool active;
  int hour;
  int minute;
  bool action;          // true = ON, false = OFF
  uint8_t weekDaysMask; // Bit 0=Dom, Bit 1=Seg, ..., Bit 6=Sáb
  bool executedThisMinute;
};

struct VollenDevice {
  const char *id;
  char name[24];
  uint8_t pin;
  uint8_t defaultPin;
  bool state;
  bool active;
  unsigned long timerEnd;
  bool timerAction;
  int pendingMinutes;
  int pendingSeconds;
  bool pendingAction;
  char comandoOff[8];
  char comandoOn[8];
  VollenSchedule schedules[5];
};

class VollenIoT {
public:
  VollenIoT();

  bool addDevice(const char *id, const char *name = nullptr);
  bool setDevicePin(const char *id, uint8_t pin);
  bool setDeviceName(const char *id, const char *name);
  bool setDeviceCommand(const char *id, const char *comandoOff,
                        const char *comandoOn);
  void removeDevice(const char *id);

  void begin();
  void connect();
  void loop();

  void publishStatus(const char *deviceId, const char *status);
  void publishState(const char *deviceId, bool state);
  void publishAllStatus(const char *status);
  void publishAllState(bool state);

  bool isWiFiConnected() const;
  bool isMQTTConnected();
  void setDeviceState(const char *deviceId, bool newState);
  bool getDeviceState(const char *deviceId) const;
  void toggleDevice(const char *deviceId);
  void setDeveloperMode(bool enable);
  bool isDeviceCommandInverted(const char *deviceId);

  void onStateChange(void (*callback)(const char *deviceId, bool state));

  // Configura intervalo do heartbeat (padrão: 30000ms = 30s)
  void setHeartbeatInterval(unsigned long intervalMs);

private:
  char _mqttHost[64];
  uint16_t _mqttPort;
  char _mqttUser[64];
  char _mqttPass[64];

  WiFiClientSecure _wifiClient;
  PubSubClient _mqttClient;

  static const uint8_t MAX_DEVICES = 16;
  VollenDevice _devices[MAX_DEVICES];
  uint8_t _deviceCount;

  void (*_onStateChange)(const char *, bool);

  unsigned long _lastReconnectAttempt;
  unsigned long _lastTimerCheck;
  unsigned long _lastHeartbeat;
  uint32_t _heartbeatInterval;

  uint32_t _bootId;
  bool _duplicateDetected;
  bool _developerMode;

  int _findDevice(const char *id) const;
  bool _addDeviceInternal(const char *id, const char *name, uint8_t pin,
                          bool state);
  void _setupPins();
  void _connectMQTT();
  bool _processMessage(const char *topic, const uint8_t *payload,
                       size_t length);
  const char *_deviceLabel(int idx) const;
  void _checkTimers();

  static void _mqttCallback(char *topic, uint8_t *payload, unsigned int length);
  static VollenIoT *_instance;
};

#endif

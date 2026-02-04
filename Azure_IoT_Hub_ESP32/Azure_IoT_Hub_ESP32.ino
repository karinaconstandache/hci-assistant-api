// Copyright (c) Microsoft Corporation. 
// SPDX-License-Identifier: MIT

/*
 * This is an Arduino-based Azure IoT Hub sample for ESPRESSIF ESP32 boards.
 * (Original description omitted for brevity — unchanged)
 */

// C99 libraries
#include <cstdlib>
#include <string.h>
#include <time.h>

// WiFi & MQTT
#include <WiFi.h>
#include <mqtt_client.h>

// Azure IoT SDK
#include <az_core.h>
#include <az_iot.h>
#include <azure_ca.h>

// Additional headers
#include "AzIoTSasToken.h"
#include "SerialLogger.h"
#include "iot_configs.h"

#define AZURE_SDK_CLIENT_USER_AGENT "c%2F" AZ_SDK_VERSION_STRING "(ard;esp32)"

#define sizeofarray(a) (sizeof(a) / sizeof(a[0]))
#define NTP_SERVERS "pool.ntp.org", "time.nist.gov"
#define MQTT_QOS1 1
#define DO_NOT_RETAIN_MSG 0
#define SAS_TOKEN_DURATION_IN_MINUTES 60
#define UNIX_TIME_NOV_13_2017 1510592825

#define PST_TIME_ZONE -8
#define PST_TIME_ZONE_DAYLIGHT_SAVINGS_DIFF 1
#define GMT_OFFSET_SECS (PST_TIME_ZONE * 3600)
#define GMT_OFFSET_SECS_DST ((PST_TIME_ZONE + PST_TIME_ZONE_DAYLIGHT_SAVINGS_DIFF) * 3600)


#define GREEN_LED_PIN 12
#define RED_LED_PIN 14

// IoT Hub config
static const char* ssid = IOT_CONFIG_WIFI_SSID;
static const char* password = IOT_CONFIG_WIFI_PASSWORD;
static const char* host = IOT_CONFIG_IOTHUB_FQDN;
static const char* mqtt_broker_uri = "mqtts://" IOT_CONFIG_IOTHUB_FQDN;
static const char* device_id = IOT_CONFIG_DEVICE_ID;
static const int mqtt_port = AZ_IOT_DEFAULT_MQTT_CONNECT_PORT;

static esp_mqtt_client_handle_t mqtt_client;
static az_iot_hub_client client;

static char mqtt_client_id[128];
static char mqtt_username[128];
static char mqtt_password[200];
static uint8_t sas_signature_buffer[256];

static unsigned long next_telemetry_send_time_ms = 0;
static uint32_t telemetry_send_count = 0;
static char telemetry_topic[128];
static String telemetry_payload = "{}";

#define INCOMING_DATA_BUFFER_SIZE 128
static char incoming_data[INCOMING_DATA_BUFFER_SIZE];

#ifndef IOT_CONFIG_USE_X509_CERT
static AzIoTSasToken sasToken(
    &client,
    AZ_SPAN_FROM_STR(IOT_CONFIG_DEVICE_KEY),
    AZ_SPAN_FROM_BUFFER(sas_signature_buffer),
    AZ_SPAN_FROM_BUFFER(mqtt_password));
#endif

// -------------------------------------------------------------------
// WiFi / Time / Connection functions (UNMODIFIED)
// -------------------------------------------------------------------

static void connectToWiFi()
{
  Logger.Info("Connecting to WIFI SSID " + String(ssid));
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Logger.Info("WiFi connected, IP address: " + WiFi.localIP().toString());
}

static void initializeTime()
{
  Logger.Info("Setting time using SNTP");
  configTime(GMT_OFFSET_SECS, GMT_OFFSET_SECS_DST, NTP_SERVERS);

  time_t now = time(NULL);
  while (now < UNIX_TIME_NOV_13_2017)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }

  Serial.println("");
  Logger.Info("Time initialized!");
}

// -------------------------------------------------------------------
// UPDATED MQTT EVENT HANDLER
// -------------------------------------------------------------------

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
#else
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
#endif

  switch (event->event_id)
  {
    case MQTT_EVENT_CONNECTED:
      Logger.Info("MQTT Connected");

      // subscribe to cloud-to-device messages
      esp_mqtt_client_subscribe(mqtt_client, AZ_IOT_HUB_CLIENT_C2D_SUBSCRIBE_TOPIC, 1);
      break;

    case MQTT_EVENT_DATA:
    {
      Logger.Info("MQTT_EVENT_DATA");

      // Read incoming payload
      int len = event->data_len;
      if (len >= INCOMING_DATA_BUFFER_SIZE)
          len = INCOMING_DATA_BUFFER_SIZE - 1;

      memcpy(incoming_data, event->data, len);
      incoming_data[len] = '\0';

      String response = String(incoming_data);
      response.toLowerCase();
      response.trim();

      // Remove wrapping quotes if present
      if (response.startsWith("\"")) response = response.substring(1);
      if (response.endsWith("\"")) response = response.substring(0, response.length() - 1);

      Logger.Info("Assistant Response: " + response);

      // ----------------------------------------------------------
      // NEW LOGIC: True = correct answer → green LED
      // ----------------------------------------------------------
      if (response == "true")
      {
          digitalWrite(GREEN_LED_PIN, HIGH);
          digitalWrite(RED_LED_PIN, LOW);
          Logger.Info("Correct → GREEN LED ON");
      }
      else if (response == "false")
      {
          digitalWrite(GREEN_LED_PIN, LOW);
          digitalWrite(RED_LED_PIN, HIGH);
          Logger.Info("Incorrect → RED LED ON");
      }
      else
      {
          Logger.Info("Unexpected value from assistant.");
      }

      break;
    }

    default:
      break;
  }

#if !(defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3)
  return ESP_OK;
#endif
}

// -------------------------------------------------------------------
// IoTHub, MQTT Init, Telemetry (UNMODIFIED)
// -------------------------------------------------------------------

static void initializeIoTHubClient()
{
  az_iot_hub_client_options options = az_iot_hub_client_options_default();
  options.user_agent = AZ_SPAN_FROM_STR(AZURE_SDK_CLIENT_USER_AGENT);

  if (az_result_failed(
        az_iot_hub_client_init(&client,
        az_span_create((uint8_t*)host, strlen(host)),
        az_span_create((uint8_t*)device_id, strlen(device_id)),
        &options)))
  {
    Logger.Error("Failed initializing IoT Hub client");
    return;
  }

  size_t client_id_length;
  az_iot_hub_client_get_client_id(
      &client,
      mqtt_client_id,
      sizeof(mqtt_client_id) - 1,
      &client_id_length);

  az_iot_hub_client_get_user_name(
      &client,
      mqtt_username,
      sizeof(mqtt_username),
      NULL);

  Logger.Info("Client ID: " + String(mqtt_client_id));
}

static int initializeMqttClient()
{
#ifndef IOT_CONFIG_USE_X509_CERT
  if (sasToken.Generate(SAS_TOKEN_DURATION_IN_MINUTES) != 0)
    return 1;
#endif

  esp_mqtt_client_config_t mqtt_config = {};
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  mqtt_config.broker.address.uri = mqtt_broker_uri;
  mqtt_config.broker.address.port = mqtt_port;
  mqtt_config.credentials.client_id = mqtt_client_id;
  mqtt_config.credentials.username = mqtt_username;
  mqtt_config.credentials.authentication.password = (const char*)az_span_ptr(sasToken.Get());
  mqtt_config.broker.verification.certificate = (const char*)ca_pem;
  mqtt_config.broker.verification.certificate_len = (size_t)ca_pem_len;
  mqtt_config.session.keepalive = 30;

#else
  mqtt_config.uri = mqtt_broker_uri;
  mqtt_config.port = mqtt_port;
  mqtt_config.client_id = mqtt_client_id;
  mqtt_config.username = mqtt_username;
  mqtt_config.password = (const char*)az_span_ptr(sasToken.Get());
  mqtt_config.keepalive = 30;
  mqtt_config.event_handle = mqtt_event_handler;
  mqtt_config.cert_pem = (const char*)ca_pem;
#endif

  mqtt_client = esp_mqtt_client_init(&mqtt_config);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
#endif

  esp_mqtt_client_start(mqtt_client);
  return 0;
}

// -------------------------------------------------------------------
// Setup & Loop
// -------------------------------------------------------------------

void setup()
{
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);

  connectToWiFi();
  initializeTime();
  initializeIoTHubClient();
  initializeMqttClient();
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    connectToWiFi();
  }
#ifndef IOT_CONFIG_USE_X509_CERT
  else if (sasToken.IsExpired())
  {
    esp_mqtt_client_destroy(mqtt_client);
    initializeMqttClient();
  }
#endif
}

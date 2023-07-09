#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

SoftwareSerial mySerial(12, 15); // RX, TX

// Definição de constantes
const int PIN_GREEN_LED = 2;
const int PIN_RED_LED = 4;
const int PIN_YELLOW_LED = 14;
const int PIN_BUTTON = 16;
const char* SSID = "ValeFloresResidence";
const char* WIFI_PASSWORD = "9FA705F8F2";
const char* SERVER_HOST = "http://192.168.1.72:5000";
int id = 0;

IPAddress local_IP(192, 168, 1, 161);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

void setup()
{
  Serial.begin(9600);
  while (!Serial);
  delay(100);

  pinMode(PIN_YELLOW_LED, OUTPUT);
  pinMode(PIN_RED_LED, OUTPUT);
  pinMode(PIN_GREEN_LED, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  // Configurar a taxa de dados para a porta serial do sensor
  finger.begin(57600);

  if (finger.verifyPassword()) {
    Serial.println("Sensor de impressão digital encontrado!");
  } else {
    Serial.println("Sensor de impressão digital não encontrado :(");
    while (1) { delay(1); }
  }

  // Conectar ao WiFi
  WiFi.mode(WIFI_STA);
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(SSID, WIFI_PASSWORD);
  Serial.println("A conectar ao WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Conexão WiFi estabelecida");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
}


void loop() {
  int result = getFingerprintIDez();
  switch (result) {
    case FINGERPRINT_OK:
      Serial.println("Impressão digital reconhecida");
      digitalWrite(PIN_GREEN_LED, HIGH);   // acende o LED verde
      delay(2000);                         // espera 2 segundos
      digitalWrite(PIN_GREEN_LED, LOW);    // apaga o LED verde
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println("Nenhuma impressão digital detectada");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Erro de comunicação");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Falha na leitura da impressão digital");
      break;
    case FINGERPRINT_NOTFOUND:
      Serial.println("Impressão digital não reconhecida");
      digitalWrite(PIN_RED_LED, HIGH);     // acende o LED vermelho
      delay(2000);                         // espera 2 segundos
      digitalWrite(PIN_RED_LED, LOW);      // apaga o LED vermelho
      break;
    default:
      Serial.println("Erro desconhecido");
      break;
  }
  if (digitalRead(PIN_BUTTON) == LOW) {
    getFingerprintEnroll();
  }
  delay(200);            // pequena pausa para não sobrecarregar o sensor
}


//Esta função recebe um ID de impressão digital, cria um documento JSON com ele, e envia uma requisição POST para o servidor. A resposta do servidor é lida e impressa no monitor serial.
void createUser(int id) {
  HTTPClient http;
  WiFiClient wifiClient;

  // Cria um documento JSON
  StaticJsonDocument<200> doc;
  doc["fingerprint_code"] = id;

  // Serializa o documento JSON
  String requestBody;
  serializeJson(doc, requestBody);

  http.begin(wifiClient, String(SERVER_HOST) + "/create_user"); // Especifica a URL para a API do teu servidor
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST(requestBody);
  delay(100); // Aguarda um pouco para dar tempo de ler a resposta
  http.end();

  if (httpResponseCode == 201) {
    String response = http.getString();
    Serial.println("Resposta do servidor:");
    Serial.println(response);
    Serial.println("Utilizador criado com sucesso");
  } else if (httpResponseCode == 500) {
    Serial.println("Ocorreu um erro ao criar o utilizador");
  } else if (httpResponseCode == 404) {
    Serial.println("Utilizador não encontrado");
  } else if (httpResponseCode == 200) {
    Serial.println("Porta desbloqueada! Bem-vindo");
  } else if (httpResponseCode == 409) {
    Serial.println("Digital já registado!");
  }
  http.end();
}

//Esta função recebe um ID de impressão digital, verifica se ele existe no sensor e, em seguida, envia uma requisição POST para o servidor para verificar se ele existe na base de dados. A resposta do servidor é lida e impressa no monitor serial.
bool verifyFingerprint(int fingerprintID) {
  // Verifica se a impressão digital existe no sensor
  if (!finger.fingerFastSearch()) {
    Serial.println("Impressão digital não encontrada no sensor");
    return false;
  }

  // Verifica se a impressão digital existe na base de dados
  HTTPClient http;
  WiFiClient wifiClient;

  // Cria um documento JSON
  StaticJsonDocument<200> doc;
  doc["user_id"] = fingerprintID;

  // Serializa o documento JSON
  String requestBody;
  serializeJson(doc, requestBody);

  http.begin(wifiClient, String(SERVER_HOST) + "/unlock"); // Especifica a URL para a API do teu servidor
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST(requestBody);
  delay(100); // Aguarda um pouco para dar tempo de ler a resposta

  if (httpResponseCode == 200) {
    String response = http.getString();
    Serial.println("Resposta do servidor:");
    Serial.println(response);
    Serial.println("Porta desbloqueada! Bem-vindo");
    return true;
  } else if (httpResponseCode == 404) {
    Serial.println("Utilizador não encontrado");
  } else {
    Serial.println("Ocorreu um erro ao verificar a impressão digital");
  }

  http.end();
  return false;
}

//A função getFingerprintIDez() será responsável por ler uma impressão digital do sensor e verificar se ela está registrada
int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return p;
  
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return p;
  
  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  return p;
  
  // Se a impressão digital foi encontrada e reconhecida, verifica se ela está registrada no servidor
  if (verifyFingerprint(finger.fingerID)) {
    return FINGERPRINT_OK;
  } else {
    return FINGERPRINT_NOTFOUND;
  }
}

//Esta função é responsável por registrar uma nova impressão digital no sensor
uint8_t getFingerprintEnroll() {
  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #"); Serial.println(id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID "); Serial.println(id);
  p = -1;
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!s

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  Serial.print("Creating model for #");  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");
    createUser(id); // Adiciona a impressão digital ao servidor
    id++;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  return true;
}

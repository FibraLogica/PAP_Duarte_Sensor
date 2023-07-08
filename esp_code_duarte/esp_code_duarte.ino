#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

#if (defined(__AVR__) || defined(ESP8266)) && !defined(__AVR_ATmega2560__)
SoftwareSerial mySerial(12, 15);
#else
#define mySerial Serial1
#endif

#define pinGreenLed 2
#define pinRedLed 4
#define pinYellowLed 14
#define pinButton 16

int lastButtonState = LOW; // O último estado do botão

IPAddress local_IP(192, 168, 1, 161);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

const char* ssid = "ValeFloresResidence";
const char* password = "9FA705F8F2";
const char* host = "http://192.168.1.70:5000";

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

uint8_t id;

void setup()
{
  Serial.begin(9600);
  while (!Serial);
  delay(100);

  pinMode(pinYellowLed, OUTPUT);
  pinMode(pinRedLed, OUTPUT);
  pinMode(pinGreenLed, OUTPUT);
  pinMode(pinButton, INPUT);
  

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
  WiFi.begin(ssid, password);
  Serial.println("A conectar ao WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Conexão WiFi estabelecida");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

uint8_t readnumber(void) {
  uint8_t num = 0;

  while (num == 0) {
    while (!Serial.available());
    num = Serial.parseInt();
  }
  return num;
}

void loop() {
  // Lê o estado atual do botão
  int buttonState = digitalRead(pinButton);
  // Se o botão foi pressionado
  if (buttonState == LOW) {
    // Registra uma impressão digital com o ID atual
    Serial.print("Registando ID #");
    Serial.println(id);
    while (!getFingerprintEnroll());
    // Incrementa o ID para o próximo registro
    id++;
  } else {
    // Verifica a impressão digital
    int result = getFingerprintID();
    if (result != -1) {
      // Se a impressão digital foi reconhecida, "abre a porta" (acende o LED)
      digitalWrite(pinYellowLed, HIGH);
      delay(5000); // Mantém a "porta" aberta por 5 segundos
      digitalWrite(pinYellowLed, LOW);
    }
  }
}

void createUser(int id) {
  HTTPClient http;
  WiFiClient wifiClient;

  // Cria um documento JSON
  StaticJsonDocument<200> doc;
  doc["fingerprint_code"] = id;

  // Serializa o documento JSON
  String requestBody;
  serializeJson(doc, requestBody);

  http.begin(wifiClient, String(host) + "/create_user"); // Especifica a URL para a API do teu servidor
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

int getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK)  return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK)  return -1;

  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID); 
  Serial.print(" with confidence of "); Serial.println(finger.confidence); 

  return finger.fingerID;
}

uint8_t getFingerprintEnroll()
{
  int p = -1;
  Serial.print("Aguardando um dedo válido para o registo com o ID #"); Serial.println(id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Imagem capturada");
        break;
      case FINGERPRINT_NOFINGER:
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Erro de comunicação");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Erro na captura da imagem");
        break;
      default:
        Serial.println("Erro desconhecido");
        break;
    }
  }

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Imagem convertida");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Imagem muito confusa");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Erro de comunicação");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Não foi possível encontrar características da impressão digital");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Não foi possível encontrar características da impressão digital");
      return p;
    default:
      Serial.println("Erro desconhecido");
      return p;
  }

  Serial.println("Remova o dedo");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID "); Serial.println(id);
  p = -1;
  Serial.println("Coloque o mesmo dedo novamente");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Imagem capturada");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Erro de comunicação");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Erro na captura da imagem");
        break;
      default:
        Serial.println("Erro desconhecido");
        break;
    }
  }

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Imagem convertida");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Imagem muito confusa");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Erro de comunicação");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Não foi possível encontrar características da impressão digital");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Não foi possível encontrar características da impressão digital");
      return p;
    default:
      Serial.println("Erro desconhecido");
      return p;
  }

  Serial.print("Criando modelo para o ID #");  Serial.println(id);
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Impressões digitais correspondem!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Erro de comunicação");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Impressões digitais não correspondem");
    return p;
  } else {
    Serial.println("Erro desconhecido");
    return p;
  }

  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Armazenado!");
    createUser(id);
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Erro de comunicação");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Não foi possível armazenar nessa localização");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Erro ao escrever na memória flash");
    return p;
  } else {
    Serial.println("Erro desconhecido");
    return p;
  }
  return true;
}
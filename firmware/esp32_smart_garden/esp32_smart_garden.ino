const int dhtSensorPin = 18; // pino do sensor de humidade e temperatura ligado ao pino 2 do Arduino
const int relayPin = 19; // Pino de controle do relé ligado ao pino 4 do Arduino
const int lightSensorPin = 34; // Pino do sensor de luminosidade KY-018 ligado ao pino A0 do Arduino
const int waterSensorPin = 35; // Pino do sensor de líquidos sem contacto ligado ao pino A1 do Arduino
const int soilSensorPin = 32; // Pino do sensor de humidade do solo ligado ao pino A2 do Arduino

bool tankEmptySent = false; //Inicializa a variavel tankEmptySent
bool pumpworking = false; //Inicializa a variavel pumpworking

#include <WiFi.h> // Inclui a biblioteca WiFi no programa para permitir a conexão com redes Wi-Fi
#include <DHT.h> // Inclui a biblioteca DHT no programa para permitir a utilização do sensor de humidade e temperatura
#define DHTTYPE DHT11 // Define o tipo de sensor DHT que está a ser utilizado (DHT11 neste caso)
DHT dht(dhtSensorPin, DHTTYPE); // Cria uma instância da biblioteca DHT com o pino do sensor DHT e o tipo definido anteriormente

const char* ssid = "YOUR_WIFI_SSID"; // Define o nome da rede Wi-Fi (SSID) à qual o dispositivo se conectará
const char* password = "YOUR_WIFI_PASSWORD"; // Define a senha da rede Wi-Fi à qual o dispositivo se conectará

void setup() {
  Serial.begin(9600); // Inicia a comunicação serial
  dht.begin(); // Inicia o sensor de humidade e temperatura
  pinMode(waterSensorPin, INPUT); // Define o pino do sensor de líquidos sem contacto como entrada
  pinMode(lightSensorPin, INPUT); // Define o pino do sensor de luminosidade KY-018 como entrada
  pinMode(soilSensorPin, INPUT); // Define o pino do sensor de humidade do solo como entrada
  pinMode(relayPin, OUTPUT); // Define o pino do relé como saída
  
  // Faz a ligação á rede Wifi
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

}

void loop() {
  int soilSensorValue = analogRead(soilSensorPin); // Lê o valor do sensor de humidade do solo
  float voltage = soilSensorValue * (5.0 / 4095.0); // Calcula a voltagem a partir do valor lido do sensor de humidade do solo
  float soilhumidity = (voltage - 0.92) / 0.08; // Calcula a humidade do solo a partir da voltagem lida do sensor de humidade do solo e converte o valor para uma escala de 0 a 100
  int waterSensorValue = digitalRead(waterSensorPin); // Lê o valor do sensor de líquidos sem contacto
  float humidity = dht.readHumidity(); // Lê a humidade relativa do ar a partir do sensor de humidade e temperatura DHT
  float temperature = dht.readTemperature(); // Lê a temperatura a partir do sensor de humidade e temperatura DHT
  int lightSensorValue = analogRead(lightSensorPin); // Lê o valor do sensor de intensidade luminosa
  float lightIntensity = map(lightSensorValue, 0, 4095, 100, 0); // Converte o valor lido do sensor de intensidade luminosa para uma escala de 0 a 100

  Serial.println("--- Medições ---");
    // Mostra o resultado no monitor serial
    Serial.print("Humidade do solo: ");
    Serial.print(soilhumidity, 2);
    Serial.println("%");

    // Verifica se a humidade do solo está abaixo de 30% e existe agua no tanque, caso os dois sejam verdade inicia a rega
    if (soilhumidity < 30) {
      if (waterSensorValue == LOW && tankEmptySent == false) {
        Serial.println("O tanque de água está vazio, não é possível regar a planta.");
        tankEmptySent = true;
         
      }
      if (waterSensorValue == HIGH) {
        digitalWrite(relayPin, HIGH); // Liga o relé
        Serial.println("Regando a planta...");
        tankEmptySent = false;
        pumpworking = true;
      }
    }

    // Verifica se a humidade do solo está acima de 70% ou se já não existe agua no tanque, caso um deles seja verdade para a rega
    if (soilhumidity > 70 || waterSensorValue == LOW) {
      digitalWrite(relayPin, LOW); // Desliga o relé
      Serial.println("Parando de regar a planta...");
      pumpworking = false;
    }

  Serial.print("Sensor de humidade e temperatura: ");
  Serial.print("Humidade = ");
  Serial.print(humidity);
  Serial.print("%, Temperatura = ");
  Serial.print(temperature);
  Serial.println(" ºC");
  Serial.print("Sensor de luminosidade: ");
  Serial.print("Intensidade de luz = ");
  Serial.print(lightIntensity);
  Serial.println("%");

  // Verifica se existe agua no tanque e apresenta o resultado no monitor Serial
  Serial.print("Sensor de líquidos sem contacto: ");
  if (waterSensorValue == LOW) {
    Serial.println("Nenhum líquido detectado");
  } else {
    Serial.println("Líquido detectado");
  }

  delay(1000); // Espera um segundo antes de executar o ciclo novamente
}

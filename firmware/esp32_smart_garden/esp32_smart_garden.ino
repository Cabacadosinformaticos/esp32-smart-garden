const int dhtSensorPin = 18; // pino do sensor de humidade e temperatura ligado ao pino 2 do Arduino
const int relayPin = 19; // Pino de controle do relé ligado ao pino 4 do Arduino
const int lightSensorPin = 34; // Pino do sensor de luminosidade KY-018 ligado ao pino A0 do Arduino
const int waterSensorPin = 35; // Pino do sensor de líquidos sem contacto ligado ao pino A1 do Arduino
const int soilSensorPin = 32; // Pino do sensor de humidade do solo ligado ao pino A2 do Arduino

#include <DHT.h> // Inclui a biblioteca DHT no programa para permitir a utilização do sensor de humidade e temperatura
#define DHTTYPE DHT11 // Define o tipo de sensor DHT que está a ser utilizado (DHT11 neste caso)
DHT dht(dhtSensorPin, DHTTYPE); // Cria uma instância da biblioteca DHT com o pino do sensor DHT e o tipo definido anteriormente

void setup() {
  Serial.begin(9600); // Inicia a comunicação serial
  dht.begin(); // Inicia o sensor de humidade e temperatura
  pinMode(waterSensorPin, INPUT); // Define o pino do sensor de líquidos sem contacto como entrada
  pinMode(lightSensorPin, INPUT); // Define o pino do sensor de luminosidade KY-018 como entrada
  pinMode(soilSensorPin, INPUT); // Define o pino do sensor de humidade do solo como entrada
  pinMode(relayPin, OUTPUT); // Define o pino do relé como saída
}

void loop() {
  float humidity = dht.readHumidity(); // Lê a humidade relativa do ar a partir do sensor de humidade e temperatura DHT
  float temperature = dht.readTemperature(); // Lê a temperatura a partir do sensor de humidade e temperatura DHT

  Serial.print("Sensor de humidade e temperatura: ");
  Serial.print("Humidade = ");
  Serial.print(humidity);
  Serial.print("%, Temperatura = ");
  Serial.print(temperature);
  Serial.println(" ºC");

  delay(1000); // Espera um segundo antes de executar o ciclo novamente
}

const int dhtSensorPin = 18; // pino do sensor de humidade e temperatura ligado ao pino 2 do Arduino
const int relayPin = 19; // Pino de controle do relé ligado ao pino 4 do Arduino
const int lightSensorPin = 34; // Pino do sensor de luminosidade KY-018 ligado ao pino A0 do Arduino
const int waterSensorPin = 35; // Pino do sensor de líquidos sem contacto ligado ao pino A1 do Arduino
const int soilSensorPin = 32; // Pino do sensor de humidade do solo ligado ao pino A2 do Arduino

void setup() {
  Serial.begin(9600); // Inicia a comunicação serial
  pinMode(waterSensorPin, INPUT); // Define o pino do sensor de líquidos sem contacto como entrada
  pinMode(lightSensorPin, INPUT); // Define o pino do sensor de luminosidade KY-018 como entrada
  pinMode(soilSensorPin, INPUT); // Define o pino do sensor de humidade do solo como entrada
  pinMode(relayPin, OUTPUT); // Define o pino do relé como saída
}

void loop() {
  delay(1000);
}

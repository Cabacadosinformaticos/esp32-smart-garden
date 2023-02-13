const int dhtSensorPin = 18; // pino do sensor de humidade e temperatura ligado ao pino 2 do Arduino
const int relayPin = 19; // Pino de controle do relé ligado ao pino 4 do Arduino
const int lightSensorPin = 34; // Pino do sensor de luminosidade KY-018 ligado ao pino A0 do Arduino
const int waterSensorPin = 35; // Pino do sensor de líquidos sem contacto ligado ao pino A1 do Arduino
const int soilSensorPin = 32; // Pino do sensor de humidade do solo ligado ao pino A2 do Arduino

bool notWorkSent = false; //Inicializa a variavel tankEmptySent
bool tankEmptySent = false; //Inicializa a variavel tankEmptySent
bool pumpworking = false; //Inicializa a variavel pumpworking
bool lowTempSent = false; //Inicializa a variavel lowTempSent
bool highTempSent = false; //Inicializa a variavel highTempSent

#include <WiFi.h> // Inclui a biblioteca WiFi no programa para permitir a conexão com redes Wi-Fi
#include <HTTPClient.h> // Inclui a biblioteca HTTPClient no programa para fazer solicitações HTTP a um servidor
#include <WebServer.h> // Inclui a biblioteca WebServer no programa para criar um servidor web que pode ser usado para consultar o dispositivo IoT
#include <UrlEncode.h> // Inclui a biblioteca UrlEncode no programa para codificar URLs para serem enviadas como parâmetros nas solicitações HTTP
#include <DHT.h> // Inclui a biblioteca DHT no programa para permitir a utilização do sensor de humidade e temperatura
#define DHTTYPE DHT11 // Define o tipo de sensor DHT que está a ser utilizado (DHT11 neste caso)
DHT dht(dhtSensorPin, DHTTYPE); // Cria uma instância da biblioteca DHT com o pino do sensor DHT e o tipo definido anteriormente


const char* ssid = "YOUR_WIFI_SSID"; // Define o nome da rede Wi-Fi (SSID) à qual o dispositivo se conectará
const char* password = "YOUR_WIFI_PASSWORD"; // Define a senha da rede Wi-Fi à qual o dispositivo se conectará

// +international_country_code + phone number
// Portugal +351, example: +351912345678 
String phoneNumber = "+351XXXXXXXXX"; // Define o número de telefone que receberá mensagens de texto (número do pais + numero de telefone)
String apiKey = "YOUR_CALLMEBOT_API_KEY"; // Define a chave da API usada para enviar mensagens de texto

WebServer server(80); // Cria um servidor na porta 80

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

  // Define a rota para a página web
  server.on("/", handleRoot);

  // Inicia o servidor web
  server.begin();
  Serial.println("Servidor iniciado");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

// Função que envia uma mensagem de texto para o número de telefone especificado
void sendMessage(String message){

  String url = "https://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&apikey=" + apiKey + "&text=" + urlEncode(message); // Cria a URL com as informações necessárias para enviar a mensagem de texto através da API do CallMeBot
  HTTPClient http; // Inicia uma solicitação HTTP POST usando a URL criada anteriormente
  http.begin(url);

  http.addHeader("Content-Type", "application/x-www-form-urlencoded"); // Define o cabeçalho Content-Type como application/x-www-form-urlencoded
  
  int httpResponseCode = http.POST(url); // Envia a solicitação HTTP POST e armazena o código de resposta HTTP em uma variável
  if (httpResponseCode == 200){ // Verifica se a mensagem foi enviada com sucesso e imprime uma mensagem no monitor serial
    Serial.print("Message sent successfully");
  }
  else{
    Serial.println("Error sending the message");
    Serial.print("HTTP response code: ");
    Serial.println(httpResponseCode);
  }

  http.end(); // Liberta os recursos usados na solicitação HTTP
}

void loop() {

  server.handleClient(); // Processa qualquer cliente que esteja a comunicar com o servidor naquele momento
  
  int soilSensorValue = analogRead(soilSensorPin); // Lê o valor do sensor de humidade do solo
  float voltage = soilSensorValue * (5.0 / 4095.0); // Calcula a voltagem a partir do valor lido do sensor de humidade do solo
  float soilhumidity = (voltage - 0.92) / 0.08; // Calcula a humidade do solo a partir da voltagem lida do sensor de humidade do solo e converte o valor para uma escala de 0 a 100
  int waterSensorValue = digitalRead(waterSensorPin); // Lê o valor do sensor de líquidos sem contacto
  float humidity = dht.readHumidity(); // Lê a humidade relativa do ar a partir do sensor de humidade e temperatura DHT
  float temperature = dht.readTemperature(); // Lê a temperatura a partir do sensor de humidade e temperatura DHT
  int lightSensorValue = analogRead(lightSensorPin); // Lê o valor do sensor de intensidade luminosa
  float lightIntensity = map(lightSensorValue, 0, 4095, 100, 0); // Converte o valor lido do sensor de intensidade luminosa para uma escala de 0 a 100

  Serial.println("--- Medições ---");
  ("Sensor de humidade do solo: ");
  // Verifica se o valor lido pelo sensor de humidade do solo está dentro do intervalo válido (0-100%)
  // Se o valor estiver fora do intervalo, envia uma mensagem de erro e define notWorkSent como true
  // Caso contrário, mostra a humidade do solo e verifica se é necessário regar a planta ou não
  if (soilhumidity < 0 || soilhumidity > 100) {
    
    if (!notWorkSent) {
    
    Serial.println("Erro: Valor de humidade fora do intervalo válido (0-100%)");
    
    sendMessage("O sensor de humidade nao está a funcionar!");
    
    notWorkSent = true;

    }
       
   } else if (soilhumidity >= 0 && soilhumidity <= 100) {
    
    notWorkSent = false;
    
    // Mostra o resultado no monitor serial
    Serial.print("Humidade do solo: ");
    Serial.print(soilhumidity, 2);
    Serial.println("%");

    // Verifica se a humidade do solo está abaixo de 30% e existe agua no tanque, caso os dois sejam verdade inicia a rega
    if (soilhumidity < 30) {
      if (waterSensorValue == LOW && tankEmptySent == false) {
        Serial.println("O tanque de água está vazio, não é possível regar a planta.");
        sendMessage("O tanque de água está vazio, não é possível regar a planta.");
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
  }

  // Apresenta os valores lidos pelos sensores no monitor Serial
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

  // Avisos a enviar caso a planta esteja a enfrentar alguma situação adversa
  // Verifica se a temperatura está muito baixa
  if (temperature < 18 && !lowTempSent) {
    Serial.println("A temperatura está abaixo do ideal para a planta.");
    lowTempSent = true;
     sendMessage("A temperatura está abaixo do ideal para a planta.");
  }
  else if (temperature >= 18 && lowTempSent) {
    lowTempSent = false;
  }

  // Verifica se a temperatura está muito alta
  if (temperature > 26 && !highTempSent) {
    Serial.println("A temperatura está acima do ideal para a planta.");
    highTempSent = true;
     sendMessage("A temperatura está acima do ideal para a planta.");
  }
  else if (temperature <= 26 && highTempSent) {
    highTempSent = false;
  }


  delay(1000); // Espera um segundo antes de executar o ciclo novamente
}

// Função responsável pela página Web
void handleRoot() {
  
  int soilSensorValue = analogRead(soilSensorPin); // Lê o valor do sensor de humidade do solo
  float voltage = soilSensorValue * (5.0 / 4095.0); // Calcula a voltagem a partir do valor lido do sensor de humidade do solo
  float soilhumidity = (voltage - 0.92) / 0.08; // Calcula a humidade do solo a partir da voltagem lida do sensor de humidade do solo e converte o valor para uma escala de 0 a 100
  int waterSensorValue = digitalRead(waterSensorPin); // Lê o valor do sensor de líquidos sem contacto
  float humidity = dht.readHumidity(); // Lê a humidade relativa do ar a partir do sensor de humidade e temperatura DHT
  float temperature = dht.readTemperature(); // Lê a temperatura a partir do sensor de humidade e temperatura DHT
  int lightSensorValue = analogRead(lightSensorPin); // Lê o valor do sensor de intensidade luminosa
  float lightIntensity = map(lightSensorValue, 0, 4095, 100, 0); // Converte o valor lido do sensor de intensidade luminosa para uma escala de 0 a 100
  
  String watertank = ""; // Inicializa a variável watertank como uma string vazia
  String waterpump = ""; // Inicializa a variável waterpump como uma string vazia

  // Verifica se existe agua no tanque
  if (waterSensorValue == LOW) {
    watertank += "Tanque vazio"; // Concatena o texto na variável watertank
  } else {
    watertank += "Tanque com água"; // Concatena o texto na variável watertank
  }

  // Verifica se a bomba de agua está ligada
  if (pumpworking == false) {
    waterpump += "Bomba desligada"; // Concatena o texto na variável waterpump
  } else {
    waterpump += "A regar a planta"; // Concatena o texto na variável waterpump
  }

  // Codigo da página Web
  String html = "<html><head><meta charset='UTF-8'> <title>Horta IoT</title> <meta name='viewport' content='width=device-width, initial-scale=1'> <link rel='icon' href='https://icons.iconarchive.com/icons/toma4025/tea/128/tea-plant-leaf-icon.png'> <link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css'> <style> body { font-family: Arial, sans-serif; background-color: #000000; text-align: center; padding-top: 50px; padding: 20px; background-image: url('https://ensina.rtp.pt/site-uploads/2021/05/movimento_xilemico_plantas_vasculares-854x480.jpg');background-repeat: no-repeat;background-size: cover; } h1 { color: white; font-size: 70px; } .grelha { display: grid; grid-template-columns: 1fr 1fr 1fr ; grid-template-rows: 150px 150px; grid-template-areas: 'temperatura humidade luminosidade' 'humidadeSolo tanque bomba' } .sensor-name { font-size: 24px; font-weight: bold; margin-bottom: 18px; } .sensor-value { font-size: 36px; font-weight: bold; margin-bottom: 10px; } .sensor-reading { background-color: white; border: 1px solid #333; border-radius: 10px; padding: 14px 20px; box-shadow: 2px 2px 5px #ccc; display: flex; flex-direction: column; align-items: center; justify-content: space-around; text-align: center; margin: 10px; } </style></head><body> <h1>Horta IoT</h1> <div class='grelha'> <div class='sensor-reading'> <div class='sensor-name'>Temperatura:</div> <div class='sensor-value'>" + String(temperature) + "ºC</div> </div> <div class='sensor-reading'> <div class='sensor-name'>Humidade:</div> <div class='sensor-value'>" + String(humidity) + "%</div> </div> <div class='sensor-reading'> <div class='sensor-name'>Luminosidade:</div> <div class='sensor-value'>" + String(lightIntensity) + "%</div> </div> <div class='sensor-reading'> <div class='sensor-name'>Humidade do Solo:</div> <div class='sensor-value'>" + String(soilhumidity) + "%</div> </div> <div class='sensor-reading'> <div class='sensor-name'>Tanque de água:</div> <div class='sensor-value'>" + String(watertank) + "</div> </div> <div class='sensor-reading'> <div class='sensor-name'>Bomba de água:</div> <div class='sensor-value'>" + String(waterpump) + "</div> </div> </div></body></html>";

  
  int refreshTime = 1; // Define o tempo em segundos para a atualização da página

  // Envia a página HTML para o cliente com a instrução de atualização automática
  server.sendHeader("Refresh", String(refreshTime));
  server.send(200, "text/html", html);
}

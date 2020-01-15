#include <SoftwareSerial.h>
#include <EEPROM.h>

// pin 10 = RX , pin 11 = TX

SoftwareSerial moduloGSM(10, 11);

//String server_url = "http://189.69.229.230:3333/test";
String server_url = "";
String serial = "1";
String model = "\"Moto\"";
String cliente = "953884989";

// como sempre vou armazenar 15 bytes (ip adress v4), fixei o mapeamento da memoria EEPROM
int adress = 0;
int finalAdress = 15;

String latitude;
String longitude;
//String json = "{\"serial\": 10, \"data\": \"porta quarto aberta\"}";
String json = "{\"serial\": " + serial + ",\"model\": " + model + ",\"latitude\": " + latitude + ",\"longitude\": " + longitude + "}";
String last_command;
String gps_position;
bool flag;
unsigned int seconds = 0;

void HTTP_Init(String APN, String USER, String PASS);
void HTTP_Post(String URL, String DATA);
void HTTP_Close();
String Comando_AT(String Comando);
String readSerial();
void Comando_AT(String Comando, String DATA); // OVERLOAD PARA O COMANDO HTTP POST
void Atribui_LATLONG(String data);
void Start();
void syncGPS();
void syncGSM();
void EEPROMWriteStr(int address, String value);
String EEPROMReadStr(int address);
String readIp();
void enviarSMS(String telefone, String mensagem);
String requestIp(String msg);
String validateIp(String str);
void forceIP();
void flushRX();

void setup()
{
  Serial.begin(9600);
  while (!Serial)
    ;
  moduloGSM.begin(9600);
  pinMode(5, OUTPUT);
  pinMode(3, OUTPUT);

  Start();

  // Configuração do timer1

  TCCR1A = 0;                          //confira timer para operação normal pinos OC1A e OC1B desconectados
  TCCR1B = 0;                          //limpa registrador
  TCCR1B |= (1 << CS10) | (1 << CS12); // configura prescaler para 1024: CS12 = 1 e CS10 = 1

  TCNT1 = 0xC2F7; // incia timer com valor para que estouro ocorra em 1 segundo
  // 65536-(16MHz/1024/1Hz) = 49911 = 0xC2F7

  TIMSK1 |= (1 << TOIE1);

  forceIP();
}

void loop()
{
  // ENVIA OS DADOS DE GPS PARA O SERVIDOR
  String res = Comando_AT("CGPSINF=2");

  Atribui_LATLONG(res);

  HTTP_Init("Tim Connect", "tim", "tim");

  HTTP_Post(server_url, json);
}

///**** Interrupção do TIMER1 a cada 1 seg ***/////
ISR(TIMER1_OVF_vect)
{
  TCNT1 = 0xC2F7;
  seconds++;
  if (seconds > 21600)
  {
    Serial.println("6 Horas !");
    EEPROMWriteStr(0, "estouatualizand");
    forceIP();
    seconds = 0;
  }
}
void forceIP()
{
  String IP = readIp();

  if (IP == "WRONG IP")
  {
    requestIp("Favor apontar o IP do servidor !");
  }
  else
    server_url = "http://" + IP + ":3333/positions";
}
///
String requestIp(String msg)
{
  enviarSMS(cliente, msg);
  while (true)
  {
    if (moduloGSM.available() > 0)
    {
      String SMS = moduloGSM.readString();
      flushRX();
      SMS.trim();
      SMS.toUpperCase();

      String numero = SMS.substring(10, 19);
      String MSG = SMS.substring(48);
      String IP = validateIp(MSG);

      if (SMS.indexOf("+CMT") == -1 || numero != cliente || IP == "WRONG IP")
      {
        enviarSMS(cliente, "Parece que recebi um dado na serial que ñ foi tua mensagem ou está incorreto");
      }
      else if (SMS == "ERROR")
      {
        Comando_AT("CFUN=0");
        syncGSM();
      }
      else
      {
        EEPROMWriteStr(adress, IP);
        break;
      }
    }
  }
}
void flushRX()
{
  while (moduloGSM.available())
    moduloGSM.read();
}
// TESTA SE REALMENTE TEM UM IP ADRESS ESCRITO NA EEPROM ENTRE AS POSIÇOES 0 A 14
void enviarSMS(String telefone, String mensagem)
{
  moduloGSM.println("AT+CMGS=\"" + telefone + "\""); // comando necessario para enviar SMS
  moduloGSM.print(mensagem);                         // Após \n escrevemos a mensagem
  moduloGSM.print((char)26);
  flushRX();
}
String readIp()
{
  String str = EEPROMReadStr(adress);
  return validateIp(str);
}

String validateIp(String str)
{
  int points;
  int index = str.indexOf('.');
  if (index == -1)
    return "WRONG IP";
  else
  {
    String sub = "";
    points++;
    for (int cont = 0; cont < str.length(); cont++)
    {
      if (cont == 0)
        sub = str.substring(index + 1); //inicio é inclusivo, logo descartando o '.';
      else
        sub = sub.substring(index + 1);

      index = sub.indexOf('.');
      if (index != -1)
        points++;
    }
    if (points == 3)
      return str;
    else
      return "WRONG IP";
  }
}

void Start()
{
  digitalWrite(5, HIGH); // ligando o modulo
  delay(1000);

  digitalWrite(5, LOW);

  Comando_AT("CFUN=0");
  delay(10);
  Comando_AT("SAPBR=0,1");

  syncGPS();

  syncGSM();
}

void syncGPS()
{
  int cont = 0;
  bool flag = true;
  String msg = "synchronizing GPS .";

  Comando_AT("CGPSPWR=1");

  while (flag)
  {
    cont++;
    delay(500);
    String res = Comando_AT("CGPSSTATUS?");
    msg += ".";

    Serial.println(msg);

    (res.indexOf("2D") != -1 || res.indexOf("3D") != -1) ? flag = false : flag = true;

    if (cont > 100)
    {
      Comando_AT("CGPSPWR=0");
      delay(10);
      Comando_AT("CGPSPWR=1");
      delay(10);
      cont = 0;
      msg = "synchronizing GPS .";
    }
  }
}

void syncGSM()
{
  int cont = 0;
  bool flag = true;
  String msg = "synchronizing GSM .";

  Comando_AT("CFUN=1");

  while (flag)
  {
    cont++;
    delay(500);
    String res = Comando_AT("CREG?");
    msg += ".";
    Serial.println(msg);
    int index = res.indexOf(',');
    String conect = res.substring(index + 1, index + 2);

    conect == "1" ? flag = false : flag = true;

    if (cont > 100)
    {
      Comando_AT("CFUN=0");
      delay(10);
      Comando_AT("CFUN=1");
      delay(10);
      cont = 0;
      msg = "synchronizing GSM .";
    }
  }
}

///******** MANIPULA OS DADOS RECEBIDOS PELO MODULO GPS E PASSA PARA O FORMATO DE JSON *******//

void Atribui_LATLONG(String data)
{
  String res = Comando_AT("CGPSSTATUS?");

  if (res.indexOf("2D") == -1 && res.indexOf("3D") == -1)
  {
    Start();
    res = Comando_AT("CGPSINF=2");

    data = res;
  }

  int start = data.indexOf('-');
  int separador = data.indexOf(',', start);

  latitude = data.substring(start, separador);

  start = data.indexOf('-', separador);
  separador = data.indexOf(',', start);

  longitude = data.substring(start, separador);

  latitude = "\"" + latitude + "\"";
  longitude = "\"" + longitude + "\"";

  json = "{\"serial\": " + serial + ",\"model\": " + model + ",\"latitude\": " + latitude + ",\"longitude\": " + longitude + "}";
}

//******** OVERLOAD COMANDO AT FUNCTIONS *******////

String Comando_AT(String Comando)
{
  last_command = Comando;
  String retorno;

  moduloGSM.println("AT+" + Comando);

  Comando.indexOf("READ") == -1 ? delay(50) : delay(500);

  retorno = moduloGSM.readString();

  Serial.println(retorno);

  retorno.trim();

  if (retorno.indexOf("ERROR") != -1 && last_command.indexOf("HTTP") != -1)
  {
    HTTP_Close();
  }

  return retorno;
}
//******* THIS ONE JUST EXECUTE HTTP+ACTION COMMAND *****//////**/*/*****

void Comando_AT(String Comando, String DATA) // EXECUTA COMANDO HTTP
{
  last_command = Comando;
  moduloGSM.println("AT+" + Comando);
  delay(10);
  moduloGSM.println(DATA);
  delay(2000);

  String retorno = moduloGSM.readString();

  retorno.trim();

  if (retorno.indexOf("ERROR") != -1 && last_command.indexOf("HTTP") != -1)
  {
    HTTP_Close();
  }

  Serial.println(retorno);
}

//********** HTTP FUNCTIONS ************

void HTTP_Init(String APN, String USER, String PASS) // INICILIAZA O MODULO HTTP DO SIM
{
  String comando;

  String res = Comando_AT("CREG?"); // VERICA SE ESTA REGISTRADO EM ALGUMA REDE
  int index = res.indexOf(',');
  String conect = res.substring(index + 1, index + 2); // EXTRAI O DADO QUE INDICA SE ESTA REGISTRADO OU Ñ

  if (!(conect == "1"))
    syncGSM(); // SE O RESULTADO FOR DIFERENTE DE 1 n ESTaTENTA CONEXÃO AO GSM NOVAMENTE

  comando = "CSTT=" + APN + ',' + USER + ',' + PASS;
  Comando_AT(comando);

  //comando = "CIICR";
  comando = "SAPBR=1,1";
  Comando_AT(comando);

  comando = "SAPBR=2,1";
  Comando_AT(comando);

  comando = "HTTPINIT";
  Comando_AT(comando);

  comando = "HTTPPARA=CID,1";
  Comando_AT(comando);
}
void HTTP_Post(String URL, String DATA) // EXECUTA OS COMANDOS PARA REALIZAR UM METODO POST AO SERVIDOR
{
  String comando;
  String data_gsm;

  comando = "HTTPPARA=URL,";
  Comando_AT(comando + URL);

  comando = "HTTPPARA=CONTENT,application/json";
  Comando_AT(comando);

  int size = DATA.length();
  comando = "HTTPDATA=" + String(size) + ",1000";
  Comando_AT(comando, DATA);

  comando = "HTTPACTION=1";
  Comando_AT(comando);

  flag = true;

  while (flag)
  { // LOOP AGUARDANDO RESPOSTA DO SERVIDOR
    if (moduloGSM.available())
    {
      delay(10);

      data_gsm = moduloGSM.readString();
      Serial.println(data_gsm);
      data_gsm.trim();

      String MSG = data_gsm.substring(48);
      String IP = validateIp(MSG);

      if (IP.length() == 15)
      {
        EEPROMWriteStr(adress, IP);

        HTTP_Close();
        flag = false;
      }
      else if (data_gsm.indexOf("HTTPACTION: 1,200") != -1)
      { // 0 pra testar resposta do metodo GET
        String res = Comando_AT("HTTPREAD");

        res.indexOf("\"flag\":true") != -1 ? digitalWrite(3, HIGH) : digitalWrite(3, LOW);

        HTTP_Close();

        flag = false;
      } // SE HOUVE ALGUM DESSES HTTP CODE STATUS
      else if (data_gsm.indexOf("+HTTPACTION: 1,601,0") != -1 || data_gsm.indexOf("+HTTPACTION: 1,408,0") != -1 || data_gsm.indexOf("+HTTPACTION: 1,604,0") != -1)
      {
        HTTP_Close();
        flag = false;
      } // SE NÃO TRAVOU E DEPOIS DE UM TEMPO OCIOSO O GSM RENICIALIZA E DISPARA A FLAG DEACT E SAIMOS DO LOOP
      else if (data_gsm.indexOf("DEACT") != -1)
      {
        Comando_AT("HTTPTERM");
        flag = false;
      }
    }
  }
}
// FECHA E INDICA QUE IRA COMEÇAR OUTRA CONEXÃO HTTP AO MODULO
// SE RECEBER ERROR DE AMBOS OS PARAMETROS, HOUVE TRAVAMENTO NO GSM, RESTARTA
void HTTP_Close()
{
  String res1 = Comando_AT("HTTPTERM");
  delay(10);
  String res2 = Comando_AT("HTTPINIT");

  if (res1.indexOf("ERROR") != -1 && res2.indexOf("ERROR") != -1)
  {
    Comando_AT("CFUN=0");
    delay(10);
    Comando_AT("SAPBR=0,1");
    syncGSM();
  }
  //  Comando_AT("SAPBR=0,1");
  //  delay(200);
}

// MANIPULAÇÃO DA EEPROM
void EEPROMWriteStr(int initAddress, String value)
{
  String msg = "Escrevendo.";
  Serial.print(msg);
  for (int index = 0; index < value.length(); index++)
  {
    Serial.print(".");
    EEPROM.update(initAddress + index, value.charAt(index));
    if (index >= 1000)
      break;
  }
}
String EEPROMReadStr(int readAddress)
{
  String readStr = "";
  char readByte;
  //int readAddress = address;

  do
  {
    readByte = EEPROM.read(readAddress); // retorna Byte lido na posição de memoria
    readStr += readByte;                 // concatena o caracter lido na String readStr(vetor de caracteres)
    readAddress++;                       // proxima posição a ser lida
  }                                      //while ( (readByte != (char)0) && (readAddress < (address + 1000)) );  condição original
  while (readAddress < 16);

  return readStr;
}

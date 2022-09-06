/* esp32vgacalendar - Calendar with VGA output for esp32
    Copyright (C) 2022 Eduardo Furiati

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
    
#include <WiFi.h>
#include "fabgl.h" // http://www.fabglib.org/
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <ESPAsyncWiFiManager.h> //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include <NTPClient.h>
#include <IRsend.h>
#include <TimeLib.h>
#include <LITTLEFS.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <esp_task_wdt.h>
#include "soc/rtc_wdt.h"
#include <Adafruit_BMP280.h>
#include <ESPmDNS.h>

#define PHILIPS_LENGHT 20
DNSServer dns;

IRsend *irsend;  // Set the GPIO to be used to sending the message.

fabgl::VGA4Controller DisplayController;
fabgl::Terminal       Terminal;
fabgl::Canvas         canvas(&DisplayController);

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -10800, 60000);
//GMT Time Zone with sign
#define GMT_TIME_ZONE -3  // change to your timezone


String lerconf(String configuracao);
#include "ptbr.h"  // english en.h or portuguese ptbr.h
#include "funcoesauxiliares.h"

AsyncWebServer server(80);

int habilitado = 1;  //enable or disable the vga output

unsigned long startMillis = 0;        //
unsigned long currentMillis;
const long interval = 60000;
unsigned long previousMillisrx = 0;
unsigned long previousMillistemp = 0;
const long intervalrx = 1000;
int intervaltemp = 5000;
unsigned long previousMillis = 0;
int period = 5000;

// Version control
#define versao  "2"
#define subversao  "0"
#define dataversao  "V. 1.0 06/09/2022 09:59h"
const char compile_date[] = __DATE__ " " __TIME__;

#include "synctime.h"

String temperatura;
Adafruit_BMP280 bmp;

// print the menu in html
String imprimemenu(byte nivelmenu, String usuario) {
  String menu = "";
  String ativof, nivelf, usuariof, ativo, nivel;
  int erro = 0, quant = 0;
  usuario.trim();

  File usuariosf = LITTLEFS.open("/usuarios.txt", "r");
  if (!usuariosf) {
    erro = 1;
  }
  else {
    quant = 0;
    while (usuariosf.available()) {
      ativof = usuariosf.readStringUntil('\n');
      nivelf = usuariosf.readStringUntil('\n');
      usuariof = usuariosf.readStringUntil('\n');
      quant++;
      usuariof.trim();
      if (usuariof == usuario) {
        ativo = ativof;
        nivel = nivelf;
      }
    }
  }
  nivel.trim();
  usuariosf.close();

  menu = "<h1>";
  menu += lerconf("NOMEESP");
  menu += "</h1><h2> [ <a href='/'> ";
  menu += String(Estado);
  menu += "</a> | <a href='/ligadesliga'>";
  menu += String(LigaDesliga);
  menu += "</a> | ";
  menu += "<a href='/avisos'>";
  menu += String(Avisos);
  menu += "</a> | ";
  menu += "<a href='/feriados'>";
  menu += String(Feriados);
  menu += "</a> | ";
  menu += "<a href='/aniversarios'>";
  menu += String(Aniversarios);
  menu += "</a> ";

  if (nivel == 0) {
    menu += " | <a href='/configuracao'>";
    menu += String(Configuracao);
    menu += "</a> | ";
    menu += "<a href='/reboot'>";
    menu += String(Reiniciar);
    menu += "</a> ] <br>";
    for (int xx = 0; xx < 52; xx++) {
      menu += "&nbsp;";
    }
    menu += "<a href='/programacao'>";
    menu += String(Programacao);
    menu += "</a>  | <a href = '/arquivos.html'>";
    menu += String(Arquivos);
    menu += "</a>  | <a href = '/usuarios'>";
    menu += String(Usuarios);
    menu += "</a>";
  }
  menu += "</h2>";
  return menu;
}

// Print the table, 0 = begin, 1 = close the table
String imprimetabela(byte iniciofim, String nomemenu)
{
  String tabela = "";
  if (iniciofim == 0)
  {
    tabela += "<table cellspacing = '2' style = 'background-color: #f0f0f0; border-bottom: 1px solid gray;";
    tabela += "border-left: 1px solid gray; border-right: 1px solid gray; border-top: 1px solid gray;'>";
    tabela += "<tr style = 'border-bottom: 1px solid gray;'><td colspan = '8' style = 'font-weight : bold;";
    tabela += "text-align: center; font-size: 12pt; border-bottom: 1px solid gray; background-color: #b7dbff;'>";
    tabela += String(nomemenu);
    tabela += "</td></tr>";
    return tabela;
  }
  if (iniciofim == 1)
  {
    tabela = "</td></tr></table></div>"; // fecha a tabela
    return tabela;
  }
}

// Print the column
String imprimecoluna(byte quant, byte numcoluna, byte colunaspan) {
  // botao salvar
  String coluna = "";
  if (quant == numcoluna and colunaspan > 0) {
    coluna = "<tr><td align = 'center' colspan = ";
    coluna += String(colunaspan);
    coluna += ">";
  }
  if (quant == numcoluna and colunaspan == 0 ) {
    coluna = "<td> ";
  }

  if (quant > numcoluna)
  {
    // se for a primeira coluna, left e gray
    if (numcoluna == 1) {
      coluna = "<td style = 'border-right: 1px solid #f0f0f0;'> ";
    }
    else {
      coluna = "<td> ";
    }
  }
  return coluna;
}

// Status Page
void handleRoot(AsyncWebServerRequest *request) {
  String cookie;
  if (request->hasHeader("Cookie")) {
    cookie = request->header("Cookie");
    if (cookie.substring(0, 13)  == "ESPSESSIONID=0") {
      request->redirect("/login");
    }
  }
  else {
    request->redirect("/login");
  }
  String usuario, p;
  // https://github.com/me-no-dev/ESPAsyncWebServer/issues/441
  int PozStart;
  int Length;
  int PozStop ;
  String StringName =  "ESPSESSIONID=";
  Length = StringName.length() ;
  PozStart = cookie.indexOf(StringName) + Length ;
  PozStop =  cookie.indexOf(";" , PozStart );
  usuario = cookie.substring(PozStart, PozStop);
  if (usuario == "") {
    request->redirect("/login");
  }

  String htmlpage = cabecalho(usuario);

  htmlpage += imprimemenu(1, usuario);

  // imprime cabecalho da tabela:
  htmlpage += imprimetabela(0, String(Estado));

  htmlpage += imprimecoluna(1, 1, 2);
  if (estadotv()) {
    htmlpage += "<font color = red>[";
    htmlpage += String(Ligado);
    htmlpage += "]";
  }
  else {
    htmlpage += "<font color = green>[";
    htmlpage += String(Desligado);
    htmlpage += "]";
  }
  htmlpage += "</font></td></tr>";

  // First LIne
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += String(Memoria);
#ifdef LANG_pt
  htmlpage += ".........:";
#endif

#ifdef LANG_en
  htmlpage += " .............:";
#endif

  htmlpage += " </td><td>Ram Free: [";
  htmlpage += String(ESP.getFreeHeap());
  htmlpage += "]</td></tr>";

  // linha 2
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += String(Temperatura);
#ifdef LANG_pt
  htmlpage += "....:";
#endif

#ifdef LANG_en
  htmlpage += " .:";
#endif

  htmlpage += " </td><td>";
  htmlpage += temperatura;
  htmlpage += "</td></tr>";
  // fim da linha 2

  // linha 3
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += String(Versao);
#ifdef LANG_pt
  htmlpage += "..............:";
#endif

#ifdef LANG_en
  htmlpage += " .........:";
#endif

  htmlpage += " </td><td>";

  htmlpage += "V. " + String(versao) + "." + String(subversao) + " Data: " + String(dataversao) + "&nbsp;" + String(Compilado) + ": " + String(compile_date);
  htmlpage += "</td></tr>";
  htmlpage +=   imprimetabela(1, "");
  htmlpage += "<BR> Logs: <br>";
  File logs = LITTLEFS.open("/log.txt", "r");
  while (logs.available()) {
    String p = logs.readStringUntil('\n');
    htmlpage += p;
    htmlpage += "<br>";
  }
  logs.close();
  request->send(200, "text/html", htmlpage);
}

// config form
void handleconfiguracao(AsyncWebServerRequest *request) {
  String cookie;
  if (request->hasHeader("Cookie")) {
    cookie = request->header("Cookie");
    if (cookie.substring(0, 13)  == "ESPSESSIONID=0") {
      request->redirect("/login");
    }
  }
  else {
    request->redirect("/login");
  }
  String usuario, p;
  // https://github.com/me-no-dev/ESPAsyncWebServer/issues/441
  int PozStart;
  int Length;
  int PozStop ;
  String StringName =  "ESPSESSIONID=";
  Length = StringName.length() ;
  PozStart = cookie.indexOf(StringName) + Length ;
  PozStop =  cookie.indexOf(";" , PozStart );
  usuario = cookie.substring(PozStart, PozStop);
  if (usuario == "") {
    request->redirect("/login");
  }

  String htmlpage = cabecalho(usuario);

  htmlpage += imprimemenu(1, usuario);

  // imprime cabecalho da tabela:
  htmlpage +=   imprimetabela(0, String(Configuracao));

  File config;
  byte gravou = 0;
  byte confmdns = 0;
  String senhaint, mdns, nomeesp;
  int i;
  String patrimonio;
  int sensortemp, sensortemppino, pinoldr, pinoir, r1, g1, b1, vsync, hsync;

  for (uint8_t i = 0; i < request->args(); i++) {
    if (request->argName(i) == "CONFMDNS") {
      mdns = request->arg(i);
      gravou = 1;
    }
    if (request->argName(i) == "SENHAINT") {
      senhaint = request->arg(i);
      gravou = 1;
    }
    if (request->argName(i) == "SENSORTEMP") {
      sensortemp = request->arg(i).toInt();
      gravou = 1;
    }
    if (request->argName(i) == "SENSORTEMPPINO") {
      sensortemppino = request->arg(i).toInt();
      gravou = 1;
    }

    if (request->argName(i) == "NOMEESP") {
      nomeesp = request->arg(i);
      gravou = 1;
    }
    if (request->argName(i) == "PINOLDR") {
      pinoldr = request->arg(i).toInt();
      gravou = 1;
    }
    if (request->argName(i) == "PINOIR") {
      pinoir = request->arg(i).toInt();
      gravou = 1;
    }
    if (request->argName(i) == "PATRIMONIO") {
      patrimonio = request->arg(i);
      gravou = 1;
    }
    if (request->argName(i) == "R1") {
      r1 = request->arg(i).toInt();
      gravou = 1;
    }
    if (request->argName(i) == "G1") {
      g1 = request->arg(i).toInt();
      gravou = 1;
    }
    if (request->argName(i) == "B1") {
      b1 = request->arg(i).toInt();
      gravou = 1;
    }
    if (request->argName(i) == "VSYNC") {
      vsync = request->arg(i).toInt();
      gravou = 1;
    }
    if (request->argName(i) == "HSYNC") {
      hsync = request->arg(i).toInt();
      gravou = 1;
    }
  }
  if (gravou) {
    // gravamos as alteracoes
    LITTLEFS.remove("/config.txt");
    config = LITTLEFS.open("/config.txt", "w");
    config.print("mdns=");
    config.println(mdns);
    config.print("senhaint=");
    config.println(senhaint);
    config.print("sensortemp=");
    config.println(sensortemp);
    config.print("sensortemppino=");
    config.println(sensortemppino);
    config.print("nomeesp=");
    config.println(nomeesp);
    config.print("pinoldr=");
    config.println(pinoldr);
    config.print("pinoir=");
    config.println(pinoir);
    config.print("patrimonio=");
    config.println(patrimonio);
    config.print("r1=");
    config.println(r1);
    config.print("g1=");
    config.println(g1);
    config.print("b1=");
    config.println(b1);
    config.print("vsync=");
    config.println(vsync);
    config.print("hsync=");
    config.println(hsync);
  }
  byte erro = 0;
  String temp;
  config = LITTLEFS.open("/config.txt", "r");
  if (!config) {
    erro = 1;
  }
  else {
    if (config.available()) {
      p = config.readStringUntil('\n');
      mdns = p.substring(5);
      p = config.readStringUntil('\n');
      senhaint = p.substring(9);
      senhaint.trim();
      p = config.readStringUntil('\n');
      sensortemp = p.substring(11).toInt();
      p = config.readStringUntil('\n');
      sensortemppino = p.substring(15).toInt();

      p = config.readStringUntil('\n');
      nomeesp = p.substring(8);

      p = config.readStringUntil('\n');
      pinoldr = p.substring(8).toInt();
      p = config.readStringUntil('\n');
      pinoir = p.substring(7).toInt();
      p = config.readStringUntil('\n');
      patrimonio = p.substring(11).toInt();
      p = config.readStringUntil('\n');
      r1 = p.substring(3).toInt();
      p = config.readStringUntil('\n');
      g1 = p.substring(3).toInt();
      p = config.readStringUntil('\n');
      b1 = p.substring(3).toInt();
      p = config.readStringUntil('\n');
      vsync = p.substring(6).toInt();
      p = config.readStringUntil('\n');
      hsync = p.substring(6).toInt();
    }
  }

  htmlpage += "<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/configuracao\"> \
              </center></td></tr> \
              </td></tr>";
  // mDNS
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += String(mDNS);
#ifdef LANG_pt
  htmlpage += " .........................:";
#endif

#ifdef LANG_en
  htmlpage += " ........................:";
#endif
  htmlpage += " </td>";

  // coluna 2
  htmlpage += imprimecoluna(2, 2, 0);
  htmlpage += "<select name = 'CONFMDNS'>";
  int n = 0;
  for (n = 0; n <= 1; n++) {
    if (n == mdns.toInt()) // ajeitar
    {
      htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
    }
    else {
      htmlpage += "<option value='";
    }
    htmlpage += String(n);
    htmlpage += "'>";
    if (n == 0) htmlpage += String(Nao);
    if (n == 1) htmlpage += String(Sim);
    htmlpage += "</option>";
  }
  htmlpage += "</select></td></tr>"; // fim da linha1 coluna 2
  // fim mDNS

  // senha integracao
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += String(Senhaintegracao);
#ifdef LANG_pt
  htmlpage += " .........:";
#endif

#ifdef LANG_en
  htmlpage += " .:";
#endif
  htmlpage += " </td>";

  // coluna 2
  htmlpage += imprimecoluna(2, 2, 0);
  htmlpage += "<input type='TEXT' size='8' maxsize='8' name='SENHAINT' id='SENHAINT' placeholder='password' value=\"";
  htmlpage += senhaint;
  htmlpage += "\"><br></td></tr>";
  // fim senha integracao

  // url ligar
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += "URL";
#ifdef LANG_pt
  htmlpage += " .............................:";
#endif

#ifdef LANG_en
  htmlpage += " ...........................:";
#endif
  htmlpage += "</td>";

  // coluna 2
  htmlpage += imprimecoluna(2, 2, 0);
  //htmlpage += "<input type='TEXT' size='20' maxsize='20' name='urlligar' id='urlligar' placeholder='URL LIGAR'><br>\n";

  htmlpage += "http://" + WiFi.localIP().toString() + "/ligar?senha=";
  htmlpage += senhaint;
  htmlpage += "&mode=[0 " + String(Desligado) + "][1 " + String(Ligado) + "]</td></tr>"; // fim da linha1 coluna 2
  // fim url ligar

  // url ligar
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += "URL Estado";
#ifdef LANG_pt
  htmlpage += " .................:";
#endif

#ifdef LANG_en
  htmlpage += " ...............:";
#endif
  htmlpage += " </td>\n";
  htmlpage += imprimecoluna(2, 2, 0);
  htmlpage += "http://" + WiFi.localIP().toString() + "/ligar?senha=";
  htmlpage += senhaint;
  htmlpage += "&nbsp; retorno: [estadotv 0=" + String(Desligado) + ",1=" + String(Ligado) + "]</td></tr>";

  // nome do esp32
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += String(NomedoEsp32);
#ifdef LANG_pt
  htmlpage += " ...........:";
#endif

#ifdef LANG_en
  htmlpage += " ..............:";
#endif
  htmlpage += " </td>";

  // coluna 2
  htmlpage += imprimecoluna(2, 2, 0);
  htmlpage += "<input type='TEXT' size='8' maxsize='8' name='NOMEESP' id='NOMEESP' value=\"";
  htmlpage += nomeesp;
  htmlpage += "\"><br></td></tr>";

  // pino ldr
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += String(PinoLDR);
#ifdef LANG_pt
  htmlpage += " .....................:";
#endif

#ifdef LANG_en
  htmlpage += " .....................:";
#endif
  htmlpage += " </td>";
  htmlpage += imprimecoluna(2, 2, 0);
  htmlpage += "<select name = 'PINOLDR'";
  htmlpage += ">";
  for (n = 13; n <= 18; n++) {
    if (n == pinoldr) // ajeitar
    {
      htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
    }
    else {
      htmlpage += "<option value='";
    }
    htmlpage += String(n);
    htmlpage += "'>";
    htmlpage += String(n);
    htmlpage += "</option>";
  }
  htmlpage += "</select></td></tr>";
  // fim pino emissor ldr

  // pino emissor ir
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += String(PinoIR);
#ifdef LANG_pt
  htmlpage += " .........................:";
#endif

#ifdef LANG_en
  htmlpage += " ........................:";
#endif
  htmlpage += " </td>";
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += "<select name = 'PINOIR'>";
  for (n = 4; n <= 17; n++) {
    if (n == pinoir) // ajeitar
    {
      htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
    }
    else {
      htmlpage += "<option value='";
    }
    htmlpage += String(n);
    htmlpage += "'>";
    htmlpage += String(n);
    htmlpage += "</option>";
  }
  htmlpage += "</select></td></tr>";

  // pinos rgbhsyncvsync
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += String(PinosVGA);
#ifdef LANG_pt
  htmlpage += " ...................:";
#endif

#ifdef LANG_en
  htmlpage += " ..................:";
#endif
  htmlpage += " </td>";
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += "R1: <select name = 'R1'>";
  for (n = 4; n <= 29; n++) {
    if (n < 6 or n > 11 ) {
      if (n == r1 ) // ajeitar
      {
        htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
      }
      else {
        htmlpage += "<option value='";
      }
      htmlpage += String(n);
      htmlpage += "'>";
      htmlpage += String(n);
      htmlpage += "</option>";
    }
  }
  htmlpage += "</select>";

  // r2
  htmlpage += "&nbsp;G1: <select name = 'G1'>";
  for (n = 4; n <= 29; n++) {
    if (n == g1) // ajeitar
    {
      htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
    }
    else {
      htmlpage += "<option value='";
    }
    htmlpage += String(n);
    htmlpage += "'>";
    htmlpage += String(n);
    htmlpage += "</option>";
  }
  htmlpage += "</select>";
  // fim G1

  // B1
  htmlpage += "&nbsp;B1: <select name = 'B1'>";
  for (n = 4; n <= 29; n++) {
    if (n == b1) // ajeitar
    {
      htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
    }
    else {
      htmlpage += "<option value='";
    }
    htmlpage += String(n);
    htmlpage += "'>";
    htmlpage += String(n);
    htmlpage += "</option>";
  }
  htmlpage += "</select>";
  // fim B1

  // HSYNC
  htmlpage += "&nbsp;HSYNC: <select name = 'HSYNC'>";
  for (n = 4; n <= 29; n++) {
    if (n == hsync) // ajeitar
    {
      htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
    }
    else {
      htmlpage += "<option value='";
    }
    htmlpage += String(n);
    htmlpage += "'>";
    htmlpage += String(n);
    htmlpage += "</option>";
  }
  htmlpage += "</select>";
  // fim HSYNC

  // VSYNC
  htmlpage += "&nbsp;VSYNC: <select name = 'VSYNC'>";
  for (n = 4; n <= 29; n++) {
    if (n == vsync) // ajeitar
    {
      htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
    }
    else {
      htmlpage += "<option value='";
    }
    htmlpage += String(n);
    htmlpage += "'>";
    htmlpage += String(n);
    htmlpage += "</option>";
  }
  htmlpage += "</select>";
  // fim VSYNC

  htmlpage += "</td></tr>";
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += String(Sensortemp);

#ifdef LANG_pt
  htmlpage += " .:";
#endif

#ifdef LANG_en
  htmlpage += " ..:";
#endif

  htmlpage += "</td>\n";
  // coluna 2
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += "<select name = 'SENSORTEMP'>\n";

  for (n = 0; n < 2; n++) {
    if (n == sensortemp) // ajeitar
    {
      htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
    }
    else {
      htmlpage += "<option value='";
    }
    htmlpage += String(n);
    htmlpage += "'>";
    if (n == 0) htmlpage += String(Nao);
    if (n == 1) htmlpage += "BMP280";
    htmlpage += "</option>";
  }
  htmlpage += "</select></td></tr>";

  // fim sensor temperatura

  // Patrimonioac
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += String(Patrimonio);
#ifdef LANG_pt
  htmlpage += " ....................: </td>\n";
#endif

#ifdef LANG_en
  htmlpage += " .............................: </td>\n";
#endif

  // coluna 2
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += "<input type='TEXT' size='15' maxsize='15' name='PATRIMONIO' id='PATRIMONIO' value=\"";
  htmlpage += patrimonio;
  htmlpage += "\"><br></td></tr>";
  // fim patrimonio

  // submit
  htmlpage += imprimecoluna(2, 2, 2);
  htmlpage += " <center> <input type = \"submit\" value=\"" + String(Salvar) + "\"></center></form>";
  //fim da tabela
  htmlpage +=   imprimetabela(1, "");
  htmlpage += "</body></html>";
  request->send(200, "text/html", htmlpage);
}

void handleReboot(AsyncWebServerRequest *request) {
  String cookie;
  if (request->hasHeader("Cookie")) {
    cookie = request->header("Cookie");
    if (cookie.substring(0, 14)  == "ESPSESSIONID=0") {
      request->redirect("/login");
    }
  }
  else {
    request->redirect("/login");
  }

  String htmlpage = cabecalho(cookie.substring(24));
  htmlpage += imprimemenu(1, cookie.substring(24));

  htmlpage += imprimetabela(0, String(Reiniciar));

  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += String(Reiniciando) + "... </td></tr> ";
  htmlpage += imprimetabela(1, "");

  htmlpage += "</body></html>";
  request->send(200, "text/html", htmlpage);
  delay(2000);
  ESP.restart();
}

void mostravga() {
  Terminal.clear();
  String esp32name = lerconf("NOMEESP");
  Terminal.println(esp32name);
  Terminal.println("");
  int n = 0;
  int nod, dia, cnt, d = 1, x1 = 1, y1 = 1, isNote = 0, y = 0, x = 1;
#ifdef LANG_pt
  Terminal.print("    \e[43m\e[34m" + String(Dom) + "\e[44m\e[33m     " + String(Seg) + "     " +  String(Ter) + "     " + String(Qua) + "     " + String(Qui) + "     " + String(Sex) + "     \e[43m\e[34m");
  Terminal.print("S");
  Terminal.write(225);
  Terminal.println("b\e[40m\e[34m");
#endif

#ifdef LANG_en
  Terminal.print("    \e[43m\e[34m" + String(Dom) + "\e[44m\e[33m     " + String(Seg) + "     " +  String(Ter) + "     " + String(Qua) + "     " + String(Qui) + "     " + String(Sex) + "     \e[43m\e[34m");
  Terminal.print(String(Sab) + "\e[40m\e[34m");
#endif

  nod = getNumberOfDays(month(), year());
  dia = getDayNumber(01, month(), year());
  switch (dia) { //locates the starting day in calender
    case 0 :
      x = 0;
      break;
    case 1 :
      x = 1;
      break;
    case 2 :
      x = 2;
      break;
    case 3 :
      x = 3;
      break;
    case 4 :
      x = 4;
      break;
    case 5 :
      x = 5;
      break;
    case 6 :
      x = 6;
      break;
  }
  int numpontos = 0;
  int proxlinha = 0;
  // espacamento lado esquerdo
  Terminal.print("    ");

  // inicio do dia 01 ou seja o dia da semana que vai comecar o mes dom, seg, ter, etc
  for (int qu = 0; qu < x; qu++) {
    Terminal.print("        ");
  }

  if (feriado(d, month()) == 1) {
  }

  // verifica se e sabado ou domingo
  if (getDayNumber(d, month(), year()) == 0 ) {
    Terminal.println("");
    Terminal.print("\e[43m");  // vermelho, Domingo
  }
  else {
    if (getDayNumber(d, month(), year()) == 6 ) {
      Terminal.print("\e[43m");  // vermelho, Sabado
    }
  }
  if (d == day()) {
    Terminal.print("\e[41m");  // hoje fundo verde
  }

  Terminal.print(" 0");
  Terminal.print(String(d));

  if (feriado(d, month()) == 1) {
    Terminal.print("\e[43m");  // feriado/anotacao
  }

  // \e[41m = verde
  //
  // imprime do dia 2 até o dia 28,29,30 ou 31
  Terminal.print("\e[40m");
  for (d = 2; d <= nod; d++) {
    if (getDayNumber(d, month(), year()) == 0 ) {
      Terminal.println("");
      Terminal.print("    \e[43m");  // vermelho, Domingo
    }
    else {
      if (getDayNumber(d, month(), year()) == 6 ) {
        Terminal.print("     \e[43m");  // vermelho, Sabado
      }
      else {

        // verifica se e feriado
        if (feriado(d, month()) == 1) {
          Terminal.print("     \e[43m");  // feriado, vermelho
        }
        else {
          if (d == day()) {
            Terminal.print("     \e[41m");  // hoje fundo verde
          }
          else {
            Terminal.print("     \e[40m");  // dia normal
          }
        }
      }
    }
    if (d < 10) {
      Terminal.print(" 0");
    }
    else {
      Terminal.print(" ");
    }
    Terminal.print(String(d));
    Terminal.print("\e[40m"); // fundo preto novamente
  }

  // fim dia 2 ate o dia 28,29,30 ou 31

  // Imprime feriados
  Terminal.println("");
  Terminal.println("");

  Terminal.print("\e[43m" + String(Feriados) + ":\e[40m " );
  String p;
  int mesi = 0;

  // mostra os feriados do mes na tela vga
  File feriadotxt = LITTLEFS.open("/feriado.txt", "r");
  while (feriadotxt.available()) {
    p = feriadotxt.readStringUntil('\n');
    mesi = p.substring(5, 7).toInt();
    if (mesi == month()) {
      Terminal.print("\e[40m" + p.substring(1, 3) + "-" + p.substring(12, p.length() - 1) + "  ");
    }
  }

  Terminal.println("");
  Terminal.println("");

  feriadotxt.close();
  n = 0;

  // birthdays
#ifdef LANG_pt
  Terminal.print("\e[43mAniversariantes do m");
  Terminal.write(234); // e circunflexo
  Terminal.print("s:\e[40m ");
#endif

#ifdef LANG_en
  Terminal.print("\e[43mBirthdays:\e[40m ");
#endif
  File anivtxt = LITTLEFS.open("/aniv.txt", FILE_READ);
  int mesanivtxt = 1;
  while (anivtxt.available()) {
    p = anivtxt.readStringUntil('\n');
    if (mesanivtxt == month()) {
      Terminal.println(p);
      n++;
    }
    mesanivtxt++;
  }
  // fim mostra os aniversariantes

  if (n == 0) Terminal.println("");
  Terminal.println("");
  int corfrente, corfundo, quebra;
  String aviso;
  // mostra os avisos/cursos

  //cabecalho \e43m e \e42m -> fundo vermelho
  // \e34m -> cor de frente branca
  // \e40m -> cor de fundo preta
  // \e44m -> cor de fundo branca
  // \e33m -> cor
  // "\e[32m"); // cor de frente vermelha
  // \e[43m\e[34mDom\e[44m\e[33m     Seg


  Terminal.println("\e[43m" + String(Avisos) + " " + String(E) + " " + String(Cursos) + "\e[40m ______________________________________________");
  File avisostxt = LITTLEFS.open("/avisos.txt", FILE_READ);
  while (avisostxt.available()) {
    p = avisostxt.readStringUntil('\n');
    corfrente = p.substring(1, 2).toInt(); // [1][1][0][dfas
    corfundo = p.substring(4, 5).toInt();
    quebra = p.substring(7, 8).toInt();
    aviso = p.substring(18);
    if (corfrente == 1) Terminal.print("\e[34m"); // cor de frente branca
    if (corfrente == 2) Terminal.print("\e[33m"); // cor de frente vermelha
    if (corfrente == 3) Terminal.print("\e[31m"); // cor de frente verde ok
    if (corfrente == 4) Terminal.print("\e[30m"); // cor de frente

    if (corfundo == 1) Terminal.print("\e[44m"); // fundo branco
    if (corfundo == 2) Terminal.print("\e[43m"); // fundo vermelho ok
    if (corfundo == 3) Terminal.print("\e[41m"); // fundo verde
    if (corfundo == 4) Terminal.print("\e[40m"); // cor de fundo preta ok
    Terminal.println(aviso);
    if (quebra) Terminal.println("");
  }
  avisostxt.close();
  Terminal.print("\e[34m\e[40m");
}

// Notes form
void handleavisos(AsyncWebServerRequest * request) {
  String cookie;
  if (request->hasHeader("Cookie")) {
    cookie = request->header("Cookie");
    if (cookie.substring(0, 13)  == "ESPSESSIONID=0") {
      request->redirect("/login");
    }
  }
  else {
    request->redirect("/login");
  }
  String usuario, p;
  // https://github.com/me-no-dev/ESPAsyncWebServer/issues/441
  int PozStart;
  int Length;
  int PozStop ;
  String StringName =  "ESPSESSIONID=";
  Length = StringName.length() ;
  PozStart = cookie.indexOf(StringName) + Length ;
  PozStop =  cookie.indexOf(";" , PozStart );
  usuario = cookie.substring(PozStart, PozStop);
  if (usuario == "") {
    request->redirect("/login");
  }

  String htmlpage = cabecalho(usuario);

  htmlpage += imprimemenu(1, usuario);

  // imprime cabecalho da tabela:
  htmlpage += imprimetabela(0, String(Avisos));

  File avisosf;
  byte gravou = 0;
  byte i;
  String corfrente[6];
  String corfundo[6];
  String quebra[6];
  String aviso[6];
  byte n = 0, quant = 0;
  byte erro = 0;
  int reiniciar = 0;
  String mensagemdeerro = "";
  String dia[6], mes[6];

  for (uint8_t i = 0; i < request->args(); i++) {
    p = "CORFRENTE" + String(quant);
    if (request->argName(i) == p) {
      corfrente[quant] = request->arg(i);
      gravou = 1;
    }

    p = "CORFUNDO" + String(quant);
    if (request->argName(i) == p) {
      corfundo[quant] = request->arg(i);
      gravou = 1;
    }

    p = "QUEBRA" + String(quant);
    if (request->argName(i) == p) {
      quebra[quant] = request->arg(i);
      gravou = 1;
    }

    p = "AVISO" + String(quant);
    if (request->argName(i) == p) {
      aviso[quant] = request->arg(i);
      gravou = 1;
    }

    p = "DIA" + String(quant);
    if (request->argName(i) == p) {
      dia[quant] = request->arg(i);
      gravou = 1;
    }

    p = "MES" + String(quant);
    if (request->argName(i) == p) {
      mes[quant] = request->arg(i);
      gravou = 1;
    }
    if (i == 5 or i == 11 or i == 17 or i == 23 or i == 29) quant++;
  }

  if (gravou) {
    reiniciar = 1;
    // gravamos as alteracoes
    LITTLEFS.remove("/avisos.txt");
    avisosf = LITTLEFS.open("/avisos.txt", "w");
    for (i = 0; i < quant; i++) {
      if (aviso[i] != "") {
        if (corfrente[i] == corfundo[i]) mensagemdeerro += "<br>" + String(Errcorfrentefundo) + " " + String(i);
        avisosf.print("[");
        avisosf.print(String(corfrente[i]));
        avisosf.print("][");
        avisosf.print(String(corfundo[i]));
        avisosf.print("][");
        avisosf.print(String(quebra[i]));
        avisosf.print("][");
        if (dia[i] == "0") {
          avisosf.print("00");
        }
        else {
          avisosf.print(dia[i]);
        }
        avisosf.print("][");
        if (mes[i] == "0") {
          avisosf.print("00");
        }
        else {
          avisosf.print(mes[i]);
        }
        avisosf.print("][");
        avisosf.println(aviso[i]);
      }
    }
    avisosf.close();
  }
  avisosf = LITTLEFS.open("/avisos.txt", "r");
  if (!avisosf) {
    erro = 1;
  }
  else {
    quant = 0;
    while (avisosf.available() and quant < 6) {
      p = avisosf.readStringUntil('\n');
      corfrente[quant] = p.substring(1, 2).toInt(); // [1][1][0][dfas
      corfundo[quant] = p.substring(4, 5).toInt();
      quebra[quant] = p.substring(7, 8).toInt();
      dia[quant] = p.substring(10, 12);
      mes[quant] = p.substring(14, 16);
      aviso[quant] = p.substring(18);
      quant++;
    }
  }

  htmlpage += "<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/avisos\">";

  // cabecalho
  htmlpage += imprimecoluna(4, 1, 0);
  htmlpage += " </td><td>" + String(Aviso) + "</td><td>" + String(CorFrente) + "</td><td>" + String(CorFundo) + "</td><td>" + String(Quebradelinha) + "</td><td>" + String(Dia) + "</td><td>" + String(Mes) + "</td></tr>";

  for (i = 0; i < quant; i++) {
    htmlpage += imprimecoluna(4, 1, 0);
    htmlpage += String(Aviso) + String(i) + " ..: </td><td><input type=\"text\" size=\"48\" name=\"AVISO";
    htmlpage += String(i);
    htmlpage += "\" value=\"";
    htmlpage += String(aviso[i]);
    htmlpage += "\"></td>\n";

    htmlpage += imprimecoluna(4, 2, 0);
    htmlpage += "<select name = 'CORFRENTE";
    htmlpage += String(i);
    htmlpage += "'>\n";
    for (n = 1; n <= 4; n++) {
      if (n == corfrente[i].toInt()) // ajeitar
      {
        htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
      }
      else {
        htmlpage += "<option value = '";
      }
      htmlpage += String(n);
      htmlpage += "'>";
      if (n == 1) htmlpage += String(Branco);
      if (n == 2) htmlpage += String(Vermelho);
      if (n == 3) htmlpage += String(Verde);
      if (n == 4) htmlpage += String(Preto);
      htmlpage += " </option>";
    }
    htmlpage += " </select></td>"; // fim da linha1 coluna 2
    // fim

    // coluna 3
    htmlpage += imprimecoluna(4, 3, 0);
    htmlpage += "<select name = 'CORFUNDO";
    htmlpage += String(i);
    htmlpage += "'>\n";
    for (n = 1; n <= 4; n++) {
      if (n == corfundo[i].toInt()) // ajeitar
      {
        htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
      }
      else {
        htmlpage += "<option value = '";
      }
      htmlpage += String(n);
      htmlpage += "'>";
      if (n == 1) htmlpage += String(Branco);
      if (n == 2) htmlpage += String(Vermelho);
      if (n == 3) htmlpage += String(Verde);
      if (n == 4) htmlpage += String(Preto);
      htmlpage += " </option>";
    }
    htmlpage += " </select></td>"; // fim da linha1 coluna 2
    // fim

    // coluna 4
    htmlpage += imprimecoluna(4, 3, 0);
    htmlpage += "<select name = 'QUEBRA";
    htmlpage += String(i);
    htmlpage += "'>\n";
    for (n = 0; n <= 1; n++) {
      if (n == quebra[i].toInt()) // ajeitar
      {
        htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
      }
      else {
        htmlpage += "<option value = '";
      }
      htmlpage += String(n);
      htmlpage += "'>";
      if (n == 0) htmlpage += String(Nao);
      if (n == 1) htmlpage += String(Sim);
      htmlpage += " </option>";
    }
    htmlpage += "</select></td>\n";

    htmlpage += imprimecoluna(5, 5, 0);
    htmlpage += "<select name = 'DIA";
    htmlpage += String(i);
    htmlpage += "' ";
    htmlpage += ">\n";
    for (n = 0; n <= 31; n++) {
      if (n == dia[i].toInt()) // ajeitar
      {
        htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
      }
      else {
        htmlpage += "<option value = '";
      }
      if (n < 10 ) htmlpage += "0";
      htmlpage += String(n);
      htmlpage += "'>";
      if (n > 0 and n < 10) htmlpage += "0";
      if (n == 0) htmlpage += "-";
      if (n > 0) htmlpage += String(n);
      htmlpage += " </option>";
    }
    htmlpage += " </select></td>"; // fim da linha1 coluna 2

    htmlpage += imprimecoluna(5, 5, 0);
    htmlpage += "<select name = 'MES";
    htmlpage += String(i);
    htmlpage += "' ";
    htmlpage += ">\n";
    for (n = 0 ; n <= 12; n++ ) {
      if (n == mes[i].toInt()) // ajeitar
      {
        htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
      }
      else {
        htmlpage += "<option value = '";
      }
      if (n < 10 ) htmlpage += "0";
      htmlpage += String(n);
      htmlpage += "'>";
      if (n > 0 and n < 10) htmlpage += "0";
      if (n == 0) htmlpage += "-";
      if (n > 0) htmlpage += String(n);
      htmlpage += " </option>";
    }
    htmlpage += " </select></td></tr>"; // fim da linha1 coluna 2
    htmlpage += "</tr>";
  }

  if (quant < 5) {
    // novo campo para adicao somente
    htmlpage += imprimecoluna(4, 1, 0);
    htmlpage += String(Aviso) + String(i) + " ..: </td><td><input type=\"text\" size=\"48\" name=\"AVISO";
    htmlpage += String(i) + "\" value=\"";
    htmlpage += String(aviso[i]);
    htmlpage += "\"></td>\n";

    htmlpage += imprimecoluna(4, 2, 0);
    htmlpage += "<select name = 'CORFRENTE";
    htmlpage += String(i);
    htmlpage += "'>\n";
    htmlpage += "<option selected = 'selected' value= '1'>" + String(Branco) + "</option>"; // e a opcao do config, seleciona
    htmlpage += "<option value = '2'>" + String(Vermelho) + "</option>";
    htmlpage += "<option value = '3'>" + String(Verde) + "</option>";
    htmlpage += "<option value = '4'>" + String(Preto) + "</option></select></td>";

    // coluna 3
    htmlpage += imprimecoluna(4, 3, 0);
    htmlpage += "<select name = 'CORFUNDO";
    htmlpage += String(i);
    htmlpage += "'><option selected = 'selected' value= '4'>" + String(Preto) + "</option>"; // e a opcao do config, seleciona
    htmlpage += "<option value = '2'>" + String(Vermelho) + "</option>";
    htmlpage += "<option value = '3'>" + String(Verde) + "</option>";
    htmlpage += "<option value = '1'>" + String(Branco) + "</option></select></td>";

    // coluna 3
    htmlpage += imprimecoluna(4, 3, 0);
    htmlpage += "<select name = 'QUEBRA";
    htmlpage += String(i);
    htmlpage += "'><option selected = 'selected' value= '0'>N&atilde;o</option> \
                 <option value = '1'>" + String(Sim) + "</option> \
                 </select></td>"; // fim da linha1 coluna 2

    htmlpage += imprimecoluna(5, 5, 0);
    htmlpage += "<select name = 'DIA";
    htmlpage += String(i);
    htmlpage += "'><option selected = 'selected' value = '0'>-</option>";
    for (n = 1; n <= 31; n++) {
      htmlpage += "<option value = '";
      if (n < 10 ) htmlpage += "0";
      htmlpage += String(n);
      htmlpage += "'>";
      if (n < 10 ) htmlpage += "0";
      htmlpage += String(n);
      htmlpage += " </option>";
    }
    htmlpage += " </select></td>"; // fim da linha1 coluna 2

    htmlpage += imprimecoluna(5, 5, 0);
    htmlpage += "<select name = 'MES";
    htmlpage += String(i);
    htmlpage += "'><option selected = 'selected' value = '0'>-</option>";
    for (n = month(); n <= 12; n++) {
      htmlpage += "<option value = '";
      if (n < 10 ) htmlpage += "0";
      htmlpage += String(n);
      htmlpage += "'>";
      if (n < 10 ) htmlpage += "0";
      htmlpage += String(n);
      htmlpage += " </option>";
    }
    htmlpage += " </select></td></tr>"; // fim da linha1 coluna 2
  }

  // fim novo campo para adicao somente

  // submit
  htmlpage += imprimecoluna(5, 5, 5);
  htmlpage += "<input type = \"submit\" value=\"" + String(Salvar) + "/" + String(Mostrarnatv) + "\"></form>";
  //fim da tabela
  htmlpage +=   imprimetabela(1, "");
  htmlpage += "<br>" + String(AdicionarAviso) + "<br>" + String(RemoverAviso);

  if (mensagemdeerro) {
    htmlpage += "<font color=\"red\">" + mensagemdeerro + "</font>";
  }
  htmlpage += "</body></html>";

  request->send(200, "text/html", htmlpage);
  if (reiniciar)  mostravga();
}

void handleaniversarios(AsyncWebServerRequest * request) {
  String cookie;
  if (request->hasHeader("Cookie")) {
    cookie = request->header("Cookie");
    if (cookie.substring(0, 14)  == "ESPSESSIONID=0") {
      request->redirect("/login");
    }
  }
  else {
    request->redirect("/login");
  }

  // seleciona o cookie correto ESPSESSIONID=
  String debug = "";
  int espl = cookie.indexOf("ESPSESSIONID=");

  String htmlpage = cabecalho(cookie.substring(espl + 13));

  htmlpage += imprimemenu(1, cookie.substring(espl + 13));
  int quant = 0;
  int numregistros = 0;
  //int ativo = 0;
  //int novo = 0;
  int reiniciar = 0;

  quant = request->args();

  if (quant) {
    reiniciar = 1;
    // preenche avisoos inicial
    LITTLEFS.remove("/aniv.txt");
    File anivtxt = LITTLEFS.open("/aniv.txt", FILE_APPEND);
    quant = request->args();
    for (uint8_t i = 0; i < request->args(); i++) {
      //message += " " + String(i) + "->" + request->argName(i) + ": " + request->arg(i) + "<br>";
      if (request->argName(i) == "aniv0") {
        anivtxt.println(request->arg(i));
      }
      if (request->argName(i) == "aniv1") {
        anivtxt.println(request->arg(i));
      }
      if (request->argName(i) == "aniv2") {
        anivtxt.println(request->arg(i));
      }
      if (request->argName(i) == "aniv3") {
        anivtxt.println(request->arg(i));
      }
      if (request->argName(i) == "aniv4") {
        anivtxt.println(request->arg(i));
      }
      if (request->argName(i) == "aniv5") {
        anivtxt.println(request->arg(i));
      }
      if (request->argName(i) == "aniv6") {
        anivtxt.println(request->arg(i));
      }
      if (request->argName(i) == "aniv7") {
        anivtxt.println(request->arg(i));
      }
      if (request->argName(i) == "aniv8") {
        anivtxt.println(request->arg(i));
      }
      if (request->argName(i) == "aniv9") {
        anivtxt.println(request->arg(i));
      }
      if (request->argName(i) == "aniv10") {
        anivtxt.println(request->arg(i));
      }
      if (request->argName(i) == "aniv11") {
        anivtxt.println(request->arg(i));
      }
      if (request->argName(i) == "aniv12") {
        anivtxt.println(request->arg(i));
      }
    }
    anivtxt.close();
  }

  File anivtxt = LITTLEFS.open("/aniv.txt", FILE_READ);
  String s, p;
  int i = 0;

  // imprime cabecalho da tabela:
  htmlpage +=   imprimetabela(0, "Aniversariantes");
  // coluna 1
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += String(Mes);
  htmlpage += "</td>\n";

  htmlpage += imprimecoluna(2, 2, 0);
  htmlpage += String(Aniversarios) + " </td></tr>\n";

  numregistros++;
#ifdef LANG_pt
  String nomemes[13] = {"zer", "Jan", "Fev", "Mar", "Abr", "Mai", "Jun", "Jul", "Ago", "Set", "Out", "Nov", "Dez"};
#endif

#ifdef LANG_en
  String nomemes[13] = {"zer", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
#endif

  for (int meses = 1; meses < 13; meses++) {
    htmlpage += "<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/aniversarios\"> ";
    htmlpage += imprimecoluna(2, 1, 0);
    htmlpage += nomemes[meses];
    // coluna1 fim
    htmlpage += "</td>\n";
    htmlpage += imprimecoluna(2, 2, 0);

    htmlpage += "<input type=\"text\" name=\"aniv";
    htmlpage += String(meses);
    htmlpage += "\" size=\"50\" maxsize=\"50\" value=\"";
    if (anivtxt.available()) p = anivtxt.readStringUntil('\n');
    htmlpage += p;
    htmlpage += "\"> </td></tr>\n";
    i++;
  }

  htmlpage += "<tr><td align='center' colspan=\"2\"><input type = \"submit\" value=\"" + String(Salvar) + "\"></form></td></tr>";

  //table end
  htmlpage +=   imprimetabela(1, "");

  htmlpage += "</body> </html>";
  request->send(200, "text/html", htmlpage);
  anivtxt.close();
  delay(3000);
  if (reiniciar)  ESP.restart();
}

void handlearquivos(AsyncWebServerRequest * request) {
  String cookie;
  if (request->hasHeader("Cookie")) {
    cookie = request->header("Cookie");
    if (cookie.substring(0, 13)  == "ESPSESSIONID=0") {
      request->redirect("/login");
    }
  }
  else {
    request->redirect("/login");
  }
  String usuario, p;
  // https://github.com/me-no-dev/ESPAsyncWebServer/issues/441
  int PozStart;
  int Length;
  int PozStop ;
  String StringName =  "ESPSESSIONID=";
  Length = StringName.length() ;
  PozStart = cookie.indexOf(StringName) + Length ;
  PozStop =  cookie.indexOf(";" , PozStart );
  usuario = cookie.substring(PozStart, PozStop);
  if (usuario == "") {
    request->redirect("/login");
  }

  String htmlpage = cabecalho(usuario);

  htmlpage += imprimemenu(1, usuario);

  // imprime cabecalho da tabela:
  htmlpage += imprimetabela(0, String(Arquivos));

  // linha 1
  htmlpage += imprimecoluna(2, 1, 0);

  File root = LITTLEFS.open("/");
  File file = root.openNextFile();
  htmlpage += String(Bytesusados) + ": " + String(LITTLEFS.usedBytes()) + "</td></tr>\n";
  htmlpage += imprimecoluna(2, 1, 0);
  htmlpage += String(Arquivo) + "</td><td>" + String(Tamanho) + "</td></tr>";

  while (file) {
    htmlpage += imprimecoluna(2, 1, 0);
    htmlpage += "<a href='/download.html?arquivo=";
    htmlpage += String(file.name()) + "'>";
    htmlpage += String(file.name()) + "</a> </td> <td>" + String(file.size()) + "</td></tr>\n";
    file = root.openNextFile();
  }

  htmlpage += " </td> </tr> \n";  // fim do row status
  htmlpage += imprimetabela(1, "");

  //htmlpage += dataehora();
  htmlpage += " </body> </html>";
  request->send(200, "text/html", htmlpage);
}

void handledownload(AsyncWebServerRequest * request) {
  String message = "File Not Found\n\n";
  String arquivo = "";

  for (uint8_t i = 0; i < request->args(); i++) {
    message += " " + request->argName(i) + ": " + request->arg(i) + "\n";
    if (request->argName(i) == "arquivo") {
      arquivo = request->arg(i);
    }
  }
  request->send(LITTLEFS, arquivo, String(), true);
}

void handleligar(AsyncWebServerRequest * request) {
  String htmlpage = "";
  int gravou = 0;
  int modo = 0;
  int temmodo = 0;
  String senha;

  for (uint8_t i = 0; i < request->args(); i++) {
    if (request->argName(i) == "senha") {
      senha = request->arg(i);
      gravou = 1;
    }
    if (request->argName(i) == "mode") {
      modo = request->arg(i).toInt();
      gravou = 1;
      temmodo = 1;
    }
  }

  if (gravou and temmodo) {
    if (modo == 0) { // desligar
      if (estadotv()) {  // se estiver ligada, envia sinal
        logs(String(Solicitado) + " " + String(Desligar));
        desligartv();
      }
    }
    else { // senao e desligar
      if (!estadotv()) { // se estiver desligada, envia sinal
        logs(String(Solicitado) + " " + String(Ligar));
        ligartv();
      }
    }
  }
  else { // retorna a informacao solicitada
    if (estadotv()) {
      htmlpage += "1";
    }
    else {
      htmlpage += "0";
    }
  }
  request->send(200, "text/html", htmlpage);
}

void handleferiados1(AsyncWebServerRequest * request) {
  String cookie;
  if (request->hasHeader("Cookie")) {
    cookie = request->header("Cookie");
    if (cookie.substring(0, 14)  == "ESPSESSIONID=0") {
      request->redirect("/login");
    }
  }
  else {
    request->redirect("/login");
  }

  int espl = cookie.indexOf("ESPSESSIONID=");

  String htmlpage = cabecalho(cookie.substring(espl + 13));

  File feriadosf, vgaf;
  byte gravou = 0;
  byte i, desabilita = 0, habilita = 0;
  byte dia[15];
  byte mes[15];
  byte fixo[15];
  String feriado[15];
  byte n = 0, quant = 0;
  byte erro = 0, habilitado = 0;
  String p;

  htmlpage += imprimemenu(1, cookie.substring(espl + 13));
  // verifica se está desabilitado o vga
  vgaf = LITTLEFS.open("/vga.txt", "r");
  while (vgaf.available()) {
    p = vgaf.readStringUntil('\n');
    habilitado = p.substring(1, 2).toInt();
  }
  vgaf.close();
  // fim verifica se está desabilitado o vga

  for (uint8_t i = 0; i < request->args(); i++) {
    p = "DIA" + String(quant);
    if (request->argName(i) == p) {
      dia[quant] = request->arg(i).toInt();
      gravou = 1;
    }

    p = "MES" + String(quant);
    if (request->argName(i) == p) {
      mes[quant] = request->arg(i).toInt();
      gravou = 1;
    }

    p = "FIXO" + String(quant);
    if (request->argName(i) == p) {
      fixo[quant] = request->arg(i).toInt();
      gravou = 1;
    }

    p = "FERIADO" + String(quant);
    if (request->argName(i) == p) {
      feriado[quant] = request->arg(i);
      gravou = 1;
      if (quant < 15 ) quant++;
    }

    p = "desabilita";
    if (request->argName(i) == p) {
      desabilita = request->arg(i).toInt();
    }
    p = "habilita";
    if (request->argName(i) == p) {
      habilita = request->arg(i).toInt();
    }
  }

  if (habilita) {
    LITTLEFS.remove("/vga.txt");
    vgaf = LITTLEFS.open("/vga.txt", "w");
    vgaf.println("[1]");
    vgaf.close();
    htmlpage += "Aguarde 10s e Recarregue a pagina";
    request->send(200, "text/html", htmlpage);
  }

  if (desabilita) {
    LITTLEFS.remove("/vga.txt");
    vgaf = LITTLEFS.open("/vga.txt", "w");
    vgaf.println("[0]");
    vgaf.close();
    htmlpage += "Aguarde 10s e Recarregue a pagina";
    request->send(200, "text/html", htmlpage);

  }

  if (habilitado == 0 or desabilita == 1) {
    if (gravou) {
      // gravamos as alteracoes
      LITTLEFS.remove("/feriado.txt");
      feriadosf = LITTLEFS.open("/feriado.txt", "w");
      for (i = 0; i < quant; i++) {
        if (feriado[i] != "") {
          feriadosf.print("[");
          if (dia[i] < 10) feriadosf.print("0");
          feriadosf.print(dia[i]);
          feriadosf.print("][");
          if (mes[i] < 10) feriadosf.print("0");
          feriadosf.print(mes[i]);
          feriadosf.print("][");
          feriadosf.print(fixo[i]);
          feriadosf.print("][");
          feriadosf.println(feriado[i]);
        }
      }
      feriadosf.close();
    }
    feriadosf = LITTLEFS.open("/feriado.txt", "r");
    if (!feriadosf) {
      erro = 1;
    }
    else {
      quant = 0;
      while (feriadosf.available() and quant < 15) {
        p = feriadosf.readStringUntil('\n');
        dia[quant] = p.substring(1, 3).toInt();
        mes[quant] = p.substring(5, 7).toInt();
        fixo[quant] = p.substring(9, 10).toInt();
        feriado[quant] = p.substring(12, 60);
        quant ++;
      }
    }

    // imprime cabecalho da tabela:
    htmlpage +=   imprimetabela(0, "Feriados");
    htmlpage += "<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/feriados\">";

    // cabecalho
    htmlpage += imprimecoluna(4, 1, 0);
    htmlpage += " </td><td>";
    htmlpage += String(Dia);
    htmlpage += "</td><td>" + String(Mes) + "</td><td>";
    htmlpage += String(Fixo);
    htmlpage += "</td><td>";
    htmlpage += String(Feriado);
    htmlpage += "</td></tr>";

    for (i = 0; i < quant; i++) {
      htmlpage += imprimecoluna(4, 1, 0);
      htmlpage += String(Feriado) + String(i) + " .......: </td>\n";
      //
      htmlpage += imprimecoluna(4, 2, 0);
      htmlpage += "<select name = 'DIA";
      htmlpage += String(i);
      htmlpage += "'>\n";
      for (n = 1; n <= 31; n++) {
        if (n == dia[i]) // ajeitar
        {
          htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
        }
        else {
          htmlpage += "<option value = '";
        }
        htmlpage += String(n);
        htmlpage += "'>";
        if (n < 10) htmlpage += "0";
        htmlpage += String(n);
        htmlpage += " </option>";
      }
      htmlpage += " </select></td>"; // fim da linha1 coluna 2
      // fim

      // coluna 3
      htmlpage += imprimecoluna(4, 3, 0);
      htmlpage += "<select name = 'MES";
      htmlpage += String(i);
      htmlpage += "'>\n";
      for (n = 1; n <= 12; n++) {
        if (n == mes[i]) // ajeitar
        {
          htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
        }
        else {
          htmlpage += "<option value = '";
        }
        htmlpage += String(n);
        htmlpage += "'>";
        if (n < 10) htmlpage += "0";
        htmlpage += String(n);
        htmlpage += " </option>";
      }
      htmlpage += " </select></td>"; // fim da linha1 coluna 2
      // fim

      // repetir
      htmlpage += imprimecoluna(5, 4, 0);
      htmlpage += "<select name = 'FIXO";
      htmlpage += String(i);
      htmlpage += "'>\n";
      for (n = 0; n <= 1; n++) {
        if (n == fixo[i])
        {
          htmlpage += "<option selected = 'selected' value= '"; // e a opcao do config, seleciona
        }
        else {
          htmlpage += "<option value = '";
        }
        htmlpage += String(n);
        htmlpage += "'>";
        if (n == 0) htmlpage += String(Nao);
        if (n == 1) htmlpage += String(Sim);

        htmlpage += " </option>";
      }
      htmlpage += " </select></td>"; // fim da linha1 coluna 2
      // fim repetir

      // coluna 5 link
      htmlpage += imprimecoluna(5, 5, 0);
      htmlpage += "<INPUT TYPE='TEXT' name='FERIADO";
      htmlpage += String(i);
      htmlpage += "' SIZE=30 MAXLENGTH=40 value='";
      htmlpage += String(feriado[i]);
      htmlpage += "'></td></select></td></tr>";
    } // fim quantidade de pinos no arquivo


    // novo campo para adicao somente
    htmlpage += imprimecoluna(5, 1, 0);
    htmlpage += String(Feriado) + String(quant) + " .......: </td>\n";
    //
    htmlpage += imprimecoluna(5, 2, 0);
    htmlpage += "<select name = 'DIA";
    htmlpage += String(i);
    htmlpage += "'>\n";
    for (n = 1; n <= 31; n++) {
      if (n == 1) // ajeitar
      {
        htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
      }
      else {
        htmlpage += "<option value = '";
      }
      htmlpage += String(n);
      htmlpage += "'>";
      if (n < 10) htmlpage += "0";
      htmlpage += String(n);
      htmlpage += " </option>";
    }
    htmlpage += " </select></td>";
    // fim

    // coluna 3
    htmlpage += imprimecoluna(5, 3, 0);
    htmlpage += "<select name = 'MES";
    htmlpage += String(i);
    htmlpage += "'>\n";
    for (n = 1; n <= 12; n++) {
      if (n == 1) // ajeitar
      {
        htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
      }
      else {
        htmlpage += "<option value = '";
      }
      htmlpage += String(n);
      htmlpage += "'>";
      if (n < 10) htmlpage += "0";
      htmlpage += String(n);
      htmlpage += " </option>";
    }
    htmlpage += " </select></td>";
    // fim

    // repetir
    htmlpage += imprimecoluna(5, 4, 0);
    htmlpage += "<select name = 'FIXO";
    htmlpage += String(quant);
    htmlpage += "'>\n";
    htmlpage += " <option selected = 'selected' value= '0'>" + String(Nao) + "</option> \
                  <option value= '1'>" + String(Sim) + "</option> \
                  </select></td>";
    // fim repetir

    // coluna 5 link
    htmlpage += imprimecoluna(5, 5, 0);
    htmlpage += "<INPUT TYPE='TEXT' name='FERIADO";
    htmlpage += String(i);
    htmlpage += "' SIZE=30 MAXLENGTH=40 value=''></td></select></td></tr>";
    // fim novo campo para adicao

    // submit
    htmlpage += imprimecoluna(5, 5, 5);
    htmlpage += "<input type = \"submit\" value=\"" + String(Salvar) + "\"></form>";
    //fim da tabela
    htmlpage +=   imprimetabela(1, "");

    htmlpage += "<a href=\"/feriados?habilita=1\">" + String(HabilitarVga) + "</a>";
    htmlpage += "<br>";
    htmlpage += String(AdicionarFeriado);
  }
  else {
    htmlpage += "<a href=\"/feriados?desabilita=1\">" + String(DesabilitarVga) + "</a>";
  }
  request->send(200, "text/html", htmlpage);
  delay(3000);
  if (desabilita == 1 or habilita == 1 ) {
    ESP.restart();
  }
}

void ligartask(void *pvParameters) {
  int sair = 0;
  int cont = 0 ;
  // pino do l1dr
  String temps = lerconf("PINOLDR");
  byte pinoldr = temps.toInt();
  if (digitalRead(pinoldr) == 0) { // so tenta ligar se estiver desligada
    while (sair == 0) {
      // send on signal
      irsend->sendRC6(0x0C, PHILIPS_LENGHT, 2);
      // test if if really on
      if (cont < 5 ) {
        cont++;
      }
      else {
        sair = 1;
      }
      delay(1000);
      if (digitalRead(pinoldr) == 0) {
        sair = 1;
      }
    }
  }
  vTaskDelete(NULL);
}

void desligartask(void *pvParameters) {
  int sair = 0;
  int cont = 0 ;
  // pino do ldr
  String temps = lerconf("PINOLDR");
  byte pinoldr = temps.toInt();
  if (digitalRead(pinoldr) == 1) { // so tenta desligar se estiver ligada
    while (sair == 0) {
      irsend->sendRC6(0x0C, PHILIPS_LENGHT, 2);
      // verifica se desligou
      if (cont < 5 ) {
        cont++;
      }
      else {
        sair = 1;
      }
      delay(1000);
      if (digitalRead(pinoldr) == 0) {
        sair = 1;
      }
    }
  }
  vTaskDelete(NULL);
}

void mudarvgatask(void *pvParameters) {
  int sair = 0;
  int cont = 0 ;

  irsend->sendRC6(0x54, PHILIPS_LENGHT, 2);
  delay(3000);

  irsend->sendRC6(0x5b, PHILIPS_LENGHT, 2);
  delay(1500);

  irsend->sendRC6(0x5b, PHILIPS_LENGHT, 2);
  delay(1500);

  for (int n = 1; n <= 7; n++ ) {
    irsend->sendRC6(0x59, PHILIPS_LENGHT, 2);
    delay(1500);
  }

  irsend->sendRC6(0x5c, PHILIPS_LENGHT, 2);
  logs(String(Ligar));
  vTaskDelete(NULL);
}


bool mudarvga() {
  xTaskCreate(
    mudarvgatask,              /* Task function. */
    "mudarvgatask",            /* String with name of task. */
    10000,                     /* Stack size in words. */
    NULL,
    1,                         /* Priority of the task. */
    NULL);                     /* Task handle. */
}

bool ligartv() {
  xTaskCreate(
    ligartask,              /* Task function. */
    "ligartask",            /* String with name of task. */
    10000,                     /* Stack size in words. */
    NULL,
    1,                         /* Priority of the task. */
    NULL);                     /* Task handle. */
  return (1);
}

bool desligartv() {
  xTaskCreate(
    desligartask,              /* Task function. */
    "desligartask",            /* String with name of task. */
    10000,                     /* Stack size in words. */
    NULL,
    1,                         /* Priority of the task. */
    NULL);                     /* Task handle. */
  return (1);
}

void handleligadesliga(AsyncWebServerRequest * request) {
  String cookie;
  if (request->hasHeader("Cookie")) {
    cookie = request->header("Cookie");
    if (cookie.substring(0, 13)  == "ESPSESSIONID=0") {
      request->redirect("/login");
    }
  }
  else {
    request->redirect("/login");
  }
  String usuario, p;
  // https://github.com/me-no-dev/ESPAsyncWebServer/issues/441
  int PozStart;
  int Length;
  int PozStop ;
  String StringName =  "ESPSESSIONID=";
  Length = StringName.length() ;
  PozStart = cookie.indexOf(StringName) + Length ;
  PozStop =  cookie.indexOf(";" , PozStart );
  usuario = cookie.substring(PozStart, PozStop);
  if (usuario == "") {
    request->redirect("/login");
  }

  int gravou = 0;
  int ace = 0;
  int temp = 0;
  int fan = 0;
  // pino do ldr
  String temps = lerconf("PINOLDR");
  int pinoldr = temps.toInt();

  for (uint8_t i = 0; i < request->args(); i++) {
    if (request->argName(i) == "TV") {
      ace = request->arg(i).toInt();
      gravou = 1;
    }
  }

  String TV = "";

  if (digitalRead(pinoldr) == 1) {
    TV = "<font color=\"red\">[" + String(Ligado) + "]";
  }
  else {
    TV = "<font color=\"green\">[" + String(Desligado) + "]";
  }
  TV += "</font>";

  String htmlpage = cabecalho(usuario);

  htmlpage += imprimemenu(1, usuario);
  htmlpage += "<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/ligadesliga\"> \
               </center></td></tr>\n";
  htmlpage += imprimetabela(0, String(LigaDesliga));

  // coluna 1
  htmlpage += imprimecoluna(2, 1, 0);

  // coluna1 fim
  htmlpage += "</td>";
  htmlpage += imprimecoluna(2, 2, 0);

  // coluna1 fim
  htmlpage += "</td></tr>\n";
  htmlpage += imprimecoluna(2, 2, 0);
  // inicio da linha
  // imprime o select dos reles
  htmlpage += "TV: <SELECT NAME ='TV'>";

  htmlpage += "<option value=1 ";
  if (ace == 1 ) htmlpage += "selected";
  htmlpage += ">" + String(Ligado) + " </option>";

  htmlpage += "<option value=0 ";
  if (ace == 0 ) htmlpage += "selected";
  htmlpage += ">" + String(Desligado) + " </option>";

  htmlpage += "<option value=2 ";
  if (ace == 2 ) htmlpage += "selected";
  htmlpage += ">" + String(MudarTV) + "</option>";

  htmlpage += "<option value=3 ";
  if (ace == 3 ) htmlpage += "selected";
  htmlpage += ">" + String(MudarVGA) + "</option>";

  htmlpage += "<option value=4 ";
  if (ace == 4 ) htmlpage += "selected";
  htmlpage += ">" + String(MudarHDMI) + "</option>";

  htmlpage += " </select>&nbsp;&nbsp;";
  htmlpage += TV;
  htmlpage += "</td></tr>";

  htmlpage += imprimecoluna(2, 2, 2);

  htmlpage += " <center> <input type = \"submit\" value=" + String(LigaDesliga) + "></center></form>";
  //fim da tabela
  htmlpage +=   imprimetabela(1, "");
  request->send(200, "text/html", htmlpage);

  if (gravou) {
    if (ace == 1)  ligartv();
    if (ace == 0)  desligartv();

    if (ace == 2) {
      irsend->sendRC6(0x9f , PHILIPS_LENGHT, 1);
    }

    if (ace == 3) {
      mudarvga();
    }

    if (ace == 4)
      irsend->sendRC6(0x40, PHILIPS_LENGHT, 1);
    delay(2000);
    irsend->sendRC6(0x41, PHILIPS_LENGHT, 1);
    delay(2000);
    irsend->sendRC6(0x42, PHILIPS_LENGHT, 1);
    delay(2000);
    irsend->sendRC6(0x43, PHILIPS_LENGHT, 1);
  }
}

void handleprogramacao(AsyncWebServerRequest * request) {
  String cookie;
  if (request->hasHeader("Cookie")) {
    cookie = request->header("Cookie");
    if (cookie.substring(0, 13)  == "ESPSESSIONID=0") {
      request->redirect("/login");
    }
  }
  else {
    request->redirect("/login");
  }
  String usuario, p;
  // https://github.com/me-no-dev/ESPAsyncWebServer/issues/441
  int PozStart;
  int Length;
  int PozStop ;
  String StringName =  "ESPSESSIONID=";
  Length = StringName.length() ;
  PozStart = cookie.indexOf(StringName) + Length ;
  PozStop =  cookie.indexOf(";" , PozStart );
  usuario = cookie.substring(PozStart, PozStop);
  if (usuario == "") {
    request->redirect("/login");
  }

  String htmlpage = cabecalho(usuario);

  htmlpage += imprimemenu(1, usuario);

  // imprime cabecalho da tabela:
  htmlpage += imprimetabela(0, String(Programacao));

  File programacaof;
  byte gravou = 0;

  int i;
  String ativo[4];
  String hora[4];
  String minuto[4];
  String ligades[4];
  String nome[4];
  int n = 0, quant = 0;
  String aa = "";
  int x = 0;

  // get the post variables
  for (uint8_t i = 0; i < request->args(); i++) {
    aa = "HORA" + String(quant);
    if (request->argName(i) == aa) {
      hora[quant] = request->arg(i);
      gravou = 1;
    }

    aa = "MINUTO" + String(quant);
    if (request->argName(i) == aa) {
      minuto[quant] = request->arg(i);
      gravou = 1;
    }

    aa = "ATIVO" + String(quant);
    if (request->argName(i) == aa) {
      ativo[quant] = request->arg(i);
      gravou = 1;
    }

    aa = "LIGADES" + String(quant);
    if (request->argName(i) == aa) {
      ligades[quant] = request->arg(i);
      gravou = 1;
    }

    aa = "NOME" + String(quant);
    if (request->argName(i) == aa) {
      nome[quant] = request->arg(i);
      gravou = 1;
      if (nome[quant] != "") quant++;
    }
  }

  if (gravou) {
    // gravamos as alteracoes
    LITTLEFS.remove("/programacao.txt");
    programacaof = LITTLEFS.open("/programacao.txt", "w");
    for (i = 0; i < quant; i++) {
      programacaof.print("[");
      programacaof.print(ativo[i]);
      programacaof.print("][");
      programacaof.print(hora[i]);
      programacaof.print("][");
      programacaof.print(minuto[i]);
      programacaof.print("][");
      programacaof.print(ligades[i]);
      programacaof.print("][");
      programacaof.println(nome[i]);
    }
    programacaof.close();
    logs(String(Arquivoprogsalvo));
  }
  // byte erro = 0;
  String temp;
  programacaof = LITTLEFS.open("/programacao.txt", "r");
  if (!programacaof) {
    logs(String(Erroarquivoprogf));
  }
  else {
    quant = 0;
    while (programacaof.available() and quant < 4) {
      p = programacaof.readStringUntil('\n');
      ativo[quant] = p.substring(1, 2).toInt(); // [0][00][00][0][
      hora[quant] = p.substring(4, 6).toInt();
      minuto[quant] = p.substring(8, 10).toInt();
      ligades[quant] = p.substring(12, 13).toInt();
      nome[quant] = p.substring(15);
      quant ++;
    }
  }

  htmlpage += "<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/programacao\"> \
               </center></td></tr></td></tr>";
  // cabecalho
  htmlpage += imprimecoluna(7, 1, 0);
  htmlpage += "</td><td>";
  htmlpage += String(Ativo);
  htmlpage += "</td><td>";
  htmlpage += String(Hora);
  htmlpage += "</td><td>";
  htmlpage += String(Minuto);
  htmlpage += "</td><td>";
  htmlpage += String(LigaDesliga);
  htmlpage += "/";
  htmlpage += String(Entrada);
  htmlpage += "</td><td>";
  htmlpage += String(Nome);
  htmlpage += "</td></tr>";

  for (i = 0; i < quant; i++) {
    htmlpage += imprimecoluna(7, 2, 0);
    htmlpage += "Prog" + String(i) + " ....: </td>\n";
    //
    htmlpage += imprimecoluna(7, 3, 0);
    htmlpage += "<select name = 'ATIVO";
    htmlpage += String(i);
    htmlpage += "'>\n";
    for (n = 0; n <= 1; n++) {
      if (n == ativo[i].toInt()) // ajeitar
      {
        htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
      }
      else {
        htmlpage += "<option value = '";
      }
      htmlpage += String(n);
      htmlpage += "'>";
      if (n == 0) htmlpage += String(Nao);
      if (n == 1) htmlpage += String(Sim);
      htmlpage += " </option>";
    }
    htmlpage += " </select></td>"; // fim da linha1 coluna 2
    // fim

    // coluna 3
    htmlpage += imprimecoluna(7, 4, 0);
    htmlpage += "<select name = 'HORA";
    htmlpage += String(i);
    htmlpage += "'>\n";
    for (n = 0; n <= 24; n++) {
      if (n == hora[i].toInt()) // ajeitar
      {
        htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
      }
      else {
        htmlpage += "<option value = '";
      }
      if (n < 10 ) htmlpage += "0";
      htmlpage += String(n);
      htmlpage += "'>";
      if (n < 10 ) htmlpage += "0";
      htmlpage += String(n);

      htmlpage += " </option>";
    }
    htmlpage += " </select></td>"; // fim da linha1 coluna 2
    // fim

    // repetir
    htmlpage += imprimecoluna(7, 5, 0);
    htmlpage += "<select name = 'MINUTO";
    htmlpage += String(i);
    htmlpage += "'>\n";
    for (n = 0; n <= 59; n++) {
      if (n == minuto[i].toInt())
      {
        htmlpage += "<option selected = 'selected' value= '"; // e a opcao do config, seleciona
      }
      else {
        htmlpage += "<option value = '";
      }
      if (n < 10 ) htmlpage += "0";
      htmlpage += String(n);
      htmlpage += "'>";
      if (n < 10 ) htmlpage += "0";
      htmlpage += String(n);
      htmlpage += " </option>";
    }
    htmlpage += " </select></td>"; // fim da linha1 coluna 2
    // fim repetir

    // coluna 5 link
    htmlpage += imprimecoluna(7, 6, 0);
    htmlpage += "<select name = 'LIGADES";
    htmlpage += String(i);
    htmlpage += "'>\n";
    for (n = 0; n <= 4; n++) {
      if (n == ligades[i].toInt())
      {
        htmlpage += "<option selected = 'selected' value= '"; // e a opcao do config, seleciona
      }
      else {
        htmlpage += "<option value = '";
      }
      htmlpage += String(n);
      htmlpage += "'>";
      if (n == 0 ) htmlpage += String(Desligar);
      if (n == 1 ) htmlpage += String(Ligar);
      if (n == 2 ) htmlpage += "TV";
      if (n == 3 ) htmlpage += "VGA";
      if (n == 4 ) htmlpage += "HDMI";
      htmlpage += " </option>";
    }
    htmlpage += " </select></td>";

    htmlpage += imprimecoluna(7, 7, 0);
    htmlpage += "<input type=\"text\" size=\"48\" name=\"NOME";
    htmlpage += String(i);
    htmlpage += "\" value=\"";
    htmlpage += String(nome[i]);
    htmlpage += "\"></td></tr>"; // fim da linha1 coluna 2
  } // fim quantidade de pinos no arquivo

  // novo campo para adicao somente
  htmlpage += imprimecoluna(7, 1, 0);
  htmlpage += "Prog" + String(i) + " .......: </td>\n";
  //
  htmlpage += imprimecoluna(7, 2, 0);
  htmlpage += "<select name = 'ATIVO";
  htmlpage += String(i);
  htmlpage += "'>";
  for (n = 0; n <= 1; n++) {
    if (n == ativo[i].toInt()) // ajeitar
    {
      htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
    }
    else {
      htmlpage += "<option value = '";
    }
    htmlpage += String(n);
    htmlpage += "'>";
    if (n == 0) htmlpage += String(Nao);
    if (n == 1) htmlpage += String(Sim);
    htmlpage += " </option>";
  }
  htmlpage += " </select></td>"; // fim da linha1 coluna 2
  // fim

  // coluna 3
  htmlpage += imprimecoluna(7, 3, 0);
  htmlpage += "<select name = 'HORA";
  htmlpage += String(i);
  htmlpage += "'><option selected = 'selected' value= '00'>00</option>"; // e a opcao do config, seleciona

  for (n = 1; n <= 24; n++) {
    htmlpage += "<option value = '";
    htmlpage += String(n);
    htmlpage += "'>";
    if (n < 10 ) htmlpage += "0";
    htmlpage += String(n);

    htmlpage += " </option>";
  }
  htmlpage += " </select></td>"; // fim da linha1 coluna 2

  htmlpage += imprimecoluna(7, 4, 0);
  htmlpage += "<select name = 'MINUTO";
  htmlpage += String(i);
  htmlpage += "'><option selected = 'selected' value= '00'>00</option>"; // e a opcao do config, seleciona

  for (n = 1; n <= 59; n++) {
    htmlpage += "<option value = '";
    htmlpage += String(n);
    htmlpage += "'>";
    if (n < 10 ) htmlpage += "0";
    htmlpage += String(n);
    htmlpage += "</option>";
  }
  htmlpage += " </select></td>"; // fim da linha1 coluna 2

  htmlpage += imprimecoluna(7, 5, 0);
  htmlpage += "<select name = 'LIGADES";
  htmlpage += String(i);
  htmlpage += "'>\n";
  for (n = 0; n <= 4; n++) {
    if (n == ligades[i].toInt())
    {
      htmlpage += "<option selected = 'selected' value= '"; // e a opcao do config, seleciona
    }
    else {
      htmlpage += "<option value = '";
    }
    htmlpage += String(n);
    htmlpage += "'>";
    if (n == 0 ) htmlpage += String(Desligar);
    if (n == 1 ) htmlpage += String(Ligar);
    if (n == 2 ) htmlpage += "TV";
    if (n == 3 ) htmlpage += "VGA";
    if (n == 4 ) htmlpage += "HDMI";
    htmlpage += " </option>";
  }
  htmlpage += " </select></td>";

  htmlpage += imprimecoluna(7, 6, 0);
  htmlpage += "<input type=\"text\" size=\"48\" name=\"NOME";
  htmlpage += String(i);
  htmlpage += "\" value=\"";
  htmlpage += String(nome[i]);
  htmlpage += "\"></td>\n";
  // fim novo campo para adicao

  // submit
  htmlpage += imprimecoluna(7, 7, 7);
  htmlpage += "<input type = \"submit\" value=\"";
  htmlpage += String(Salvar);
  htmlpage += "\"></form>";
  //close the table
  htmlpage += imprimetabela(1, "");
  htmlpage += String(AdicionarProg);
  htmlpage += "<br>";
  htmlpage += String(RemoverProg);
  request->send(200, "text/html", htmlpage);
}

void handleusuarios(AsyncWebServerRequest * request) {
  String cookie;
  if (request->hasHeader("Cookie")) {
    cookie = request->header("Cookie");
    if (cookie.substring(0, 13)  == "ESPSESSIONID=0") {
      request->redirect("/login");
    }
  }
  else {
    request->redirect("/login");
  }
  String usuario, p;
  // https://github.com/me-no-dev/ESPAsyncWebServer/issues/441
  int PozStart;
  int Length;
  int PozStop ;
  String StringName =  "ESPSESSIONID=";
  Length = StringName.length() ;
  PozStart = cookie.indexOf(StringName) + Length ;
  PozStop =  cookie.indexOf(";" , PozStart );
  usuario = cookie.substring(PozStart, PozStop);
  if (usuario == "") {
    request->redirect("/login");
  }

  String htmlpage = cabecalho(usuario);

  htmlpage += imprimemenu(1, usuario);

  // imprime cabecalho da tabela:
  htmlpage += imprimetabela(0, String(Usuarios));

  File usuariosf;
  byte gravou = 0;
  byte i;
  String ativo[6];
  String usuarioa[6];
  String senha[6];
  String nivel[6];
  byte n = 0, quant = 0;
  byte erro = 0;
  String mensagemdeerro = "";

  for (uint8_t i = 0; i < request->args(); i++) {
    p = "ATIVO" + String(quant);
    if (request->argName(i) == p) {
      ativo[quant] = request->arg(i);
      gravou = 1;
    }

    p = "USUARIO" + String(quant);
    if (request->argName(i) == p) {
      usuarioa[quant] = request->arg(i);
      usuarioa[quant].trim();
      gravou = 1;
    }

    p = "SENHA" + String(quant);
    if (request->argName(i) == p) {
      senha[quant] = request->arg(i);
      gravou = 1;
    }

    p = "NIVEL" + String(quant);
    if (request->argName(i) == p) {
      nivel[quant] = request->arg(i);
      gravou = 1;
    }
    if (i == 3 or i == 7 or i == 11 or i == 15 or i == 19 ) quant++;
  }

  if (gravou) {
    // gravamos as alteracoes
    LITTLEFS.remove("/usuarios.txt");
    usuariosf = LITTLEFS.open("/usuarios.txt", "w");
    for (i = 0; i < quant; i++) {
      if (usuarioa[i] != "") {
        usuariosf.println(ativo[i]);
        usuariosf.println(nivel[i]);
        usuariosf.println(usuarioa[i]);
        usuariosf.println(senha[i]);
      }
    }
    usuariosf.close();
  }

  String p2;
  int ind1;

  usuariosf = LITTLEFS.open("/usuarios.txt", "r");
  if (!usuariosf) {
    erro = 1;
  }
  else {
    quant = 0;
    while (usuariosf.available() and quant < 6) {
      ativo[quant] = usuariosf.readStringUntil('\n');
      nivel[quant] = usuariosf.readStringUntil('\n');
      usuarioa[quant] = usuariosf.readStringUntil('\n');
      senha[quant] = usuariosf.readStringUntil('\n');
      quant++;
    }
  }
  usuariosf.close();
  htmlpage += "<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/usuarios\">";

  // cabecalho
  htmlpage += imprimecoluna(4, 1, 0);
  htmlpage += String(Usuario) + "</td><td>" + String(Senha) + "</td><td>" + String(Ativo);
  htmlpage += "</td><td>" + String(Nivel) + "</td></tr>";

  for (i = 0; i < quant; i++) {
    htmlpage += imprimecoluna(4, 1, 0);
    htmlpage += "<input type=\"text\" size=\"8\" name=\"USUARIO";
    htmlpage += String(i);
    htmlpage += "\" value=\"";
    htmlpage += String(usuarioa[i]);
    htmlpage += "\"></td>\n";

    htmlpage += imprimecoluna(4, 1, 0);
    htmlpage += "<input type=\"text\" size=\"8\" name=\"SENHA";
    htmlpage += String(i);
    htmlpage += "\" value=\"";
    htmlpage += String(senha[i]);
    htmlpage += "\"></td>\n";

    // coluna 3
    htmlpage += imprimecoluna(4, 3, 0);
    htmlpage += "<select name = 'ATIVO";
    htmlpage += String(i);
    htmlpage += "'>\n";
    for (n = 0; n <= 1; n++) {
      if (n == ativo[i].toInt()) // ajeitar
      {
        htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
      }
      else {
        htmlpage += "<option value = '";
      }
      htmlpage += String(n);
      htmlpage += "'>";
      if (n == 0) htmlpage += String(Nao);
      if (n == 1) htmlpage += String(Sim);
      htmlpage += " </option>";
    }
    htmlpage += " </select></td>";
    // fim

    // coluna 4
    htmlpage += imprimecoluna(4, 3, 0);
    htmlpage += "<select name = 'NIVEL";
    htmlpage += String(i);
    htmlpage += "'>\n";
    for (n = 0; n <= 1; n++) {
      if (n == nivel[i].toInt()) // ajeitar
      {
        htmlpage += " <option selected = 'selected' value= '"; // e a opcao do config, seleciona
      }
      else {
        htmlpage += "<option value = '";
      }
      htmlpage += String(n);
      htmlpage += "'>";
      if (n == 0) htmlpage += String(Usuario);
      if (n == 1) htmlpage += "Administrador";
      htmlpage += " </option>";
    }
    htmlpage += " </select></td></tr>";
  }

  if (quant < 5) {
    // novo campo para adicao somente
    htmlpage += imprimecoluna(4, 1, 0);
    htmlpage += "<input type=\"text\" size=\"8\" name=\"USUARIO";
    htmlpage += String(i) + "\" value=\"";
    htmlpage += String(usuarioa[i]);
    htmlpage += "\"></td>\n";

    htmlpage += imprimecoluna(4, 1, 0);
    htmlpage += "<input type=\"text\" size=\"8\" name=\"SENHA";
    htmlpage += String(i) + "\" value=\"";
    htmlpage += String(senha[i]);
    htmlpage += "\"></td>\n";

    // coluna 3
    htmlpage += imprimecoluna(4, 3, 0);
    htmlpage += "<select name = 'ATIVO";
    htmlpage += String(i);
    htmlpage += "'>\n<option selected = 'selected' value= '1'>" + String(Sim) + "</option>";
    htmlpage += "<option value = '0'>" + String(Nao) + "</option></select></td>";

    // coluna 3
    htmlpage += imprimecoluna(4, 3, 0);
    htmlpage += "<select name = 'Nivel";
    htmlpage += String(i);
    htmlpage += "'>\n<option selected = 'selected' value= '0'>Usuario</option> \
    <option value = '1'>Administrador</option> \
    </select></td></tr>";
  }
  // fim novo campo para adicao somente

  // submit
  htmlpage += imprimecoluna(5, 5, 5);
  htmlpage += "<input type = \"submit\" value=\"" + String(Salvar) + "\"></form>";
  //fim da tabela
  htmlpage +=   imprimetabela(1, "");
  htmlpage += "<br>" + String(AdicionarUsuario);


  if (mensagemdeerro) {
    htmlpage += "<font color=\"red\">" + mensagemdeerro + "</font>";
  }

  htmlpage += "</body></html>";
  request->send(200, "text/html", htmlpage);
}

//login page, also called for disconnect
void handleLogin(AsyncWebServerRequest * request) {
  String msg, debug;
  AsyncWebServerResponse *response = request->beginResponse(301);

  String ativo[6];
  String nivel[6];
  String usuario[6];
  String senha[6];
  int quant = 0;
  File usuariosf = LITTLEFS.open("/usuarios.txt", "r");
  if (!usuariosf) {
    //erro = 1;
  }
  else {
    while (usuariosf.available() and quant < 6) {
      ativo[quant] = usuariosf.readStringUntil('\n');
      nivel[quant] = usuariosf.readStringUntil('\n');
      usuario[quant] = usuariosf.readStringUntil('\n');
      usuario[quant].trim();
      senha[quant] = usuariosf.readStringUntil('\n');
      senha[quant].trim();
      quant++;
    }
  }

  if (request->hasHeader("Cookie")) {
    String cookie = request->header("Cookie");
  }
  String usuariologado = "";
  int autenticou = 0;

  if (request->hasArg("DISCONNECT")) {
    response->addHeader("Location", "/login");
    response->addHeader("Cache-Control", "no-cache");
    response->addHeader("Set-Cookie", "ESPSESSIONID=0");
    request->send(response);
  }

  if (request->hasArg("USERNAME") && request->hasArg("PASSWORD")) {
    for (int n = 0; n <= quant; n++) {
      if (request->arg("USERNAME") == usuario[n] and request->arg("PASSWORD") == senha[n]) {
        autenticou = 1;
        usuariologado = usuario[n];
      }
    }
    if (autenticou) {
      response->addHeader("Location", "/");
      response->addHeader("Cache-Control", "no-cache");
      response->addHeader("Set-Cookie", "ESPSESSIONID=" + usuariologado);
      request->send(response);
    }
    debug += "Wrong username/password! try again.";
    debug += request->arg("USERNAME");
  }
  String htmlpage = "<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
  htmlpage +=    "<script>\n"
                 " var erro=0; "
                 "function aosalvar() {\n"
                 " //var senhabase6 = btoa(document.getElementById('PASSWORD').value); \n"
                 "// document.getElementById('PASSWORD').value = senhabase6;\n"
                 " document.form.submit(); \n"
                 "} \n"
                 " </script> \n";
  " <style type = \"text/css\">:"\
  "body {"
  " font-family:Arial, Helvetica, sans-serif;"
  " font-size:14px;"
  "}"
  "label {"
  " font-weight:bold;"
  " width:100px;"
  " font-size:14px;"
  "}"
  ".box {"
  " border:#666666 solid 1px;"
  "}"
  "#top {"
  "background: #eee;"
  "border-bottom: 1px solid #ddd;"
  "padding: 0 10px;"
  "line-height: 40px;"
  "font-size: 12px;"
  "}"
  "</style>"
  "</head>";

  htmlpage += "<body bgcolor = \"#FFFFFF\"> <center><body><h1>";
  htmlpage += lerconf("NOMEESP");
  htmlpage += "</h1><form id = 'form' name = 'form' action = '/login' method = 'POST'> \
  <table> \
  <tr><td>" + String(Usuario) + ": </td> \
  <td><input type='text' size='15' maxsize='15' name='USERNAME' placeholder='user name'></td></tr> \
  <tr><td>" + String(Senha) + " ..: </td> \
  <td><input type='password' size='15' maxsize='15' name='PASSWORD' id='PASSWORD' placeholder='password'></td></tr> \
  <tr><td colspan=\"2\" align=\"center\"><INPUT TYPE = 'BUTTON' NAME = 'enviar' onclick = 'aosalvar()' VALUE = 'Enviar'></center></form>";
  htmlpage += msg + "</td></tr></table>";
  if (debug) htmlpage += debug;
  request->send(200, "text/html", htmlpage);
}

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  rtc_wdt_protect_off();
  rtc_wdt_disable();
  WiFi.mode(WIFI_STA);
  //Local intialization. Once its business is done, there is no need to keep it around  wifiManager.resetSettings();
  WiFi.persistent(true);

  AsyncWiFiManager wifiManager(&server, &dns);

  //reset saved settings
  //wifiManager.resetSettings();
  wifiManager.autoConnect("AutoConnectAP");
  //esp_wifi_set_max_tx_power(82);
  WiFi.setTxPower(WIFI_POWER_19dBm);
  inicializafs();
 // LITTLEFS.begin();
  //LITTLEFS.format();
  //LITTLEFS.remove("/feriado.txt");
  //LITTLEFS.remove("/programacao.txt");
  server.on("/login", handleLogin);
  server.on("/", handleRoot);
  server.on("/aniversarios", handleaniversarios);
  server.on("/ligadesliga", handleligadesliga);
  server.on("/avisos", handleavisos);
  server.on("/programacao", handleprogramacao);
  server.on("/configuracao", handleconfiguracao);
  server.on("/reboot", handleReboot);
  server.on("/feriados", handleferiados1);
  server.on("/arquivos.html", handlearquivos);
  server.on("/download.html", handledownload);
  server.on("/usuarios", handleusuarios);
  server.on("/ligar", handleligar);
  server.onNotFound([](AsyncWebServerRequest * request) {
    request->send(200, "text/html", "URI Not Found" );
  });
  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();

  //DisplayController.begin(red, green, blue, hsync, vsync);
  //DisplayController.begin(GPIO_NUM_27, GPIO_NUM_26, GPIO_NUM_25, GPIO_NUM_19, GPIO_NUM_23);
  //novo                       r1           r2           g1           g2           b1           b2            hsync       vsync
  //  DisplayController.begin(GPIO_NUM_16, GPIO_NUM_17, GPIO_NUM_18, GPIO_NUM_23, GPIO_NUM_14, GPIO_NUM_27, GPIO_NUM_26, GPIO_NUM_25);
  //DisplayController.begin(GPIO_NUM_16, GPIO_NUM_18, GPIO_NUM_14, GPIO_NUM_26, GPIO_NUM_25);

  delay(3000);
  // verifica se está desabilitado o vga
  File vgaf;
  String p;
  vgaf = LITTLEFS.open("/vga.txt", "r");
  while (vgaf.available()) {
    p = vgaf.readStringUntil('\n');
    habilitado = p.substring(1, 2).toInt();
  }
  vgaf.close();
  int r1, g1, b1, vsync, hsync;
  // fim verifica se está desabilitado o vga

  if (habilitado) {
    // read pins
    p = lerconf("R1");
    r1 = p.toInt();
    p = lerconf("G1");
    g1 = p.toInt();
    p = lerconf("B1");
    b1 = p.toInt();
    p = lerconf("VSYNC");
    vsync = p.toInt();
    p = lerconf("HSYNC");
    hsync = p.toInt();
    DisplayController.begin((gpio_num_t) r1, (gpio_num_t) g1, (gpio_num_t) b1, (gpio_num_t) hsync, (gpio_num_t) vsync);
    DisplayController.setResolution(VGA_640x480_60Hz, false);
    DisplayController.shrinkScreen(4, 0);
    DisplayController.moveScreen(4, 0);
    // Color item 0 is pure Red
    DisplayController.setPaletteItem(1, RGB888(255, 255, 255));
    DisplayController.setPaletteItem(2, RGB888(255, 0, 0));
    DisplayController.setPaletteItem(3, RGB888(0, 255, 0));
    DisplayController.setPaletteItem(4, RGB888(0, 0, 0));

    Terminal.begin(&DisplayController);
    Terminal.loadFont(&fabgl::FONT_10x20);
    Terminal.enableCursor(true);
    Terminal.inputQueueSize = 128;
  }
  timeClient.setPoolServerName("a.ntp.br");
  timeClient.update();
  delay(2000);
  setSyncProvider(requestSync);  //set function to call when sync required
  setSyncInterval(50000);        //sync Time Library to RTC every 5 seconds
  // pino do ir
  String temps = lerconf("PINOIR");
  int pinoir = temps.toInt();
  static IRsend static_irsend(pinoir);
  irsend = &static_irsend;
  irsend->begin();
  // pino do ldr
  temps = lerconf("PINOLDR");
  int pinoldr = temps.toInt();

  pinMode(pinoldr, INPUT);
  feriadossort();
  if (habilitado) {
    mostravga();
  }

  rotatelogs();  // rotate the log file, freeing space

  if (!bmp.begin(0x77)) {
    logs(String(SensorDeTemperaturanaoenc));
  }
  else {
    logs(String(SensorDeTemperaturaenc));
    /* Default settings from datasheet. */
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                    Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                    Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                    Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
  }
  // enable or not mDNS
  String mdns = lerconf("mDNS");
  String nomeesp = lerconf("NOMEESP");
  if (mdns.toInt()) {
    MDNS.begin(nomeesp.c_str());
    MDNS.addService("http", "tcp", 80);
    logs("mDNS");
  }
}

void imprimehora() {
  delay(400);
  char dia[3];
  char mes[3];
  char ano[5];
  char temp1[3];

#ifdef LANG_pt
  sprintf(dia, "%02d", day());
  sprintf(mes, "%02d", month());
  sprintf(ano, "%02d", year());
#endif

#ifdef LANG_en
  sprintf(dia, "%02d", day());
  sprintf(mes, "%02d", month());
  sprintf(ano, "%02d", year());
#endif
  int x = 60;
  int y = 15;

  canvas.selectFont(&fabgl::FONT_std_24);

#ifdef LANG_pt
  canvas.drawText(125 + x, y, dia);
#endif

#ifdef LANG_en
  canvas.drawText(156 + x, y, mes);
#endif
  canvas.drawText(149 + x, y, "/");

#ifdef LANG_pt
  canvas.drawText(156 + x, y, mes);
#endif

#ifdef LANG_en
  canvas.drawText(125 + x, y, dia);
#endif

  canvas.drawText(179 + x, y, "/");
  canvas.drawText(185 + x, y, ano);

  char hora[3];
  char minuto[3];
  char segundo[3];
  sprintf(hora, "%02d", hour());
  sprintf(minuto, "%02d", minute());
  sprintf(segundo, "%02d", second());

  canvas.drawText(245 + x, y, hora);
  canvas.drawText(271 + x, y, ":");
  canvas.drawText(281 + x, y, minuto);
  canvas.drawText(305 + x, y, ":");
  canvas.drawText(315 + x, y, segundo);

  temperatura.toCharArray(temp1, 3);

  canvas.drawText(245 + x, y, hora);
  canvas.drawText(271 + x, y, ":");
  canvas.drawText(281 + x, y, minuto);
  canvas.drawText(305 + x, y, ":");
  canvas.drawText(315 + x, y, segundo);
  canvas.drawText(525 + x, y, temp1);
  canvas.drawText(555 + x, y, "C");
}

// reboot every day
void verificareiniciar() {
  if (hour() == 6) {
    if (minute() == 50) {
      if (second() < 10) {
        ESP.restart();
      }
    }
  }
  return;
}

void verificaprog() {
  int hora;
  int minuto;
  int ligades;
  int ativo;
  String p, nome;
  if (feriado(day(), month()) != 1 and getDayNumber(day(), month(), year()) != 0 and getDayNumber(day(), month(), year()) != 7) {
    File programacaof = LITTLEFS.open("/programacao.txt", "r");
    if (!programacaof) {
      logs(String(Erroarquivoprogf));
    }
    else {
      while (programacaof.available()) {
        p = programacaof.readStringUntil('\n');
        ativo = p.substring(1, 2).toInt();
        if (ativo) {
          hora = p.substring(4, 6).toInt();
          minuto = p.substring(8, 10).toInt();
          ligades = p.substring(12, 13).toInt();
          nome = p.substring(15);
          if (hora == hour() and minuto == minute()) {
            logs(String(Executandoprog) + ": " + nome);
            if (ligades == 0 ) desligartv();
            if (ligades == 1 ) ligartv();
            if (ligades == 2 ) irsend->sendRC6(0x9f , PHILIPS_LENGHT, 1);
            if (ligades == 3 ) mudarvga();
          }
        }
      }
      programacaof.close();
    }
  } // se nao for feriado ou sabado/domingo
}

void loop() {
  if (habilitado) {
    imprimehora();
  }
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
    }
  }

  if (currentMillis - previousMillisrx >= intervalrx) {
    verificaprog();
    // reboot time (usually after midnight to update the day)
    verificareiniciar();
  }
  if (currentMillis - previousMillistemp >= intervaltemp) {
    previousMillistemp = currentMillis;
    temperatura = bmp.readTemperature();
  }
}

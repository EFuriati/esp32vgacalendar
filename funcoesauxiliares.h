bool inicializafs() {
  if (!LITTLEFS.begin()) {
    LITTLEFS.format();
    File file = LITTLEFS.open("/log.txt", "w");
    file.close();
    file = LITTLEFS.open("/config.txt", "w");

    file.println("mdns=1");
    file.println("senhaint=123123");
    file.println("sensortemp=0");
    file.println("sensortemppino=4");
    file.println("nomeesp=espchange");
    file.println("pinoldr=11");
    file.println("pinoir=17");
    file.println("patrimonio=1");
    file.println("r1=16");
    file.println("g1=17");
    file.println("b1=19");
    file.println("vsync=20");
    file.println("hsync=21");
    file.close();

    file = LITTLEFS.open("/usuarios.txt", "w");
    file.println("1");
    file.println("1");
    file.println("admin");
    file.println("admin");
  }
}
String cabecalho(String usuariologado) {
  String esp32name = lerconf("NOMEESP");
  String htmlpage = "<html><head><title>";
  htmlpage += esp32name;
  htmlpage += "</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
  htmlpage += "<style>";
  htmlpage += "header {";
  htmlpage += "background-color: #666;";
  htmlpage += "padding: 10px;";
  htmlpage += "text-align: right;";
  htmlpage += "font-size: 16px;";
  htmlpage += "color: white;";
  htmlpage += "}";
  htmlpage += "</style></head><body>";
  htmlpage += "<header>";
  htmlpage += "<a href=\"/login?DISCONNECT\">" + String(usuariologado) + "</a>";
  htmlpage += "</header>";
  return htmlpage;
}

// https://imasters.com.br/desenvolvimento/webserver-log-no-esp8266-com-spiffs
void logs(String msg) {
#ifdef LANG_en
  char const * wdays[] = {"Sat", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri"};
#endif
#ifdef LANG_pt
  char const * wdays[] = {"Sab", "Dom", "Seg", "Ter", "Qua", "Qui", "Sex"};
#endif
  int day_ = weekday() % 7;

  //Abre o arquivo para adição (append)
  //Inclue sempre a escrita na ultima linha do arquivo
  File rFile = LITTLEFS.open("/log.txt", "a+");

  if (rFile) {
    if (day() < 10 ) rFile.print("0");
    rFile.print(day());
    rFile.print("/");
    if (month() < 10 ) rFile.print("0");
    rFile.print(month());
    rFile.print("/");
    rFile.print(year());
    rFile.print("(");
    rFile.print(wdays[day_]);
    rFile.print(")");
    if (day_ == 2 ) "  ";
    if (day_ == 3 ) "   ";
    rFile.print(" - ");
    rFile.print(timeClient.getFormattedTime());
    rFile.println(" : " + msg);
  }
  rFile.close();
}

void feriadossort() {
  int dia[20];
  int mes[20];
  int ativo[20];
  String feriado[20];
  String fixo[20];
  int quant = 0;
  String p;
  int diat, mest;
  String feriadot, fixot;

  // busca os feriados
  File feriadosf = LITTLEFS.open("/feriado.txt", "r");
  while (feriadosf.available()) {
    p = feriadosf.readStringUntil('\n');
    if (p.substring(0, 1) == "[") {
      dia[quant] = p.substring(1, 3).toInt();
      mes[quant] = p.substring(5, 7).toInt();
      fixo[quant] = p.substring(9, 10).toInt();
      feriado[quant] = p.substring(12);
      feriado[quant].trim();
      quant++;
    }
  }

  // primeira passada, mes
  for (int step = 0; step < quant - 1; step++) {
    for (int n = 0; n < quant - step; n++) {
      if (feriado[n] != "") {
        if (mes[n] > mes[n + 1]) {
          diat = dia[n];
          mest = mes[n];
          feriadot = feriado[n];
          fixot = fixo[n];

          dia[n] = dia[n + 1];
          mes[n] = mes[n + 1];
          feriado[n] = feriado[n + 1];
          fixo[n] = fixo[n + 1];

          dia[n + 1] = diat;
          mes[n + 1] = mest;
          fixo[n + 1] = fixot;
          feriado[n + 1] = feriadot;
        }
      }
    }
  }
  // segunda passada, dia
  for (int step = 0; step < quant - 1; step++) {
    for (int n = 0; n < quant - step; n++) {
      if (feriado[n] != "") {
        if (dia[n] > dia[n + 1] and mes[n] == mes[n + 1]) {
          diat = dia[n];
          mest = mes[n];
          feriadot = feriado[n];
          fixot = fixo[n];

          dia[n] = dia[n + 1];
          mes[n] = mes[n + 1];
          feriado[n] = feriado[n + 1];
          fixo[n] = fixo[n + 1];

          dia[n + 1] = diat;
          mes[n + 1] = mest;
          fixo[n + 1] = fixot;
          feriado[n + 1] = feriadot;
        }
      }
    }
  }

  feriadosf.close();
  // grava os feriados
  LITTLEFS.remove("/feriado.txt");
  feriadosf = LITTLEFS.open("/feriado.txt", "w");
  for (int i = 0; i <= quant; i++) {
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
  return;
}

String lerconf(String configuracao) {
  File config = LITTLEFS.open("/config.txt", "r");
  byte erro = 0;
  String p, retconf;
  if (!config) {
    erro = 1;
  }
  else {
    if (config.available()) {
      p = config.readStringUntil('\n');
      if (configuracao == "mDNS") {
        retconf = p.substring(5);
      }
      p = config.readStringUntil('\n');
      //senhaint = p.substring(9);
      // senhaint.trim();
      p = config.readStringUntil('\n');
      //sensortemp = p.substring(11).toInt();
      p = config.readStringUntil('\n');
      if (configuracao == "SENSORTEMPPINO") {
        retconf = p.substring(15).toInt();
      }
      p = config.readStringUntil('\n');
      if (configuracao == "NOMEESP") {
        retconf = p.substring(8);
      }

      p = config.readStringUntil('\n');
      if (configuracao == "PINOLDR") {
        retconf = p.substring(8).toInt();
      }

      p = config.readStringUntil('\n');
      if (configuracao == "PINOIR") {
        retconf = p.substring(7).toInt();
      }

      p = config.readStringUntil('\n');
      if (configuracao == "PATRIMONIO") {
        retconf = p.substring(13).toInt();
      }
      p = config.readStringUntil('\n');
      if (configuracao == "R1") {
        retconf = p.substring(3).toInt();
      }
      p = config.readStringUntil('\n');
      if (configuracao == "G1") {
        retconf = p.substring(3).toInt();
      }
      p = config.readStringUntil('\n');
      if (configuracao == "B1") {
        retconf = p.substring(3).toInt();
      }
      p = config.readStringUntil('\n');
      if (configuracao == "VSYNC") {
        retconf = p.substring(6).toInt();
      }
      p = config.readStringUntil('\n');
      if (configuracao == "HSYNC") {
        retconf = p.substring(6).toInt();
      }
    }
  }

  return retconf;
}

void rotatelogs() {
  File file = LITTLEFS.open("/log.txt");
  if (file.size() > 10000) {
    if (LITTLEFS.exists("/log1.txt")) {
      LITTLEFS.remove("/log1.txt");
    }
    file.close();
    LITTLEFS.rename("/log.txt", "/log1.txt");
    file.close();
    file = LITTLEFS.open("/log.txt", "w");
    file.close();
  }

  file = LITTLEFS.open("/debuglog.txt");
  if (file.size() > 25000) {
    if (LITTLEFS.exists("/debuglog1.txt")) {
      LITTLEFS.remove("/debuglog1.txt");
    }
    file.close();
    LITTLEFS.rename("/debuglog.txt", "/debuglog1.txt");
  }
  return;
}


int check_leapYear(int year) { //checks whether the year passed is leap year or not
  if (year % 400 == 0 || (year % 100 != 0 && year % 4 == 0))
    return 1;
  return 0;
}

int feriado(int d, int mon) {
  int mesatual = month();
  int retorno = 0;
  String p;
  int mesi;
  // mostra os feriados do mes na tela vga
  File feriadotxt = LITTLEFS.open("/feriado.txt", FILE_READ);
  while (feriadotxt.available()) {
    p = feriadotxt.readStringUntil('\n');
    mesi = p.substring(5, 7).toInt();
    if (mesi == mesatual) {
      if (d == p.substring(1, 3).toInt()) retorno = 1;
    }
  }
  feriadotxt.close();
  return retorno;
}

int getNumberOfDays(int month, int year) { //returns the number of days in given month
  switch (month) {                        //and year
    case 1 : return (31);
    case 2 : if (check_leapYear(year) == 1)
        return (29);
      else
        return (28);
    case 3 : return (31);
    case 4 : return (30);
    case 5 : return (31);
    case 6 : return (30);
    case 7 : return (31);
    case 8 : return (31);
    case 9 : return (30);
    case 10: return (31);
    case 11: return (30);
    case 12: return (31);
    default: return (-1);
  }
}

int getDayNumber(int day, int mon, int year) { //retuns the day number
  int res = 0, t1, t2, y = year;
  year = year - 1600;
  while (year >= 100) {
    res = res + 5;
    year = year - 100;
  }
  res = (res % 7);
  t1 = ((year - 1) / 4);
  t2 = (year - 1) - t1;
  t1 = (t1 * 2) + t2;
  t1 = (t1 % 7);
  res = res + t1;
  res = res % 7;
  t2 = 0;
  for (t1 = 1; t1 < mon; t1++) {
    t2 += getNumberOfDays(t1, y);
  }
  t2 = t2 + day;
  t2 = t2 % 7;
  res = res + t2;
  res = res % 7;
  if (y > 2000)
    res = res + 1;
  res = res % 7;
  return res;
}

bool estadotv() {
  String temps = lerconf("PINOLDR");
  int pinoldr = temps.toInt();
  int estadotv = digitalRead(pinoldr);
  return estadotv;
}

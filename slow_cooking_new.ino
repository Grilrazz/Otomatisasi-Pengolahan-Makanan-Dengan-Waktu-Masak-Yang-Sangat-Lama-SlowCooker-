#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_MLX90614.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
Adafruit_MLX90614 mlx = Adafruit_MLX90614();


// =====================================================
// PIN
// =====================================================

// BUTTON
const int btnStart = 2;
const int btnStop  = 4;
const int btnUp    = A1;
const int btnDown  = A2;

// TDS
const int tdsPin = A0;

// BTS7960
const int RPWM = 6;
const int LPWM = 7;
const int REN  = 8;
const int LEN  = 9;

// L298N KOMPOR
const int ENA = 5;
const int IN1 = 10;
const int IN2 = 11;

// L298N SENSOR
const int ENB = 3;
const int IN3 = 12;
const int IN4 = 13;


// =====================================================
// VARIABLE
// =====================================================

String menuMakanan[] = {
  "Rendang",
  "Gudeg",
  "Dodol"
};

int jumlahMenu = 3;
int selectedMenu = 0;

int mode = 0;

/*
0 = pilih makanan
1 = setting jam
2 = setting menit
3 = memasak
4 = selesai
*/

int jam = 0;
int menit = 0;

float suhu;

//======================
// TDS
//======================

float ppm = 0;
float ppmLast = 0;
float ppmAverage = 0;

int rankRPM = 1;
int pwmMotor = 45;

// Buffer Moving Average
float ppmBuffer[5];
byte ppmIndex = 0;
int deltaTDS = 0;

bool matang = false;
bool komporSudahMati = false;
bool tdsSudahAktif = false;


// =====================================================
// COUNTDOWN
// =====================================================

unsigned long totalDetik = 0;
unsigned long lastCountdown = 0;

// =====================================================
// RPM
// =====================================================
unsigned long lastRPMUpdate = 0;

// =====================================================
// TIMER
// =====================================================

unsigned long lastLCD = 0;
unsigned long lastTDS = 0;
unsigned long lastSerial = 0;
unsigned long waktuMulaiMasak = 0;
bool tdsAwalSudahDibaca = false;

// =====================================================
// PENGADUK
// =====================================================

bool pengadukNyala = false;


// =====================================================
// STABIL TDS
// =====================================================

int stabilCounter = 0;


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(9600);

  lcd.init();
  lcd.backlight();

  mlx.begin();

  // BUTTON
  pinMode(btnStart, INPUT_PULLUP);
  pinMode(btnStop, INPUT_PULLUP);
  pinMode(btnUp, INPUT_PULLUP);
  pinMode(btnDown, INPUT_PULLUP);

  // BTS7960
  pinMode(RPWM, OUTPUT);
  pinMode(LPWM, OUTPUT);
  pinMode(REN, OUTPUT);
  pinMode(LEN, OUTPUT);

  // L298N
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // ENABLE BTS
  digitalWrite(REN, HIGH);
  digitalWrite(LEN, HIGH);

  stopPengaduk();
  stopMotorSensor();
  stopKomporMotor();

  lcd.setCursor(0,0);
  lcd.print("Slow Cooking");

  lcd.setCursor(0,1);
  lcd.print("System Ready");

  delay(2000);

  lcd.clear();
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  bacaTombol();

  switch(mode) {

    case 0:
      menuPilihMakanan();
      break;

    case 1:
      settingJam();
      break;

    case 2:
      settingMenit();
      break;

    case 3:
      prosesMasak();
      break;

    case 4:
      selesaiMasak();
      break;
  }
}


// =====================================================
// MENU
// =====================================================

void menuPilihMakanan() {

  if(millis() - lastLCD >= 300) {

    lastLCD = millis();

    lcd.setCursor(0,0);
    lcd.print("Pilih Menu    ");

    lcd.setCursor(0,1);
    lcd.print("                ");

    lcd.setCursor(0,1);
    lcd.print(menuMakanan[selectedMenu]);
  }
}


// =====================================================
// SET JAM
// =====================================================

void settingJam() {

  if(millis() - lastLCD >= 300) {

    lastLCD = millis();

    lcd.setCursor(0,0);
    lcd.print("Set Jam       ");

    lcd.setCursor(0,1);
    lcd.print("Jam: ");
    lcd.print(jam);
    lcd.print("      ");
  }
}


// =====================================================
// SET MENIT
// =====================================================

void settingMenit() {

  if(millis() - lastLCD >= 300) {

    lastLCD = millis();

    lcd.setCursor(0,0);
    lcd.print("Set Menit     ");

    lcd.setCursor(0,1);
    lcd.print("Menit: ");
    lcd.print(menit);
    lcd.print("      ");
  }
}


// =====================================================
// PROSES MASAK
// =====================================================

void prosesMasak() {

  // =========================================
  // COUNTDOWN
  // =========================================

  if(millis() - lastCountdown >= 1000) {

    lastCountdown = millis();

    if(totalDetik > 0) {

      totalDetik--;
    }
    else {

      mode = 4;
    }
  }


  // =========================================
  // BACA SUHU BOX
  // =========================================

  suhu = mlx.readObjectTempC();

  // =========================================
  // MONITOR SERIAL SETIAP 3 DETIK
  // =========================================

  if(millis() - lastSerial >= 3000)
  {
    lastSerial = millis();

    monitorSerial();
  }

  // =========================================
  // BACA TDS AWAL PADA DETIK KE-10
  // =========================================
  if(!tdsAwalSudahDibaca &&
  millis() - waktuMulaiMasak >= 10000)
  {
    prosesTDSAwal();
  }

  // =========================================
  // UPDATE RPM BERDASARKAN SUHU
  // =========================================

  if(millis() - lastRPMUpdate >= 1000)
  {
    lastRPMUpdate = millis();

    tentukanRankRPM();
    updatePWMMotor();

    if(pengadukNyala)
    {
      jalankanPengaduk();
    }
  }
  
  // =========================================
  // PENGADUK SELALU NYALA
  // =========================================

  if(!pengadukNyala) {

    jalankanPengaduk();

    pengadukNyala = true;
  }


  // =========================================
  // CEK TDS TIAP 5 MENIT
  // =========================================

  if(millis() - lastTDS >= 300000) {

    prosesCekTDS();

    lastTDS = millis();
  }


  // =========================================
  // LCD MONITORING
  // WAKTU + SUHU + TDS + RPM
  // =========================================

  if(millis() - lastLCD >= 1000) {

    lastLCD = millis();

    int sisaJam = totalDetik / 3600;
    int sisaMenit = (totalDetik % 3600) / 60;
    int sisaDetik = totalDetik % 60;


  // =======================================
  // TAMPILAN 1
  // WAKTU + SUHU
  // =======================================

    lcd.setCursor(0,0);

    if(sisaJam < 10) lcd.print("0");
    lcd.print(sisaJam);

    lcd.print(":");

    if(sisaMenit < 10) lcd.print("0");
    lcd.print(sisaMenit);

    lcd.print(":");

    if(sisaDetik < 10) lcd.print("0");
    lcd.print(sisaDetik);

    lcd.print(" ");

    lcd.print(suhu, 1);
    lcd.print("C");


  // =======================================
  // BARIS 2
  // TDS + RANK + PWM
  // =======================================

    lcd.setCursor(0,1);

    lcd.print("P:");
    lcd.print((int)ppmAverage);

    lcd.print(" R:");
    lcd.print(rankRPM);

    lcd.print(" PWM:");
    lcd.print(pwmMotor);

    lcd.print(" ");
  }

  //cekKematangan();
}


// =====================================================
// PROSES CEK TDS
// =====================================================

void prosesCekTDS() {

  // ===================================
  // STOP PENGADUK
  // ===================================

  stopPengaduk();

  pengadukNyala = false;

  delay(3000);


  // ===================================
  // SENSOR TURUN
  // ===================================

  sensorTurun();

  delay(1000);

  stopMotorSensor();


  // ===================================
  // TUNGGU STABIL
  // ===================================

  delay(15000);


  // ===================================
  // BACA TDS
  // ===================================

  ppm = bacaPPM();

  updateMovingAverage();

  // ===================================
  // CEK PERUBAHAN TDS
  // ===================================

  if(ppmLast > 0)
  {
    deltaTDS = abs(ppmAverage - ppmLast);

    // Jika perubahan TDS sudah cukup besar
    if(deltaTDS >= 5)
    {
      tdsSudahAktif = true;
    }
  }

  tentukanRankRPM();

  updatePWMMotor();

  Serial.println("-----------------------");

  Serial.print("PPM : ");
  Serial.println(ppm);

  Serial.print("AVG : ");
  Serial.println(ppmAverage);

  Serial.print("Rank : ");
  Serial.println(rankRPM);

  Serial.print("PWM : ");
  Serial.println(pwmMotor);

  ppmLast = ppmAverage;


  // ===================================
  // SENSOR NAIK
  // ===================================

  sensorNaik();

  delay(1000);

  stopMotorSensor();


  // ===================================
  // JALANKAN PENGADUK
  // ===================================

  delay(1000);

  jalankanPengaduk();

  pengadukNyala = true;
}

void prosesTDSAwal()
{

  Serial.println("======================");
  Serial.println("PEMBACAAN TDS AWAL");
  Serial.println("======================");


  // ===================================
  // STOP PENGADUK
  // ===================================

  stopPengaduk();

  pengadukNyala = false;

  Serial.println("Pengaduk STOP");

  delay(3000);


  // ===================================
  // SENSOR TURUN
  // ===================================

  Serial.println("Sensor TDS TURUN");

  sensorTurun();

  delay(1000);

  stopMotorSensor();


  // ===================================
  // TUNGGU SENSOR STABIL
  // ===================================

  Serial.println("Menunggu sensor stabil...");

  delay(15000);


  // ===================================
  // BACA TDS AWAL
  // ===================================

  ppm = bacaPPM();

  updateMovingAverage();


  // ===================================
  // SIMPAN SEBAGAI NILAI AWAL
  // ===================================

  ppmLast = ppmAverage;

  deltaTDS = 0;

  Serial.print("TDS AWAL : ");
  Serial.println(ppmLast);


  // ===================================
  // SENSOR NAIK
  // ===================================

  Serial.println("Sensor TDS NAIK");

  sensorNaik();

  delay(1000);

  stopMotorSensor();


  // ===================================
  // JALANKAN PENGADUK
  // ===================================

  delay(1000);

  jalankanPengaduk();

  pengadukNyala = true;

  tdsAwalSudahDibaca = true;

  Serial.println("TDS AWAL SELESAI");
  Serial.println("======================");
}

// =====================================================
// SELESAI
// =====================================================

void selesaiMasak() {

  stopPengaduk();

  if(!komporSudahMati) {

    matikanKompor();

    komporSudahMati = true;
  }

  lcd.setCursor(0,0);
  lcd.print("MASAKAN        ");

  lcd.setCursor(0,1);
  lcd.print("SELESAI        ");
}


// =====================================================
// BUTTON
// =====================================================

void bacaTombol() {

  // START
  if(digitalRead(btnStart) == LOW) {

    delay(200);

    mode++;

    if(mode == 3) {

      matang = false;
      komporSudahMati = false;

      stabilCounter = 0;

      // RESET SISTEM TDS
      tdsSudahAktif = false;
      tdsAwalSudahDibaca = false;
      ppmLast = 0;
      ppmAverage = 0;
      ppmIndex = 0;
      deltaTDS = 0;

      totalDetik = (jam * 3600UL) + (menit * 60UL);

      lastCountdown = millis();
      lastTDS = millis();
      lastSerial = millis();
      lastRPMUpdate = millis();

      waktuMulaiMasak = millis();

      jalankanPengaduk();

      pengadukNyala = true;
    }

    if(mode > 4)
      mode = 0;
  }


  // STOP
  if(digitalRead(btnStop) == LOW) {

    stopPengaduk();
    stopMotorSensor();
    matikanKompor();

    mode = 0;

    delay(200);
  }


  // UP
  if(digitalRead(btnUp) == LOW) {

    if(mode == 0) {

      selectedMenu++;

      if(selectedMenu >= jumlahMenu)
        selectedMenu = 0;
    }

    else if(mode == 1) {

      jam++;

      if(jam > 12)
        jam = 12;
    }

    else if(mode == 2) {

      menit++;

      if(menit > 59)
        menit = 59;
    }

    delay(200);
  }


  // DOWN
  if(digitalRead(btnDown) == LOW) {

    if(mode == 0) {

      selectedMenu--;

      if(selectedMenu < 0)
        selectedMenu = jumlahMenu - 1;
    }

    else if(mode == 1) {

      jam--;

      if(jam < 0)
        jam = 0;
    }

    else if(mode == 2) {

      menit--;

      if(menit < 0)
        menit = 0;
    }

    delay(200);
  }
  
}

//------------------------------------------------------
// Konversi ADC menjadi PPM
//------------------------------------------------------
float bacaPPM()
{
    float voltage = analogRead(tdsPin) * 5.0 / 1023.0;

    float tds = (133.42 * voltage * voltage * voltage
               - 255.86 * voltage * voltage
               + 857.39 * voltage) * 0.5;

    return tds;
}

void updateMovingAverage()
{
    ppmBuffer[ppmIndex] = ppm;

    ppmIndex++;

    if(ppmIndex >= 5)
        ppmIndex = 0;

    float total = 0;

    for(int i=0;i<5;i++)
        total += ppmBuffer[i];

    ppmAverage = total / 5.0;
}

void tentukanRankRPM()
{
  // ==================================================
  // TDS BELUM AKTIF
  // RPM HANYA BERDASARKAN SUHU
  // ==================================================

  if(!tdsSudahAktif)
  {
    if(suhu < 60)
      rankRPM = 1;

    else if(suhu < 70)
      rankRPM = 2;

    else if(suhu < 80)
      rankRPM = 3;

    else if(suhu < 90)
      rankRPM = 4;

    else
      rankRPM = 5;

    return;
  }


  // ==================================================
  // TDS SUDAH AKTIF
  // TDS MENENTUKAN RANK DASAR
  // SUHU MENYESUAIKAN
  // ==================================================

  int rankDasar = 1;


  // ==========================================
  // RANK DASAR DARI TDS
  // ==========================================

  if(selectedMenu == 0)
  {
    // RENDANG

    if(ppmAverage < 120)
      rankDasar = 1;

    else if(ppmAverage < 145)
      rankDasar = 2;

    else if(ppmAverage < 170)
      rankDasar = 3;

    else if(ppmAverage < 195)
      rankDasar = 4;

    else
      rankDasar = 5;
  }


  else if(selectedMenu == 1)
  {
    // GUDEG

    if(ppmAverage < 130)
      rankDasar = 1;

    else if(ppmAverage < 155)
      rankDasar = 2;

    else if(ppmAverage < 175)
      rankDasar = 3;

    else if(ppmAverage < 200)
      rankDasar = 4;

    else
      rankDasar = 5;
  }


  else
  {
    // DODOL

    if(ppmAverage < 140)
      rankDasar = 1;

    else if(ppmAverage < 170)
      rankDasar = 2;

    else if(ppmAverage < 200)
      rankDasar = 3;

    else if(ppmAverage < 230)
      rankDasar = 4;

    else
      rankDasar = 5;
  }


  // ==========================================
  // SUHU MENYESUAIKAN RANK
  // ==========================================

  rankRPM = rankDasar;


  if(suhu < 60)
  {
    rankRPM -= 1;
  }

  else if(suhu >= 60 && suhu < 80)
  {
    // Tidak ada perubahan
  }

  else if(suhu >= 80 && suhu < 90)
  {
    rankRPM += 1;
  }

  else if(suhu >= 90)
  {
    rankRPM += 2;
  }


  // ==========================================
  // BATASI RANK 1 - 5
  // ==========================================

  if(rankRPM < 1)
    rankRPM = 1;

  if(rankRPM > 5)
    rankRPM = 5;
}

void updatePWMMotor()
{

    switch(rankRPM)
    {

        case 1:
            pwmMotor=35;
        break;

        case 2:
            pwmMotor=45;
        break;

        case 3:
            pwmMotor=60;
        break;

        case 4:
            pwmMotor=75;
        break;

        case 5:
            pwmMotor=90;
        break;

    }

}

// =====================================================
// MOTOR PENGADUK
// =====================================================

void jalankanPengaduk()
{

    analogWrite(LPWM,0);

    analogWrite(RPWM,pwmMotor);

}


// =====================================================
// STOP PENGADUK
// =====================================================

void stopPengaduk() {

  analogWrite(RPWM, 0);
  analogWrite(LPWM, 0);
}


// =====================================================
// MOTOR KOMPOR
// =====================================================

void matikanKompor() {

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  analogWrite(ENA, 70);

  delay(3000);

  analogWrite(ENA, 0);
}


void stopKomporMotor() {

  analogWrite(ENA, 0);
}


// =====================================================
// SENSOR TURUN
// =====================================================

void sensorTurun() {

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  analogWrite(ENB, 90);
}


// =====================================================
// SENSOR NAIK
// =====================================================

void sensorNaik() {

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  analogWrite(ENB, 100);
}


// =====================================================
// STOP SENSOR
// =====================================================

void stopMotorSensor() {

  analogWrite(ENB, 0);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}


// =====================================================
// CEK KEMATANGAN
// =====================================================

void cekKematangan() {

  // MINIMAL 30 MENIT
  if(millis() - waktuMulaiMasak < 1800000) {
    return;
  }

  bool stabil = false;

  if(selectedMenu == 0) {

    // RENDANG
    if(deltaTDS <= 5)
      stabil = true;
  }

  else if(selectedMenu == 1) {

    // GUDEG
    if(deltaTDS <= 3)
      stabil = true;
  }

  else if(selectedMenu == 2) {

    // DODOL
    if(deltaTDS <= 2)
      stabil = true;
  }


  // ===================================
  // HARUS STABIL 3X
  // ===================================

  if(stabil) {

    stabilCounter++;
  }
  else {

    stabilCounter = 0;
  }


  if(stabilCounter >= 3) {

    matang = true;
  }


  if(matang) {

    mode = 4;
  }
}

  // ===================================
  // SERIAL MONITOR
  // ===================================
void monitorSerial()
{
  unsigned long sisaJam = totalDetik / 3600;
  unsigned long sisaMenit = (totalDetik % 3600) / 60;
  unsigned long sisaDetik = totalDetik % 60;

  Serial.println();
  Serial.println("================================");
  Serial.println("       MONITORING SISTEM");
  Serial.println("================================");

  Serial.print("Menu          : ");
  Serial.println(menuMakanan[selectedMenu]);

  Serial.print("Suhu          : ");
  Serial.print(suhu, 2);
  Serial.println(" C");

  Serial.print("TDS           : ");
  Serial.print(ppm, 2);
  Serial.println(" ppm");

  Serial.print("TDS Average   : ");
  Serial.print(ppmAverage, 2);
  Serial.println(" ppm");

  Serial.print("TDS Aktif     : ");

  if(tdsSudahAktif)
    Serial.println("YA");
  else
    Serial.println("BELUM");

  Serial.print("Delta TDS     : ");
  Serial.print(deltaTDS);
  Serial.println(" ppm");

  Serial.print("Rank RPM      : ");
  Serial.println(rankRPM);

  Serial.print("PWM Motor     : ");
  Serial.println(pwmMotor);

  Serial.print("Motor         : ");

  if(pengadukNyala)
    Serial.println("ON");
  else
    Serial.println("OFF");

  Serial.print("Waktu         : ");

  if(sisaJam < 10)
    Serial.print("0");

  Serial.print(sisaJam);
  Serial.print(":");

  if(sisaMenit < 10)
    Serial.print("0");

  Serial.print(sisaMenit);
  Serial.print(":");

  if(sisaDetik < 10)
    Serial.print("0");

  Serial.println(sisaDetik);

  Serial.println("================================");
}
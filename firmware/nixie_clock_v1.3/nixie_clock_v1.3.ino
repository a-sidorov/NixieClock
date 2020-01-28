/*
  SET
    - удержали в режиме часов - настройка ЧАСОВ
    - удержали в режиме настройки часов - настройка Будильника
    - двойной клик в режиме настройки часов или будильника - вернуться к часам
    - удержали в настройке часов - возврат к часам с новым временем
    - клик во время настройки - смена настройки часов/минут
  UP
    - вкл/выкл будильника
  DOWN
    - посмотреть время будильника

*/
// ************************** НАСТРОЙКИ **************************
#define NIGHT_START 23      // час перехода на ночную подсветку (BRIGHT_N)
#define NIGHT_END 7         // час перехода на дневную подсветку (BRIGHT)
#define FREQ 900            // частота писка будильника
#define REVERSE_TUBES 0     // 1 - зеркально "отразить" отображение времени 

#define CLOCK_TIME 7       // время (с), которое отображаются часы будильника
#define ALM_TIMEOUT 30      // таймаут будильника


// пины
#define PIEZO 10

//#define ALARM 10

#define DECODER0 A3
#define DECODER1 A1
#define DECODER2 A0
#define DECODER3 A2

#define KEY0 11   // точка
#define KEY1 3   // часы
#define KEY2 4    // часы
#define KEY3 5    // минуты
#define KEY4 6    // минуты
#define KEY5 7    // секунды
#define KEY6 8    // секунды

/////////////////////////////////////////////////////////////////////////////////////////

#include "GyverTimer.h"

GTimer_ms modeTimer((long) CLOCK_TIME * 1000);
GTimer_ms dotTimer(500);
GTimer_ms blinkTimer(800);
GTimer_ms almTimer((long) ALM_TIMEOUT * 1000);

#include "GyverButton.h"
GButton btnSet(3, LOW_PULL, NORM_OPEN);
GButton btnUp(3, LOW_PULL, NORM_OPEN);
GButton btnDwn(3, LOW_PULL, NORM_OPEN);

#include <Wire.h>
#include "RTClib.h"
RTC_DS3231 rtc;

#include "EEPROMex.h"

#if (REVERSE_TUBES == 0)
int opts[] = {KEY0, KEY1, KEY2, KEY3, KEY4, KEY5, KEY6};
#else
int opts[] = {KEY6, KEY5, KEY4, KEY3, KEY2, KEY1, KEY0};
#endif

byte counter;
byte digitsDraw[7];
boolean dotFlag;
int8_t hrs = 10, mins = 10, secs;
int8_t alm_hrs = 10, alm_mins = 10;
int8_t mode = 0;    // 0 часы, 2 настройка будильника, 1 настройка часов, 4 аларм
boolean changeFlag;
boolean blinkFlag;
boolean alm_flag;
boolean alm_on;

void setup() {
  Serial.begin(9600);

  TCCR1B = TCCR1B & 0b11111000 | 0x01;

  almTimer.stop();
  btnSet.setTimeout(400);
  btnSet.setDebounce(90);
  rtc.begin();
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  DateTime now = rtc.now();
  secs = now.second();
  mins = now.minute();
  hrs = now.hour();

  pinMode(DECODER0, OUTPUT);
  pinMode(DECODER1, OUTPUT);
  pinMode(DECODER2, OUTPUT);
  pinMode(DECODER3, OUTPUT);
  pinMode(KEY0, OUTPUT);
  pinMode(KEY1, OUTPUT);
  pinMode(KEY2, OUTPUT);
  pinMode(KEY3, OUTPUT);
  pinMode(KEY4, OUTPUT);
  pinMode(KEY5, OUTPUT);
  pinMode(KEY6, OUTPUT);

  pinMode(PIEZO, OUTPUT);
//  pinMode(ALARM, INPUT_PULLUP);

  if (EEPROM.readByte(100) != 66) {   // проверка на первый запуск
    EEPROM.writeByte(100, 66);
    EEPROM.writeByte(0, 0);     // часы будильника
    EEPROM.writeByte(1, 0);// минуты будильника
    EEPROM.writeByte(2, false);
  }
  alm_hrs = EEPROM.readByte(0);
  alm_mins = EEPROM.readByte(1);
  alm_on = EEPROM.readByte(2);
  sendTime();
}

void sendTime() {
  if (mode == 3 || mode == 2) {
    digitsDraw[1] = (byte) alm_hrs / 10;
    digitsDraw[2] = (byte) alm_hrs % 10;

    digitsDraw[3] = (byte) alm_mins / 10;
    digitsDraw[4] = (byte) alm_mins % 10;

    digitsDraw[5] = (byte) 0;
    digitsDraw[6] = (byte) 0;
  } else {
    digitsDraw[1] = (byte) hrs / 10;
    digitsDraw[2] = (byte) hrs % 10;

    digitsDraw[3] = (byte) mins / 10;
    digitsDraw[4] = (byte) mins % 10;

    digitsDraw[5] = (byte) secs / 10;
    digitsDraw[6] = (byte) secs % 10;
  }
  showDigits();
}

void loop() {
  if (hrs > NIGHT_END && hrs < NIGHT_START) {
    analogWrite(9, 230);// min_stable 110 default 130
  } else analogWrite(9, 115);

  calculateTime();
}

void processKeys() {
  int analog = analogRead(A6);
  btnSet.tick(analog >= 900);
  btnUp.tick(analog > 700 && analog < 900);
  btnDwn.tick(analog > 150 && analog < 400);
}

void buttonsTick() {
  processKeys();
  if (mode == 1 || mode == 2) {
    if (btnUp.isClick()) {
      modeTimer.setInterval((long) CLOCK_TIME * 1000);
      blinkFlag = true;
      if (mode == 2) {
        if (!changeFlag) {
          alm_mins++;
          if (alm_mins > 59) {
            alm_mins = 0;
            alm_hrs++;
          }
          if (alm_hrs > 23) alm_hrs = 0;
        } else {
          alm_hrs++;
          if (alm_hrs > 23) alm_hrs = 0;
        }
      } else {
        if (!changeFlag) {
          mins++;
          if (mins > 59) {
            mins = 0;
            hrs++;
          }
          if (hrs > 23) hrs = 0;
        } else {
          hrs++;
          if (hrs > 23) hrs = 0;
        }
        rtc.adjust(DateTime(2014, 1, 21, hrs, mins, 0));
      }
    }

    if (btnDwn.isClick()) {
      modeTimer.setInterval((long) CLOCK_TIME * 1000);
      blinkFlag = true;
      if (mode == 2) {
        if (!changeFlag) {
          alm_mins--;
          if (alm_mins < 0) {
            alm_mins = 59;
            alm_hrs--;
          }
          if (alm_hrs < 0) alm_hrs = 23;
        } else {
          alm_hrs--;
          if (alm_hrs < 0) alm_hrs = 23;
        }
      } else {
        if (!changeFlag) {
          mins--;
          if (mins < 0) {
            mins = 59;
            hrs--;
          }
          if (hrs < 0) hrs = 23;
        } else {
          hrs--;
          if (hrs < 0) hrs = 23;
        }
        rtc.adjust(DateTime(2014, 1, 21, hrs, mins, 0));
      }
    }

    if (blinkTimer.isReady()) {
      if (blinkFlag) blinkTimer.setInterval(800);
      else blinkTimer.setInterval(200);
      blinkFlag = !blinkFlag;
    }
    /*if (blinkFlag) {      // горим
      if (changeFlag) {
        digitsDraw[1] = 10;
        digitsDraw[2] = 10;
      } else {
        digitsDraw[3] = 10;
        digitsDraw[4] = 10;
      }
    }*/
  }

  if (mode == 0 && btnSet.isHolded()) {
    mode = 1;
    modeTimer.setInterval((long) CLOCK_TIME * 1000);
  }

  if ((mode == 1 && btnSet.isHolded())) {
    rtc.adjust(DateTime(2014, 1, 21, hrs, mins, 0));
    sendTime();
    modeTimer.setInterval((long) CLOCK_TIME * 1000);
    mode = 2;
  }

  if (mode == 2 && btnSet.isHolded()) {
    sendTime();
    EEPROM.updateByte(0, alm_hrs);
    EEPROM.updateByte(1, alm_mins);
    alm_on = true;
    EEPROM.updateByte(2, alm_on);
    mode = 0;
  }

  if ((mode == 1 || mode == 2)&& btnSet.isDouble()) {
    sendTime();
    EEPROM.updateByte(0, alm_hrs);
    EEPROM.updateByte(1, alm_mins);
    alm_on = true;
    EEPROM.updateByte(2, alm_on);
    mode = 0;
  }

  if ((mode == 1 || mode == 2) && btnSet.isClick()) {
    changeFlag = !changeFlag;
    modeTimer.setInterval((long) CLOCK_TIME * 1000);
  }

  if (modeTimer.isReady()) {
    if (mode == 1) {
      rtc.adjust(DateTime(2014, 1, 21, hrs, mins, 0));
    } else if (mode == 2) {
      EEPROM.updateByte(0, alm_hrs);
      EEPROM.updateByte(1, alm_mins);
    }
    mode = 0;
  }

  if (mode == 0 && btnUp.isClick()) {
    alm_on = !alm_on;
    EEPROM.updateByte(2, alm_on);
  }

  if (mode == 0 && btnDwn.isClick()) {
    mode = 3;
  }

  if (mode == 3 && btnDwn.isClick()) {
    mode = 0;
  }
}

void calculateTime() {
  if (dotTimer.isReady()) dotFlag = !dotFlag;
  buttonsTick();
  DateTime now = rtc.now();
  secs = now.second();
  mins = now.minute();
  hrs = now.hour();;
  sendTime();

Serial.println(mode);

  if (secs == 0 && mode == 0 ) {      // каждые полчаса
    burnIndicators();    // чистим чистим!
  }

  // Serial.println(alm_hrs);

  if (!alm_flag && alm_mins == mins && alm_hrs == hrs && alm_on) {
    mode = 0;
    alm_flag = true;
    almTimer.start();
    almTimer.reset();
  }

  if (alm_flag) {
    if (almTimer.isReady() || !alm_on) {
      alm_flag = false;
      almTimer.stop();
      mode = 0;
      noTone(PIEZO);
    }
  }

  if (alm_flag) {
    if (!dotFlag) {
      noTone(PIEZO);
      for (byte i = 1; i < 7; i++) digitsDraw[i] = 10;
    } else {
      tone(PIEZO, FREQ);
      sendTime();
    }
  }

}

void burnIndicators() {
  unsigned int lasttime[7];
  for (int i = 1; i < 7; ++i) {
    lasttime[i] = digitsDraw[i];
  }

  for (int i = 1; i < 7; ++i) {
    digitsDraw[i]++;
    while (digitsDraw[i] != lasttime[i]) {
      for (int x = 0; x < 2; x++) {
        showDigits();
      }
      digitsDraw[i]++;
      if (digitsDraw[i] > 9) digitsDraw[i] = 0;
    }
  }
}

void showDigits() {
  if (alm_on)
    digitalWrite(opts[0], HIGH);
  else
    digitalWrite(opts[0], LOW);
  //Выводим часы и мигаем при настройке
  for (int i = 1; i < 3; ++i) {
    setDigit(digitsDraw[i]);
    if (!((mode == 1 || mode == 2) && changeFlag && blinkFlag)) {
      digitalWrite(opts[i], HIGH);
      delay(2);
      //потушим первый индикатор
      digitalWrite(opts[i], LOW);
      delay(1);
    }
  }
//Выводим минуты и мигаем при настройке
  for (int i = 3; i < 5; ++i) {
    setDigit(digitsDraw[i]);
    if (!((mode == 1 || mode == 2) && !changeFlag && blinkFlag)) {
      digitalWrite(opts[i], HIGH);
      delay(2);
      //потушим первый индикатор
      digitalWrite(opts[i], LOW);
      delay(1);
    }
  }
  if (mode == 0) {
    for (int i = 5; i < 7; ++i) {
      setDigit(digitsDraw[i]);
      digitalWrite(opts[i], HIGH);
      delay(2);
      //потушим первый индикатор
      digitalWrite(opts[i], LOW);
      delay(1);
    }
  }
}

// настраиваем декодер согласно отображаемой ЦИФРЕ
void setDigit(byte digit) {
  switch (digit) {
    case 0:digitalWrite(DECODER0, LOW);
      digitalWrite(DECODER1, LOW);
      digitalWrite(DECODER2, LOW);
      digitalWrite(DECODER3, LOW);
      break;
    case 1:digitalWrite(DECODER0, HIGH);
      digitalWrite(DECODER1, LOW);
      digitalWrite(DECODER2, LOW);
      digitalWrite(DECODER3, LOW);
      break;
    case 2:digitalWrite(DECODER0, LOW);
      digitalWrite(DECODER1, HIGH);
      digitalWrite(DECODER2, LOW);
      digitalWrite(DECODER3, LOW);
      break;
    case 3:digitalWrite(DECODER0, HIGH);
      digitalWrite(DECODER1, HIGH);
      digitalWrite(DECODER2, LOW);
      digitalWrite(DECODER3, LOW);
      break;
    case 4:digitalWrite(DECODER0, LOW);
      digitalWrite(DECODER1, LOW);
      digitalWrite(DECODER2, HIGH);
      digitalWrite(DECODER3, LOW);
      break;
    case 5:digitalWrite(DECODER0, HIGH);
      digitalWrite(DECODER1, LOW);
      digitalWrite(DECODER2, HIGH);
      digitalWrite(DECODER3, LOW);
      break;
    case 6:digitalWrite(DECODER0, LOW);
      digitalWrite(DECODER1, HIGH);
      digitalWrite(DECODER2, HIGH);
      digitalWrite(DECODER3, LOW);
      break;
    case 7:digitalWrite(DECODER0, HIGH);
      digitalWrite(DECODER1, HIGH);
      digitalWrite(DECODER2, HIGH);
      digitalWrite(DECODER3, LOW);
      break;
    case 8:digitalWrite(DECODER0, LOW);
      digitalWrite(DECODER1, LOW);
      digitalWrite(DECODER2, LOW);
      digitalWrite(DECODER3, HIGH);
      break;
    case 9:digitalWrite(DECODER0, HIGH);
      digitalWrite(DECODER1, LOW);
      digitalWrite(DECODER2, LOW);
      digitalWrite(DECODER3, HIGH);
      break;
    case 10:digitalWrite(DECODER0, LOW);
      digitalWrite(DECODER1, HIGH);
      digitalWrite(DECODER2, HIGH);
      digitalWrite(DECODER3, HIGH);
  }
}

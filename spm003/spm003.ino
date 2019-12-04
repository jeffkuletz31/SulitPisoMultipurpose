/*
    Project     :   Sulit Piso Multipurpose
    Version     :   2.5

    Created by  :   Rhalf Wendel Caacbay
    Email       :   rhalfcaacbay@gmail.com

*/
#include<LiquidCrystal_I2C.h>
//#include<U8g2lib.h>
#include<Timer.h>
#include<Terminal.h>
#include<Buzzer.h>
#include<BillCoinAcceptor.h>
#include<Storage.h>
#include<Protocol.h>
#include<Button.h>
#include<Device.h>
#include<Helper.h>
#include<WatchDog.h>

//U8G2_ST7920_128X64_1_SW_SPI u8g2(U8G2_R0, /* clock=*/ 12, /* data=*/ 11, /* CS=*/ 10, /* reset=*/ 100);
Timer tDisplay(Timer::MILLIS), tInterrupt(Timer::MILLIS), tLimit(Timer::MILLIS), tPower(Timer::MILLIS);
Terminal terminals[4] = {Terminal(A0), Terminal(A1), Terminal(A2), Terminal(A3)};
Buzzer buzzer = Buzzer(13, 1875, 50);
BillCoinAcceptor coinAcceptor = BillCoinAcceptor(2);
Storage storage = Storage();
Protocol protocol = Protocol(9, 8);
Button buttons[4] = {Button(4), Button(5), Button(6), Button(7)};
Helper helper = Helper();
LiquidCrystal_I2C lcd(0x27, 25, 4);

uint8_t index = 0;
String space = " ";
bool isLimit = false;

void cbLimit() {
  uint32_t amount = storage.getCurrentAmount();
  uint32_t limit = storage.getLimit();
  if (amount >= limit) isLimit = true;
  else isLimit = false;
}

void cbPower() {
  //standby power consumption of device is 3watts therefore
  //float power = 50.0wh
  storage.incrementPower(50); //50 milliwatts per hour
  storage.incrementTime(3600);  // 3600 seconds per hour
}


void cbDisplay() {
  cbLcd1602();
}


void cbLcd1602() {
  if (Timer::getSeconds() < 3) {
    lcd.setCursor(0, 0);
    lcd.print(helper.padding(Device::getCompany(), space, 16));
    lcd.setCursor(0, 1);
    lcd.print(helper.padding(Device::getCountry(), space, 16));
  } else if (Timer::getSeconds() >= 3 && Timer::getSeconds() < 6) {
    lcd.setCursor(0, 0);
    lcd.print(helper.padding(Device::getTrans(), space, 16));
    lcd.setCursor(8, 0);
    lcd.print(storage.getCurrentTransA());
    lcd.setCursor(0, 1);
    lcd.print(helper.padding(Device::getAmount(), space, 16));
    lcd.setCursor(8, 1);
    lcd.print(helper.toUtf8Currency(storage.getCurrentAmount()));
  }  else if (Timer::getSeconds() >= 6 && Timer::getSeconds() < 9) {
    lcd.setCursor(0, 0);
    lcd.print(helper.padding(Device::getServe(), space, 16));
    lcd.setCursor(8, 0);
    lcd.print(helper.padding(helper.toUtf8Time(storage.getCurrentServe()), space, 16));
    lcd.setCursor(0, 1);
    lcd.print(helper.padding(Device::getCredit(), space, 16));
    lcd.setCursor(8, 1);
    lcd.print(helper.toUtf8Currency(storage.getCurrentCredit()));
//  } else if (Timer::getSeconds() >= 9 && Timer::getSeconds() < 12) {
//    lcd.setCursor(0, 0);
//    lcd.print(helper.padding(Device::getPower(), space, 16));
//    lcd.setCursor(8, 0);
//    float pKwh  = storage.getPkwh() / 100.0;
//    float power = storage.getCurrentPower() / 1000.0 / 1000.0;
//    lcd.print(pKwh * power);
//    lcd.setCursor(0, 1);
//    lcd.print(helper.padding("", space, 16));

  } else {
    lcd.setCursor(0, 0);
    lcd.print(helper.padding(Device::getCoin(), space, 16));
    
    lcd.setCursor(8, 0);
    lcd.print(helper.padding(helper.toUtf8Currency(coinAcceptor.coinPulse), space, 16));

    if (terminals[0].timeLapse > 0) {
      lcd.setCursor(0, 1);
      lcd.print(helper.padding(Device::getTime(), space, 16));
      lcd.setCursor(8, 1);
      lcd.print(helper.padding(helper.toUtf8Time(terminals[0].timeLapse), space, 16));

    } else {
      if (storage.getMode() == 0) {
        if (coinAcceptor.coinPulse > 0) {
          lcd.setCursor(0, 1);
          lcd.print(helper.padding(Device::getTime(), space, 16));
          lcd.setCursor(8, 1);
          lcd.print(helper.padding(helper.toUtf8Time(coinAcceptor.coinPulse * storage.getRate()), space, 16));
        } else {
          lcd.setCursor(0, 1);
          lcd.print(helper.padding(Device::getTime(), space, 16));
          lcd.setCursor(8, 1);

          if (isLimit) lcd.print(helper.padding(Device::getLimit(), space, 16));
          else lcd.print(helper.padding(Device::getVacant(), space, 16));
        }
      } else {
        lcd.setCursor(0, 1);
        lcd.print(helper.padding(Device::getTime(), space, 16));
        lcd.setCursor(8, 1);
        if (isLimit) lcd.print(helper.padding(Device::getLimit(), space, 16));
        else lcd.print(helper.padding(Device::getFree(), space, 16));
      }
    }
  }
}

void cbInterrupt() {
  for (index = 0; index < 4; index++) {
    //Buttons
    buttons[index].run();
    //Terminals
    terminals[index].run();
  }
}

void serialEvent() {
  if (Serial.available()) {
    char chr = (char) Serial.read();
    protocol.buffer += chr;
    if (chr == '\n') {
      Serial.print(">>" + protocol.buffer);
      protocol.interpret();
      Serial.print("<<" + protocol.buffer);
      protocol.buffer = "";
    }
  }
}

void onReceived(void) {
  Serial.print(">>" + protocol.buffer);
  protocol.interpret();
  Serial.print("<<" + protocol.buffer);
  protocol.print(protocol.buffer);
  protocol.buffer = "";
}

void onCoin() {
  buzzer.play();
  coinAcceptor.readCoinPulse();
}

void onShortPressed(uint8_t pin) {
  //Process
  for (index = 0; index < 4; index++) {
    if (buttons[index].getPin() == pin) {
      // check if money is minimum
      if (!terminals[index].getState())
        if (coinAcceptor.coinPulse < storage.getMinimum())
          continue;

      uint32_t coinValue = coinAcceptor.coinPulse;
      uint32_t timeValue = coinValue * storage.getRate();

      //add to record
      if (storage.getMode() == 0) {
        if (coinAcceptor.coinPulse == 0 ) return;
        storage.incrementAmount(coinValue);
      }

      if (storage.getMode() == 1) {
        uint8_t value = 1;
        storage.incrementFree(value);
        timeValue = value * storage.getRate();
      }

      //process
      buzzer.play();

      //charger consumes 20watts per hour
      //float power = (20000.0 / 3600.0) * timeValue;
      //power = 5.56
      storage.incrementPower(6 * timeValue);

      //trigger
      terminals[index].set(timeValue);
      coinAcceptor.coinPulse = 0;
    }
  }
}

void onLongPressed(uint8_t pin) {
  for (index = 0; index < 4; index++) {
    if (buttons[index].getPin() == pin) {
      buzzer.play();
      terminals[index].reset();
    }
  }
}

void setup() {
  // put your setup code here, to run once:

  if (storage.getFirst() != 1) {
    storage.format(190807);
    storage.setFirmware(25);
    storage.setFirst(1);
  }

  buzzer.play();
  Serial.begin(9600);

  protocol.onReceived(onReceived);
  protocol.begin(9600);

  // u8g2.begin();
  // u8g2.enableUTF8Print();
  lcd.init();
  lcd.backlight();

  coinAcceptor.attach(onCoin);
  protocol.terminals = terminals;

  tDisplay.begin(Timer::FOREVER, 1000, cbDisplay);
  tInterrupt.begin(Timer::FOREVER, 25, cbInterrupt);

  tLimit.begin(Timer::FOREVER, 10000, cbLimit);
  tPower.begin(Timer::FOREVER, 3600, cbPower);

  tDisplay.start();
  tInterrupt.start();

  tLimit.start();
  tPower.start();

  for (index = 0; index < 4; index++) {
    //terminals[index].setActiveState(false);
    //buttons[index].setActiveState(false);
    buttons[index].setOnShortPressed(onShortPressed);
    buttons[index].setOnLongPressed(onLongPressed);
  }

  WatchDog::enable(WatchDog::S002);
}

void loop() {
  // put your main code here, to run repeatedly:
  tDisplay.run();
  tInterrupt.run();

  tLimit.run();
  tPower.run();

  protocol.run();

  WatchDog::reset();
}

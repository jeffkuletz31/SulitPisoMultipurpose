#include<LiquidCrystal_I2C.h>
#include<Timer.h>
#include<Terminal.h>
/*
    Project     :   Sulit Piso Multipurpose
    Version     :   2.0

    Created by  :   Rhalf Wendel Caacbay
    Email       :   rhalfcaacbay@gmail.com

*/
#include<Buzzer.h>
#include<BillCoinAcceptor.h>
#include<Storage.h>
#include<Protocol.h>
#include<Button.h>
#include<Device.h>
#include<Helper.h>
#include<WatchDog.h>

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
  uint32_t gross = storage.getCurrentGross();
  uint32_t limit = storage.getLimit();
  if (gross >= limit) {
    isLimit = true;
    buzzer.play();
  } else {
    isLimit = false;
  }
}

void cbPower() {
  //standby power consumption of device is 3watts therefore
  //float power = 3000.0 / 60.0;
  //power = 50
  storage.incrementPower(50);
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
    lcd.print(helper.padding(Device::getTransaction(), space, 16));
    lcd.setCursor(8, 0);
    lcd.print(storage.getCurrentTransaction());
    lcd.setCursor(0, 1);
    lcd.print(helper.padding(Device::getGross(), space, 16));
    lcd.setCursor(8, 1);
    lcd.print(helper.toUtf8Currency(storage.getCurrentGross()));
  }  else if (Timer::getSeconds() >= 6 && Timer::getSeconds() < 9) {
    lcd.setCursor(0, 0);
    lcd.print(helper.padding(Device::getServingTime(), space, 16));
    lcd.setCursor(8, 0);
    lcd.print(helper.padding(helper.toUtf8Time(storage.getCurrentServe()), space, 16));
    lcd.setCursor(0, 1);
    lcd.print(helper.padding(Device::getCredit(), space, 16));
    lcd.setCursor(8, 1);
    lcd.print(helper.toUtf8Currency(storage.getCurrentCredit()));
  } else if (Timer::getSeconds() >= 9 && Timer::getSeconds() < 12) {
    lcd.setCursor(0, 0);
    lcd.print(helper.padding(Device::getPower(), space, 16));
    lcd.setCursor(8, 0);
    lcd.print(storage.getCurrentPower() / 1000.0);
    lcd.setCursor(0, 1);
    lcd.print(helper.padding("", space, 16));

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
      Serial.print("<<" + protocol.buffer);
      protocol.interpret();
      Serial.print(">>" + protocol.buffer);
      protocol.buffer = "";
    }
  }
}

void onReceived(void) {
  Serial.print("<<" + protocol.buffer);
  protocol.interpret();
  protocol.send();
  Serial.print(">>" + protocol.buffer);
}

void onCoin() {
  buzzer.play();
  coinAcceptor.readCoinPulse();
}

void onShortPressed(uint8_t pin) {
  coinAcceptor.coinPulse += storage.getMode();

  //check if coin inserted
  if (coinAcceptor.coinPulse == 0 ) return;
  //Process
  for (index = 0; index < 4; index++) {
    if (buttons[index].getPin() == pin) {
      // check if money is minimum
      if (!terminals[index].getState())
        if (coinAcceptor.coinPulse < storage.getMinimum())
          continue;
      //process
      buzzer.play();

      uint32_t coinValue = coinAcceptor.coinPulse;
      uint32_t timeValue = coinAcceptor.coinPulse * storage.getRate();

      //add to record
      if (storage.getMode() == 0) storage.incrementGross(coinValue);
      if (storage.getMode() >= 1) storage.incrementCredit(coinValue);

      storage.incrementTransaction(1);
      storage.incrementServe(timeValue);


      //charger consumes 500watts per hour
      //float power = (500000.0 / 3600.0);
      // carwash power = 138.90
      storage.incrementPower(140 * timeValue);

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
  buzzer.play();
  Serial.begin(9600);

  protocol.setOnReceived(onReceived);
  protocol.begin(9600);

  lcd.init();
  lcd.backlight();

  coinAcceptor.attach(onCoin);
  protocol.terminals = terminals;

  tDisplay.begin(Timer::FOREVER, 1000, cbDisplay);
  tInterrupt.begin(Timer::FOREVER, 25, cbInterrupt);

  tLimit.begin(Timer::FOREVER, 10000, cbLimit);
  tPower.begin(Timer::FOREVER, 60000, cbPower);

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

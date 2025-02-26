/*
 *  _ _ _ _ _ _ _ _
 * _RUN__12346578_      /16
 * _OFF__12345678_      /16
 * 
 * _COUNT:_0000__O_     /16
 * _COUNT:_9999__L_     /16
 * 
 * 
 * 
 * 
 * сжали                                                
 * запайка / 2-3 сек                                    
 * вк.шнек / на 5 оборотов (сколь времени?)             
 * вк.вниз / плавно                                     
 * вык.шнек / 5 об. + обнулить счетчик                  
 * вык.вниз / по концевику
 * разжали
 * вкл. вверх / быстро
 * вык.вверх / по концевику
 * 
 * 
 * 
 * 
 * bool clamp (1/0)
 * byte sealing (500...5000)
 * byte feed (0...20)
 * byte pwm (0..255)
 * bool lift (1/0)
 */


#include <GyverIO.h>
#include <EncButton.h>

/*-----( Import needed libraries )-----*/
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

#include <EEPROM.h>  // импортируем библиотеку


/*-----( Variables )-----*/

#define BA A3  // Enc A-right
#define BB A2  // Enc button
#define BC A1  // Enc B-left

#define out_air 6       // Клапан возд.
#define out_feed 7      // Шнек вкл.
#define out_heater 8    // ТЭН вкл.
#define out_down 5      // вверх
#define out_up 4        // вниз
#define out_enable 3    // вкл мотора
#define out_rezz 9      // резерв


#define EB_DEB 50       // debounce timeout
#include <EncButton.h>
EncButton eb(BA, BC, BB);
Button key_up(10);      // верхний концевик
Button key_down(11);    // Нижний концевик
Button key_feed(12);    // конт шнека

byte menu = 1;
bool menu_set = 0;
byte setValue = 0;
bool setRUN = 0;

bool up_limit = 0;
bool dowm_limit = 0;

// DATA STATE
bool clamp; //(1/0)          - air valve state
bool lift_dir; //(1/0)       - lift direction

unsigned long tmr1;
unsigned long tmr2;
unsigned long tmr3;
unsigned long tmr4;

byte state = 0;

//-----------

byte r_on[] = {
  B00000,
  B01110,
  B01010,
  B01010,
  B01010,
  B01010,
  B01010,
  B11111
};
byte r_off[] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B01110,
  B11111
};



/*-----( EEPROM )------*/
struct Settings {
  byte total_count = 0;         /*(0..9999) Счетчик циклов*/
  bool mode = 0;                /*(0/1) once or loop, default: 0(loop) */
  byte feed_count = 3;          /*(0..10) Кол-во оборотов шнека*/
  byte lift_pwm = 130;            /*(0..255) Скорость опускания*/
  int delay_air = 3000;        /*(0..255) Время клапанов*/
  int delay_heater = 3000;     /*(500..2500 ms) Время запайки*/
};
Settings SET;





void setup() {
  Serial.begin(9600);

  pinMode(BA, INPUT_PULLUP);
  pinMode(BB, INPUT_PULLUP);
  pinMode(BC, INPUT_PULLUP);



  pinMode(key_up, INPUT);
  pinMode(key_dwn, INPUT);
  pinMode(key_feed, INPUT);

  pinMode(out_air, OUTPUT);
  pinMode(out_feed, OUTPUT);
  pinMode(out_heater, OUTPUT);
  pinMode(out_rezz, OUTPUT);

  pinMode(out_down, OUTPUT);
  pinMode(out_up, OUTPUT);
  pinMode(out_enable, OUTPUT);

  digitalWrite(out_air, 0);
  digitalWrite(out_feed, 0);
  digitalWrite(out_heater, 0);
  digitalWrite(out_rezz, 0);


  //dataErise();
  //dataWrite();
  //dataRead();
  //sensorRead();
  //dataDebug();





  lcd.init();
  lcd.backlight();
  lcd.createChar(0, r_off);
  lcd.createChar(1, r_on);
  // lcd.createChar(2, sSen);
  // lcd.createChar(3, ten1);
  // lcd.createChar(4, ten2);
  // lcd.createChar(5, ten3);
  // lcd.createChar(6, sDuw);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Load---");
  lcd.clear();
  updateMenu();

  // сбросить счётчик энкодера
  eb.counter = 0;

  //dataDebug();
}

void loop() {
  eb.tick();
  key_up.tick();
  key_down.tick();
  key_feed.tick();

  if (eb.hold()) {
    //Serial.println("Hold");
    if (menu_set == false) {
      menu_set = true;
    } else {
      menu_set = false;
      menu = 1;
      dataWrite();  // Записываем в EEPROM
      //dataDebug();  // Выводим в порт для проверки
    }
    updateMenu();
  }

  if (eb.click()) {
    //Serial.println("Select");

    if (menu_set == 1) {
      menu++;
      updateMenu();


    } else {
      //Кнопка пуск
      setRUN = !setRUN;
      test_lift();
    }
  }

  if (eb.right() && menu_set) {
    //Serial.println("Right");
    setValue++;
    dataSET(0);
  }
  if (eb.left() && menu_set) {
    //Serial.println("Left");
    setValue++;
    dataSET(1);
  }

  if (millis() - tmr3 >= 1000) {  // 1 sec.
    tmr3 = millis();
    digitalWrite(13, !digitalRead(13));
    updateMenu();
  }
  if (setRUN) {
    digitalWrite(13,1); //LED START ON
    //execute();
    
  }else{
    digitalWrite(13,0); //LED START OFF
  }
}


void updateMenu() {
  lcd.clear();
  if (menu_set) {
    switch (menu) {
      case 0:
        menu = 1;
        break;

      case 1:
        lcd.setCursor(0, 0);
        lcd.print(char(126));
        lcd.print(" Counter reset: "); 
        lcd.setCursor(0, 1);
        lcd.print("  [ ");
        lcd.print(SET.total_count);
        lcd.print(" ]");
        break;

      case 2:
        lcd.setCursor(0, 0);
        lcd.print(char(126));
        lcd.print(" Select MODE: ");
        lcd.setCursor(0, 1);
        lcd.print("  [ ");
        lcd.print(SET.mode);
        lcd.print(" ] ");
        break;

      case 3:
        lcd.setCursor(0, 0);
        lcd.print(char(126));
        lcd.print(" Feed counter: ");
        lcd.setCursor(0, 1);
        lcd.print("  [ ");
        lcd.print(SET.feed_count);
        lcd.print(" ]");
        break;

      case 4:
        lcd.setCursor(0, 0);
        lcd.print(char(126));
        lcd.print(" Lift speed: ");
        lcd.setCursor(0, 1);
        lcd.print("  [ ");
        lcd.print(SET.lift_pwm);
        lcd.print(" ]");
        break;

      case 5:
        lcd.setCursor(0, 0);
        lcd.print(char(126));
        lcd.print(" Delay Air: ");
        lcd.setCursor(0, 1);
        lcd.print("  [ ");
        lcd.print(SET.delay_air);
        lcd.print(" ]");
        break;

      case 6:
        lcd.setCursor(0, 0);
        lcd.print(char(126));
        lcd.print(" Delay Heater: ");
        lcd.setCursor(0, 1);
        lcd.print("  [ ");
        lcd.print(SET.delay_heater);
        lcd.print(" ]");
        break;


      case 7:
        menu = 1;
        lcd.setCursor(0, 0);
        lcd.print(char(126));
        lcd.print(" Counter reset: "); 
        lcd.setCursor(0, 1);
        lcd.print("  [ ");
        lcd.print(SET.total_count);
        lcd.print(" ]");
        break;
    }
  } else {

    // Run
    lcd.setCursor(0, 0);
    if (setRUN) {
      lcd.print("On ");
    } else {
      lcd.print("Off ");
    }

    switch (state){
      case 0:
      lcd.print("-----");
      break;

      case 1:
      lcd.print(char(1));
      lcd.print(char(0));
      lcd.print(char(0));
      lcd.print(char(0));
      lcd.print(char(0));
      break;

      case 2:
      lcd.print(char(1));
      lcd.print(char(1));
      lcd.print(char(0));
      lcd.print(char(0));
      lcd.print(char(0));
      break;

      case 3:
      lcd.print(char(1));
      lcd.print(char(1));
      lcd.print(char(1));
      lcd.print(char(0));
      lcd.print(char(0));
      break;

      case 4:
      lcd.print(char(1));
      lcd.print(char(1));
      lcd.print(char(1));
      lcd.print(char(1));
      lcd.print(char(0));
      break;

      case 5:
      lcd.print(char(1));
      lcd.print(char(1));
      lcd.print(char(1));
      lcd.print(char(1));
      lcd.print(char(1));
      break;
    }
    
    lcd.print(" ");
    lcd.print(state);
    lcd.print(" ");

    lcd.setCursor(1, 1);
    lcd.print("COUNT: ");
    lcd.print(SET.total_count);
  }
}

void dataSET(boolean setType) {
  switch (menu) {
    case 1:  // set RC
      if (setType == 0) {
        SET.total_count = constrain(SET.total_count + setValue, 0, 999);
        setValue = 0;
        updateMenu();
      } else {
        SET.total_count = constrain(SET.total_count - setValue, 0, 999);
        setValue = 0;
        updateMenu();
      }

      break;

    case 2:  // set RL1
      if (setType == 0) {
        SET.mode = constrain(SET.mode + setValue, 0, 1);
        setValue = 0;
        updateMenu();
      } else {
        SET.mode= constrain(SET.mode - setValue, 0, 1);
        setValue = 0;
        updateMenu();
      };
      break;

    case 3:  // set RL2
      if (setType == 0) {
        SET.feed_count = constrain(SET.feed_count + setValue, 0, 10);
        setValue = 0;
        updateMenu();
      } else {
        SET.feed_count = constrain(SET.feed_count - setValue, 0, 10);
        setValue = 0;
        updateMenu();
      };
      break;

    case 4:  // set RL3
      if (setType == 0) {
        SET.lift_pwm = constrain(SET.lift_pwm + 10*setValue, 100, 255);
        setValue = 0;
        updateMenu();
      } else {
        SET.lift_pwm = constrain(SET.lift_pwm - 10*setValue, 100, 255);
        setValue = 0;
        updateMenu();
      };
      break;

    case 5:  // set RL4
      if (setType == 0) {
        SET.delay_air = constrain(SET.delay_air + 100*setValue, 100, 5000);
        setValue = 0;
        updateMenu();
      } else {
        SET.delay_air = constrain(SET.delay_air - 100*setValue, 100, 5000);
        setValue = 0;
        updateMenu();
      };
      break;

      case 6:  // set RL4
      if (setType == 0) {
        SET.delay_heater = constrain(SET.delay_heater + 100*setValue, 100, 5000);
        setValue = 0;
        updateMenu();
      } else {
        SET.delay_heater = constrain(SET.delay_heater - 100*setValue, 100, 5000);
        setValue = 0;
        updateMenu();
      };
      break;
  }
}

void dataDebug() {
  Serial.println(" ");
  Serial.print("setTMP: ");
  Serial.println(EEPROM.read(10));
  Serial.print("setHUM: ");
  Serial.println(EEPROM.read(11));
  Serial.print("Mode: ");
  Serial.println(EEPROM.read(12));
  Serial.print("Gist: ");
  Serial.println(EEPROM.read(13));
  Serial.print("setTC: ");
  Serial.println(EEPROM.read(14));
  Serial.print("setHC: ");
  Serial.println(EEPROM.read(16));
  Serial.println(" ");
}

void dataRead() {
  SET.total_count = EEPROM.read(10);
  SET.mode = EEPROM.read(11);
  SET.feed_count = EEPROM.read(12);
  SET.lift_pwm = EEPROM.read(13);
  SET.delay_air = EEPROM.read(14);
  SET.delay_heater = EEPROM.read(16);
}

void dataWrite() {
  EEPROM.put(10, SET.total_count);
  EEPROM.put(11, SET.mode);
  EEPROM.put(12, SET.feed_count);
  EEPROM.put(13, SET.lift_pwm);
  EEPROM.put(14, SET.delay_air);
  EEPROM.put(16, SET.delay_heater);
}

void dataWriteDemo() {
  EEPROM.put(10, 37);
  EEPROM.put(11, 10);
  EEPROM.put(12, 3);
  EEPROM.put(13, 2);
  EEPROM.put(14, 0);
  EEPROM.put(16, 0);
}

void dataErise() {
  EEPROM.put(10, 0);
  EEPROM.put(11, 0);
  EEPROM.put(12, 0);
  EEPROM.put(13, 0);
  EEPROM.put(14, 0);
  EEPROM.put(15, 0);
  EEPROM.put(16, 0);
  EEPROM.put(17, 0);
}


void test_lift(){

  if (setRUN == true){
    digitalWrite(out_rezz, 1);
    //Serial.println("start");
    if(state == 0){
      Serial.println("111");
      
      if(key_up.click()) {
      //  Serial.println("2222");
        do{
          digitalWrite(out_up, 0);
          digitalWrite(out_down, 1);
          analogWrite (out_enable,50);
          Serial.println("3333");
          Serial.println(digitalRead(key_up)); 
        } while (digitalRead(key_up) == 0);
        
        digitalWrite(out_up, 0);
        digitalWrite(out_down, 0);
        analogWrite (out_enable,0);

        Serial.println("st_1");
        state = 1;
      //}
    }

    if(key_up.click()) {
      digitalWrite(out_up, 0);
      digitalWrite(out_down, 0);
      analogWrite (out_enable,0);
      up_limit = 1;
    }else{
      up_limit = 2;
    }
    if(key_down.click()) {
      digitalWrite(out_up, 0);
      digitalWrite(out_down, 0);
      analogWrite (out_enable,0);
      down_limit = 1;
    }else{
      down_limit = 2;
    }

    if (state == 1){
      //сжатие
      digitalWrite(out_air, 1);
      Serial.println("air");
      delay(2000);
      digitalWrite(out_heater, 1);
      Serial.println("heater");
      delay(SET.delay_heater);
      delay(3000);
      digitalWrite(out_heater, 0);
      Serial.println("heater off");
      
      // if (millis() - tmr3 >= SET.delay_heater && state == 1) {  // 1 sec.
      //   tmr3 = millis();
      //   digitalWrite(out_heater, 0);
      //   state = 2;
      //   Serial.println("st_2");
      // }

      state = 2;
      Serial.println("st_2");
    }

    if (state == 2){
      int i = 0;
      digitalWrite(out_feed, 1);
      do {
        i = i + digitalRead(key_feed);
        Serial.println("feed"); 
        Serial.println(i); 

        digitalWrite(out_up, 1);
        digitalWrite(out_down, 0);
        analogWrite (out_enable,  SET.lift_pwm);

      } while (i < SET.feed_count);

      digitalWrite(out_feed, 0);

      do{
        digitalWrite(out_up, 0);
        digitalWrite(out_down, 0);
        analogWrite (out_enable,  0);
      } while (digitalRead(key_dwn) == 0);

      state = 3;
      Serial.println("st_3");     
    }

      //digitalWrite(out_up, 1);
      //digitalWrite(out_down, 0);
      //analogWrite (out_enable,  SET.lift_pwm); 

  }
  
  if (setRUN == false){
      digitalWrite(out_up, 0);
      digitalWrite(out_down, 0);

      digitalWrite(out_feed, 0);
      digitalWrite(out_heater, 0);
      digitalWrite(out_air, 0);
      state = 0;
      digitalWrite(out_rezz, 0);
      Serial.println("STOP");
  }
  updateMenu();
  
}


void execute() {
  
    if(digitalRead(key_up) == 0){
      state = 0;
      //Motor_RUN
    }else{
      state = 1;
    }

    if(state == 1){
      digitalWrite(out_air, 1);   //air_clamp on
      delay(SET.delay_air);
      state = 2;
    }

    if(state == 2){
      digitalWrite(out_heater, 1);   //out_heater on
      delay(SET.delay_heater);
      digitalWrite(out_heater, 0);   //out_heater off
      state = 3;
    }

    if(state == 3){
      //key_feed = 0;
      // Feed RUN
      // lift_down
      // Feed STOP
      if(digitalRead(key_dwn) == 0){
        // lift_down 
      }
      state = 4;
    }

    updateMenu();

}//end_execute
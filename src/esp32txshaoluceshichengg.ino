#include <RDA5807.h>
#include <Battery18650Stats.h>
#include <TFT_eSPI.h>
#include <HamShield.h>
#include <Arduino.h>
#include <OneButton.h>
#include <Preferences.h>
#include <RotaryEncoder.h>
#include <esp_task_wdt.h>

#define SW1_BUTTON_PIN 32
#define SW2_BUTTON_PIN 35
#define SW3_BUTTON_PIN 16
#define PUSH_BUTTON_PIN 36
#define RE_PIN1 34//编码器
#define RE_PIN2 39//编码器
#define ADC_PIN 25//ADC采集
#define LCD_BLK 4
#define LCD_CS 15
#define AMP_PIN 33//功放引脚
#define EN_1846 26//1846电源
#define EN_5807 27//5807电源
#define PWR_ON 12//MCU电源控制引脚
#define SENB 17//1846CS引脚
#define RX_ON 19//
#define TX_ON 23//
#define S_SDA 21//I2C SDA
#define S_SCL 22//I2C SCLK
#define FACTOR 13//偏频修正系数
#define color1 0xC638

bool state_1846_5087 ; //5807为真1846为假，方便判断当前模式
bool doubleclick_up;//双击按钮后为真，方便编码器判断当前是否调整音量
bool seeking;//双击上按钮进入搜台模式
bool seeking_executed;//标记搜台是否已经执行过
bool modetx;//双击下按钮进入发射模式
unsigned long previousMillis = 0; // 存储上一次执行的时间
const unsigned long interval = 20000; // 设置关闭LCD屏幕间隔时间为20000毫秒（20秒）
unsigned long adj_previousMillis = 0; // 存储上一次执行的时间
const unsigned long rssi_interval = 500; // 设置读取信号时间（0.5秒）
unsigned long rssi_previousMillis = 0; // 存储上一次执行的时间
const unsigned long adj_interval = 10000; // 设置退出音量调整时间（10秒）
// 本项目仅供学习，请勿在任何可能影响无线电正常通信的地方测试此项目
// Regulatory compliance: This project is for learning purposes only
// Note: These arrays cannot be const because they are modified at runtime
int station_1846_tx[20]={409750,409762,409775,409787,409800,409812,409825,409837,409850,409862,409875,409887,409900,409912,409925,409937,409950,409962,409975,409987};
int station_1846_rx[20]={409750,409762,409775,409787,409800,409812,409825,409837,409850,409862,409875,409887,409900,409912,409925,409937,409950,409962,409975,409987};
int station_5807[20]={409750,409762,409775,409787,409800,409812,409825,409837,409850,409862,409875,409887,409900,409912,409925,409937,409950,409962,409975,409987};
const uint32_t vhf_low=200000,vhf_heigh=260000;
const uint32_t uhf_low=400000,uhf_heigh=520000;
const uint32_t whf_low=134000,whf_heigh=174000;
uint32_t read_5807=8700;
uint32_t read_1846=400000;
String band_1846;
const float adcVoltageRange = 3.3;// ESP32的ADC参考电压，通常为3.3V或板载电压
const int adcResolution = 12; // ESP32的ADC分辨率，通常为12位
float freq=0.00;//
float hfreq;
uint32_t finally_freq_5807,finally_freq_1846;
int strength=0;
int value=996;
int channel=0;
bool muted=0;
bool adj_volume;
int volume=1;
int step_5807=100;
int step_1846=1;
const int arraySize = 126;  // 最大可能的 I2C 设备数量
uint8_t addressArray[arraySize];  // 定义数组
float hamfreq=0.00;
int batteryLevel=0;
float conv_factor;
int factor_1846;
// 定义点击次数计数器
int clickCount = 0;
// 定义目标点击次数
const int targetClickCount = 5;
HamShield hamShield;
RDA5807 radio5807;
Battery18650Stats battery;
Preferences prefs;//永久储存库
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);//屏幕
OneButton middle_button(SW1_BUTTON_PIN, true);//中按钮
OneButton down_button(SW2_BUTTON_PIN, true);//下按钮
OneButton up_button(SW3_BUTTON_PIN, true);//上按钮
OneButton push_button(PUSH_BUTTON_PIN, true);//编码器按钮
RotaryEncoder encoder(RE_PIN1, RE_PIN2, RotaryEncoder::LatchMode::TWO03);//编码器

// 对讲机搜台函数声明
uint32_t seek_1846_down();
uint32_t seek_1846_up();

void setup() {
  // 启用看门狗定时器 (30秒超时)
  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL);

  pinMode(EN_5807, OUTPUT);
  digitalWrite(EN_5807, HIGH);//5807电源开机
  Serial.begin(9600);

  seeking_executed = false; // 初始化搜台执行状态

  pinMode(PWR_ON,OUTPUT);//初始化pw引脚
  digitalWrite(PWR_ON, LOW);//MUC电源保持
  delay(2000);//等待2秒
  digitalWrite(PWR_ON, HIGH);//MUC电源保持
  Wire.begin(S_SDA,S_SCL);//ham库非标准i2c，重新启动标准i2c
  button_reset();//初始化按钮

  pinMode(AMP_PIN, OUTPUT);
  digitalWrite(AMP_PIN, HIGH);//功放电源开机
  pinMode(LCD_BLK, OUTPUT);
  digitalWrite(LCD_BLK, HIGH);
  pinMode(LCD_CS, OUTPUT);
  digitalWrite(LCD_CS, HIGH);
  pinMode(0, INPUT_PULLUP);

  prefs.begin("frequency");//初始化存储库

  // 修复除零风险：添加默认值和验证
  int temp_conv_factor = prefs.getInt("conv_factor", 1300); // 默认值 1.30
  if(temp_conv_factor == 0) {
    temp_conv_factor = 1300; // 防止除零
  }
  conv_factor = temp_conv_factor / 100.0;
  battery=Battery18650Stats(ADC_PIN,conv_factor);//电池
  read_station_1846_rx();
  read_station_5807();
  tft.begin();
  tft.writecommand(0x11);//st7789屏使用此行代码
  tft.setRotation(3);//st7789屏使用此行代码
  tft.fillScreen(TFT_BLACK);
  setup_1846();


  spr.createSprite(320,170);
  spr.setTextDatum(4);
  spr.setSwapBytes(true);
  spr.setFreeFont(&Orbitron_Light_24);
  spr.setTextColor(color1,TFT_BLACK);

  drawSprite();
}

void loop() {
  push_button.tick();
  up_button.tick();
  middle_button.tick();
  down_button.tick();
  readEncoder();
  timers();
  //Serial.println(hamShield.readRSSI());//测试对讲机信号
}

void timers(){
  unsigned long currentMillis = millis(); // 获取当前时间

  if (currentMillis - previousMillis >= interval) { // 如果当前时间与上一次执行时间的差值大于等于间隔时间
    digitalWrite(LCD_BLK, LOW); // 关闭LCD
    previousMillis = currentMillis; // 更新上一次执行的时间
  }

  if (currentMillis - adj_previousMillis >= adj_interval) { // 如果当前时间与上一次执行时间的差值大于等于间隔时间
    adj_volume=false;//退出音量操作
    seeking=false;//退出自动搜台
    seeking_executed=false;//重置搜台执行状态
    drawSprite();
    adj_previousMillis = currentMillis; // 更新上一次执行的时间
  }

  if (currentMillis - rssi_previousMillis >= rssi_interval) { // 如果当前时间与上一次执行时间的差值大于等于间隔时间
    if(state_1846_5087){
      if(strength!=getStrength()){//如果信号强度有变化才绘制屏幕
        drawSprite();
      }
    }else{
      if(strength!=getStrength_1846()){//如果信号强度有变化才绘制屏幕
        drawSprite();
      }
    }

    rssi_previousMillis = currentMillis; // 更新上一次执行的时间
  }

}
void setup_5807(){
  pwr_5807();//开机
  delay(1000); // 保持停止状态2秒
  hamShield =HamShield(SENB, 0, 0, 3);
  Wire.begin(S_SDA,S_SCL);//ham库非标准i2c，重新启动标准i2c
  radio5807.setup();  
  read_5807=prefs.getInt("off_5807");
  Serial.println(read_5807);
  radio5807.setFrequency(read_5807);
  Serial.println(radio5807.getFrequency());
  delay(200);
  radio5807.setVolume(volume);  //最大值为15，大于15是为15，建议是7
  state_1846_5087=true;
  modetx=false;
}

void setup_1846(){
  pwr_1846();//开机
  delay(1000);
  factor_1846=prefs.getInt("factor_1846",FACTOR);

  // I2C 错误处理和重试逻辑
  int max_retries = 3;
  bool init_success = false;

  for(int retry = 0; retry < max_retries && !init_success; retry++) {
    hamShield= HamShield(SENB, S_SCL, S_SDA, 3);
    band_1846="UHF";
    Serial.print("Radio connection attempt ");
    Serial.print(retry + 1);
    Serial.println("/3:");

    int result = hamShield.testConnection();
    Serial.println(result);

    if(result > 0) {
      init_success = true;
      hamShield.initialize();
      hamShield.setSQOff();
      hamShield.dangerMode();
      Serial.println("A1846S initialized successfully");
    } else if(retry < max_retries - 1) {
      Serial.println("A1846S connection failed, retrying...");
      delay(500);
      // 重新初始化 I2C 总线
      Wire.end();
      delay(100);
      Wire.begin(S_SDA, S_SCL);
    }
  }

  if(!init_success) {
    Serial.println("ERROR: A1846S initialization failed after all retries");
    // 继续运行，但功能可能受限
  }

  subtractFromArray(station_1846_tx, 20,factor_1846);//处理偏频
  read_1846=prefs.getInt("off_1846");
  if(read_1846==0)//没有存储的话，给一个默认值
  {
    read_1846=400000;
  }
  Serial.println(read_1846);
  hamShield.frequency(read_1846);
  volume=10;
  hamShield.setVolume1(volume);
  hamShield.setVolume2(volume);
  volume = prefs.getInt("volume", 10);   // 读完再恢复变量
  hamShield.setModeReceive();

  hamShield.setSQLoThresh(-90);
  hamShield.setSQHiThresh(-102);
  hamShield.setSQOn();

  state_1846_5087=false;
  modetx=true;
}


void begin_i2c(){
  pinMode(S_SDA, OUTPUT);
  pinMode(S_SCL, OUTPUT);
}
void stop_i2c(){
  pinMode(S_SDA, INPUT_PULLUP);
  pinMode(S_SCL, INPUT_PULLUP);
}
void pwr_1846(){
  pinMode(EN_5807, OUTPUT);
  digitalWrite(27, LOW);//5807电源关机
  pinMode(EN_1846, OUTPUT);
  digitalWrite(26, HIGH);//1846电源开机
}
void pwr_5807(){
  pinMode(EN_5807, OUTPUT);
  digitalWrite(27, HIGH);//5807电源开机
  pinMode(EN_1846, OUTPUT);
  digitalWrite(26, LOW);//1846电源关机
}
//清除所有按钮随机状态并定义单、双、长按
void button_reset(){
  push_button.reset();//清除一下按钮状态机的状态
  push_button.attachClick(push_click);
  push_button.attachDoubleClick(push_doubleclick);
  push_button.attachLongPressStart(push_longPressStart);
  down_button.reset();//清除一下按钮状态机的状态
  down_button.attachClick(down_click);//单击
  down_button.attachPress(down_press);//按下即操作，与单击不同，单击是按下松开后操作。
  down_button.attachDoubleClick(down_doubleclick);
  down_button.attachLongPressStart(down_longPressStart);//长按
  down_button.attachLongPressStop(down_longPressStop);//长按松开
  
  middle_button.reset();//清除一下按钮状态机的状态
  middle_button.attachClick(middle_click);
  middle_button.attachDoubleClick(middle_doubleclick);
  middle_button.attachLongPressStart(middle_longPressStart);//长按
  up_button.reset();//清除一下按钮状态机的状态
  up_button.attachClick(up_click);
  up_button.attachDoubleClick(up_doubleclick);
  up_button.attachLongPressStart(up_longPressStart);//长按
  up_button.attachMultiClick(up_fiveClicks);
}

//旋转编码器
void readEncoder() {
  
  static int pos = 0;
  encoder.tick();
  int newPos = encoder.getPosition();
  uint16_t volume1=0;
  volume1=hamShield.getVolume1();
  if (pos != newPos) {
    
    if(newPos>pos){
      if(adj_volume){
        previousMillis=millis();
        if(state_1846_5087){
          radio5807.setVolumeDown();
          //Serial.println(radio5807.getVolume());//调整收音机音量
          if(radio5807.getVolume()==0){
            digitalWrite(AMP_PIN, LOW);//功放电源关
          }
          
        }else{
          
          volume1--;
          if(volume1<1){
            volume1=0;
            digitalWrite(AMP_PIN, LOW);//功放电源关
          }else{
            //调整对讲机机音量
            hamShield.setVolume1(volume1);
            hamShield.setVolume2(volume1);
          }
          

        }
        
      }else if(seeking){
        Serial.println("Seeking mode active, direction: up");
        if(!seeking_executed) {
          // 第一次旋转：执行搜台
          seeking_executed = true;
          if(state_1846_5087){//自动搜台
            radio5807.seek(RDA_SEEK_WRAP,RDA_SEEK_DOWN,drawSprite);
            delay(200);
            //绘制屏幕
            drawSprite();
          }else{
//1846搜台 - 向上搜索
            uint32_t found_freq = seek_1846_up();
            if(found_freq > 0) {
              hamShield.frequency(found_freq);
              Serial.println("Found frequency (up): " + String(found_freq));
              drawSprite();
            } else {
              Serial.println("No signal found (up)");
            }
          }
        } else {
          // 第二次旋转：退出搜台模式
          seeking = false;
          seeking_executed = false;
          Serial.println("Seeking mode exited");
          drawSprite();
        }
      }else{
        if(state_1846_5087){
          radio5807.setFrequencyDown();
          Serial.println(radio5807.getFrequency());
          radio5807.setFrequency(radio5807.getFrequency());
          
          //Serial.println(radio5807.checkI2C(addressArray));
          Serial.println(radio5807.getRealFrequency());
        }else{
          if(!modetx){//非发射模式可以调整频率
            hfreq=hamShield.getFrequency();
            finally_freq_1846=(uint32_t)hfreq;
            finally_freq_1846=finally_freq_1846-step_1846;
            if(finally_freq_1846<whf_low && band_1846=="WHF"){
              finally_freq_1846=whf_heigh;
            }else if(finally_freq_1846<vhf_low && band_1846=="VHF"){
              finally_freq_1846=vhf_heigh;
            }else if(finally_freq_1846<uhf_low && band_1846=="UHF"){
              finally_freq_1846=uhf_heigh;
            }
            hamShield.frequency(finally_freq_1846);
          }
          
        }
      }

    }
  
    if(newPos<pos){
      if(adj_volume){
        if(state_1846_5087){
          radio5807.setVolumeUp();
          if(radio5807.getVolume()>0){
            digitalWrite(AMP_PIN, HIGH);//功放电源开
          }
        }else{
          volume1++;
          if(volume1>=15){
            volume1=15;
          }
          if(volume1>0)
          {
            digitalWrite(AMP_PIN, HIGH);//功放电源开
          }
          hamShield.setVolume1(volume1);
          hamShield.setVolume2(volume1);
        }
      }else if(seeking){
        if(state_1846_5087){
          if(!seeking_executed) {
            // 第一次旋转：执行搜台
            seeking_executed = true;
            radio5807.seek(RDA_SEEK_WRAP,RDA_SEEK_UP,drawSprite);
            delay(200);
            //绘制屏幕
            drawSprite();
          } else {
            // 第二次旋转：退出搜台模式
            seeking = false;
            seeking_executed = false;
            Serial.println("Seeking mode exited");
            drawSprite();
          }
        }else{
          //1846搜台 - 向下搜索
          Serial.println("Seeking mode active, direction: down");
          if(!seeking_executed) {
            // 第一次旋转：执行搜台
            seeking_executed = true;
            uint32_t found_freq = seek_1846_down();
            if(found_freq > 0) {
              hamShield.frequency(found_freq);
              Serial.println("Found frequency (down): " + String(found_freq));
              drawSprite();
            } else {
              Serial.println("No signal found (down)");
            }
          } else {
            // 第二次旋转：退出搜台模式
            seeking = false;
            seeking_executed = false;
            Serial.println("Seeking mode exited");
            drawSprite();
          }
        }
      }else{
        if(state_1846_5087){
          radio5807.setFrequencyUp();
          Serial.println(radio5807.getFrequency());
          radio5807.setFrequency(radio5807.getFrequency());

          Serial.println(radio5807.getRealFrequency());
        }else{
          if(!modetx){//非发射模式可以调整频率
            hfreq=hamShield.getFrequency();
            finally_freq_1846=(uint32_t)hfreq;
            finally_freq_1846=finally_freq_1846+step_1846;
            if(finally_freq_1846>whf_heigh && band_1846=="WHF"){
              finally_freq_1846=whf_low;
            }else if(finally_freq_1846>vhf_heigh && band_1846=="VHF"){
              finally_freq_1846=vhf_low;
            }else if(finally_freq_1846>uhf_heigh && band_1846=="UHF"){
              finally_freq_1846=uhf_low;
            }
            hamShield.frequency(finally_freq_1846);
          }
          
        }
      }
         
    }

    pos = newPos;
    // 移除阻塞 delay - OneButton 库已处理防抖
    drawSprite();
  }
}
//单击编码器
void push_click()
{
  //Serial.println("p_click");//单击编码器
  adj_volume=false;//退出调整音量模式
  seeking=false;//退出搜台模式
  seeking_executed=false;//重置搜台执行状态
  if(state_1846_5087){
    if(step_5807==10){
      step_5807=50;
    }else if(step_5807==50){
      step_5807=100;
    }else if(step_5807==100){
      step_5807=200;
    }else if(step_5807==200){
      step_5807=10;
    }
    radio5807.setStep(step_5807);
    //以下待写绘制屏幕，输出当前步进
  }else{
    if(step_1846==1){
      step_1846=10;
    }else if(step_1846==10){
      step_1846=25;
    }else if(step_1846==25){
      step_1846=50;
    }else if(step_1846==50){
      step_1846=100;
    }else if(step_1846==100){
      step_1846=200;
    }else if(step_1846==200){
      step_1846=1000;
    }else if(step_1846==1000){
      step_1846=1;
    }

  }
  drawSprite();
}
//双击编码器
void push_doubleclick()
{
  Serial.println("p_doubleclick");//双击编码器
  adj_volume=false;//退出调整音量模式

  if(state_1846_5087){
    // FM收音机模式：进入搜台
    seeking=true;
    seeking_executed = false; // 重置搜台执行状态
    Serial.println("FM mode: seeking enabled");
  }else{
    // 对讲机模式：总是进入搜台模式
    seeking=true;
    seeking_executed = false; // 重置搜台执行状态
    modetx=false; // 确保在接收模式以进行搜台
    Serial.println("HamShield mode: seeking enabled, switched to RX mode");
  }
  drawSprite();
}
//长按编码器
void push_longPressStart()
{
  adj_volume=false;//退出调整音量模式
  seeking=false;//退出搜台模式
  off_5807();
  off_1846();
  digitalWrite(AMP_PIN, LOW);//功放电源关机
  digitalWrite(LCD_BLK, LOW);//屏幕背光关
  digitalWrite(PWR_ON, LOW);//MUC电源关机
}
//单击上按钮
void up_click()
{
  Serial.println("up_click");//单击上按钮
  adj_volume=false;//退出调整音量模式
  seeking=false;//退出搜台模式
  if(state_1846_5087){
    radio5807.setFrequency(station_5807[channel]);
  }else{
    if(modetx){
      hamShield.frequency(station_1846_tx[channel]);
    }else{
      hamShield.frequency(station_1846_rx[channel]);
    }
    
  }
  
  drawSprite();
}
//双击上按钮
void up_doubleclick()
{
  Serial.println("up_doubleclick");//双击上按钮
  adj_volume=true;//进入调整音量模式
  seeking=false;//退出搜台模式
}
//5击上按钮
void up_fiveClicks()
{
  clickCount++;
    if (clickCount == targetClickCount) {
      uint32_t factor_getFrequency=hamShield.getFrequency();
      uint32_t factor=409800-factor_getFrequency;
      prefs.putInt("factor_1846",factor);
        // 重置计数器，以便下一轮检测
      clickCount = 0;
    }
  
}
//长按上按钮
void up_longPressStart()
{
  Serial.println("up_longPressStart");//长按上按钮
  adj_volume=false;//退出调整音量模式
  seeking=false;//退出搜台模式
  if(state_1846_5087){
    volume=radio5807.getVolume()+9;//从收音切换到对讲，音量+9
    if(volume>15){
      volume=15;
    }
    setup_1846();
    off_5807();
    
  }else{
    volume=hamShield.getVolume1()-9;//从对讲切换到收音，音量-9
    if(volume<1){
      volume = 1;
    }
    Serial.println("off_1846");//单击中按钮
    off_1846();
    Serial.println("off_1846");//单击中按钮
    setup_5807();
    
  }
  drawSprite();
}

//单击中按钮
void middle_click()
{
  Serial.println("middle_click");//单击中按钮
  adj_volume=false;//退出调整音量模式
  seeking=false;//退出搜台模式
  channel--;
  if(channel==-1)
  {
    channel=19;
  }

  // 移除阻塞 delay - OneButton 库已处理防抖
  drawSprite();
}
//双击中按钮
void middle_doubleclick()
{
  Serial.println("middle_doubleclick");//双击中按钮
  adj_volume=false;//退出调整音量模式
  seeking=false;//退出搜台模式

  if(state_1846_5087){
    // FM收音机模式：删除存储的频率
    uint16_t frequency=0;
    save_freq(frequency,channel);
  }else{
    // 对讲机模式：切换频段
    if(band_1846=="UHF"){
      band_1846="VHF";
      hamShield.frequency(vhf_low);
    }else if(band_1846=="VHF"){
      band_1846="WHF";
      hamShield.frequency(whf_low);
    }else if(band_1846=="WHF"){
      band_1846="UHF";
      hamShield.frequency(uhf_low);
    }
  }
  drawSprite();
}
//长按中按钮
void middle_longPressStart()
{
  //Serial.println("middle_longPressStart");//长按中按钮
  adj_volume=false;//退出调整音量模式
  seeking=false;//退出搜台模式
  uint32_t frequency;
  
  if(state_1846_5087){
    frequency=radio5807.getFrequency();
  }else{
    frequency=hamShield.getFrequency();
  }
  save_freq(frequency,channel);
  //Serial.println(frequency);//长按中按钮
  // Serial.println(channel);//长按中按钮
}
//按下下按钮 - RF发射安全检查
void down_press()
{
  Serial.println("down_press");//按下下按钮
  adj_volume=false;//退出调整音量模式
  seeking=false;//退出搜台模式
  if(state_1846_5087){

  }else{
    // RF发射安全检查
    if(modetx && isInArray(hamShield.getFrequency(),station_1846_tx,20)){
      // 频率验证：确保频率在合法范围内
      uint32_t current_freq = hamShield.getFrequency();
      if(current_freq < 134000 || current_freq > 520000) {
        Serial.println("Error: Frequency out of valid range");
        drawSprite();
        return;
      }

      // 功率限制：使用较低功率防止PA过热（原为15最大值）
      // 注意：实际部署时应添加天线检测和SWR检查
      const int safe_tx_power = 10;  // 降低到安全功率水平
      hamShield.setModeTransmit();
      hamShield.setRfPower(safe_tx_power);
      hamShield.setTxSourceMic();

      Serial.println("TX enabled at safe power level: " + String(safe_tx_power));
    }
  }

  drawSprite();
}
//单击下按钮
void down_click()
{
  Serial.println("down_click");//单击下按钮
  hamShield.setModeReceive();
  adj_volume=false;//退出调整音量模式
  seeking=false;//退出搜台模式
  channel++;
  if(channel==20)
  {
    channel=0;
  }
  // 移除阻塞 delay - OneButton 库已处理防抖
  drawSprite();
}
//双击下按钮
void down_doubleclick()
{
  Serial.println("down_doubleclick");//双击下按钮
  adj_volume=false;//退出调整音量模式
  seeking=false;//退出搜台模式
  if(state_1846_5087){
    //收音机模式
  }else{
    //对讲机模式
    if(modetx){
      modetx=false;//发射模式
    }else{
      modetx=true;//发射模式
    }
  }
  drawSprite();
}
//长按下按钮
void down_longPressStart()
{
  Serial.println("p_longPressStart");//长按下按钮
  adj_volume=false;//退出调整音量模式
  seeking=false;//退出搜台模式
  if(state_1846_5087){
    //电池校准
    int adcValue = analogRead(ADC_PIN);
    float voltage = adcValue * (adcVoltageRange / (1 << adcResolution));//电池部分
    voltage=4.2-voltage;
    int temp=voltage*100;
    prefs.putInt("conv_factor",temp);

  }else{
    //if(modetx && isInArray(hamShield.getFrequency(),station_1846_tx,20)){//进入发射模式才能发射
      
    //  hamShield.setModeTransmit();
    //  hamShield.setRfPower(15);
    //  hamShield.setTxSourceMic();
      //digitalWrite(TX_ON, HIGH);//1846rxon
      //digitalWrite(RX_ON, LOW);//1846txon
    //}
  }
  
  drawSprite();
}
//松开长按下按钮
void down_longPressStop()
{
  Serial.println("down_longPressStop");//松开长按下按钮
  adj_volume=false;//退出调整音量模式
  seeking=false;//退出搜台模式
  //modetx=true;//进入发射模式
  hamShield.setModeReceive();
  drawSprite();
}
//屏幕绘制部分
void drawSprite()
{
  digitalWrite(LCD_BLK, HIGH); 
  if(state_1846_5087){
    freq = radio5807.getFrequency() / 100.0;
    value = freq * 10;  
    strength=getStrength();
  }else{
    hfreq = hamShield.getFrequency();
    hfreq=hfreq+factor_1846;
    value = hfreq/100;
    //hfreq=hfreq/1000;
    strength=getStrength_1846();
  }
  spr.fillSprite(TFT_BLACK);
  spr.setTextColor(TFT_WHITE,TFT_BLACK);
  if(state_1846_5087)
  {
    spr.drawFloat(freq,2,160,74,6);
  }else{
    
    spr.drawFloat(hfreq,0,160,74,6);
  }
  
  spr.setFreeFont(&Orbitron_Light_24);
  if(state_1846_5087){
    spr.drawString("FM Radio",160,22);
  }else{
    if(modetx){
      spr.drawString("W-Talkie",160,22);
    }else{
      spr.drawString(band_1846,160,22);
    }
    
  }
  spr.drawString("STATIONS",38,14,2);
  spr.drawRoundRect(1,1,76,110,4,0xAD55);
  spr.drawRoundRect(240,20,76,22,4,TFT_WHITE);
  spr.setTextFont(0);
  spr.setTextColor(0xBEDF,TFT_BLACK);
  batteryLevel=battery.getBatteryChargeLevel();
  spr.drawString(String(batteryLevel)+"%",292,8,1);
  if(state_1846_5087){
    spr.drawString("step:"+String(step_5807)+"KHz",242,8,1);
  }else{
    spr.drawString("step:"+String(step_1846)+"KHz",242,8,1);
  }
  
  //存6个台部分 - 修复数组越界访问
  spr.setTextFont(0);
  spr.setTextColor(0xBEDF,TFT_BLACK);
  int dwchannel=channel;
  if(dwchannel>14) dwchannel=14;
  if(dwchannel<0) dwchannel=0;
  Serial.println("dw"+String(dwchannel));

  // 确保循环不会越界访问数组 (数组大小为20，索引0-19)
  int max_i = min(dwchannel + 5, 19);  // 最多显示6个频道，确保不超过数组边界
  for(int i=dwchannel; i<=max_i; i++){
    float drawchannel;
    String temp;
    if(state_1846_5087){
      drawchannel=station_5807[i]/100.0;
      temp=String(drawchannel,2);
    }else{
      drawchannel=(station_1846_rx[i]+factor_1846);
      if(drawchannel==factor_1846){
        drawchannel=station_1846_rx[i];
      }
      temp=String(drawchannel,0);
    }
    if(modetx){
      drawchannel=(station_1846_tx[i]+factor_1846);
      temp=String(drawchannel,0);
    }
    spr.drawString(temp,38,32+((i-dwchannel)*12),1);
    if(i==channel){
      spr.fillCircle(12,31+((i-dwchannel)*12),2,TFT_WHITE);
    }
    else{
      spr.fillCircle(12,31+((i-dwchannel)*12),2,0xFBAE);
    }


  }
  
  spr.setTextColor(TFT_WHITE,TFT_BLACK);

  spr.drawString("SIGNAL:",266,54);
  spr.drawString("VOL:",257,102,2); 
  if(state_1846_5087){
    spr.drawNumber(radio5807.getVolume(),282,102,2);
  }else{
    if(digitalRead(AMP_PIN)==LOW){
      spr.drawNumber(0,282,102,2);
    }else{
      spr.drawNumber(hamShield.getVolume1(),282,102,2);
    }
    
  }
  spr.fillRoundRect(288,96,20,20,3,0xCC40);

  if(adj_volume)
  spr.fillCircle(297,105,6,TFT_WHITE);
  //画出RSSI信号强度
  
  if (strength < 7){
    for (int i = 0; i < strength; i++)    
    spr.fillRect(244 + (i * 4), 80 - (i * 1), 2, 4 + (i * 1), TFT_RED);   //信号强度部分，不好红色
  }else{
    for (int i = 0; i < strength; i++)  
    spr.fillRect(244 + (i * 4), 80 - (i * 1), 2, 4 + (i * 1), TFT_GREEN );//信号强度部分，好绿色
  } 
  int temp=value-20;
  for(int i=0;i<40;i++)
  {
    if((temp%10)==0){
      spr.drawLine(i*8,170,i*8,140,color1);
      
      spr.drawLine((i*8)+1,170,(i*8)+1,140,color1);
      spr.drawFloat(temp/10.0,1,i*8,130,2);
    }
    else if((temp%5)==0 && (temp%10)!=0){
      spr.drawLine(i*8,170,i*8,150,color1);
      spr.drawLine((i*8)+1,170,(i*8)+1,150,color1);
    }
    else{
      spr.drawLine(i*8,170,i*8,160,color1);
    }
  
    temp=temp+1;
  }
  if(state_1846_5087){
    spr.drawString("Stereo: "+String(radio5807.isStereo()),275,31,2);
  }else{
    String rtx;
    if(hamShield.getRX()){
      rtx="RX";
    }
    if(hamShield.getTX()){
      rtx="TX";
    }
    spr.drawString("MODE: "+rtx,275,31,2);
  }

  spr.drawLine(160,114,160,170,TFT_RED);
  spr.pushSprite(0,0);
  
  previousMillis=millis();
  adj_previousMillis=millis();
}
//读取5807信号强度，并转换成符合屏幕绘制的样子。。。
int getStrength()
{

  uint8_t rssi;

  rssi = radio5807.getRssi();   //0-99map到0-17
  //Serial.println(rssi);//双击编码器
  delay(10);

  if ((rssi >= 0) and (rssi <= 1))
    return 1; // S0
  if ((rssi > 1) and (rssi <= 1))
    return 2; // S1
  if ((rssi > 2) and (rssi <= 3))
    return  3; // S2
  if ((rssi > 3) and (rssi <= 4))
    return  4; // S3
  if ((rssi > 4) and (rssi <= 10))
    return  5; // S4
  if ((rssi > 10) and (rssi <= 16))
    return 6; // S5
  if ((rssi > 16) and (rssi <= 22))
    return 7; // S6
  if ((rssi > 22) and (rssi <= 28))
    return  8; // S7
  if ((rssi > 28) and (rssi <= 34))
    return 9; // S8
  if ((rssi > 34) and (rssi <= 44))
    return 10; // S9
  if ((rssi > 44) and (rssi <= 54))
    return 11; // S9 +10
  if ((rssi > 54) and (rssi <= 64))
    return 12; // S9 +20
  if ((rssi > 64) and (rssi <= 74))
    return 13; // S9 +30
  if ((rssi > 74) and (rssi <= 84))
    return 14; // S9 +40
  if ((rssi > 84) and (rssi <= 94))
    return 15; // S9 +50
  if (rssi > 94)
    return 16; // S9 +60
  if (rssi > 95)
    return 17; //>S9 +60

  return 0;

}
//读取1846信号强度，并转换成符合屏幕绘制的样子。。。

int getStrength_1846()
{

  int rssi;

  rssi = hamShield.readRSSI();   //0-99map到0-17
  //Serial.println(rssi);//双击编码器
  delay(10);

  if ((rssi >= -137) and (rssi <= -133))
    return 1; // S0
  if ((rssi > -133) and (rssi <= -128))
    return 2; // S1
    
  if ((rssi > -128) and (rssi <= -123))
    return  3; // S2
  if ((rssi > -123) and (rssi <= -118))
    return  4; // S3
  if ((rssi > -118) and (rssi <= -113))
    return  5; // S4
     
  if ((rssi > -113) and (rssi <= -108))
    return 6; // S5
  if ((rssi > -108) and (rssi <= -103))
    return 7; // S6
  if ((rssi > -103) and (rssi <= -98))
    return  8; // S7
  if ((rssi > -98) and (rssi <= -93))
    return 9; // S8
  if ((rssi > -93) and (rssi <= -88))
    return 10; // S9
  if ((rssi > -88) and (rssi <= -83))
    return 11; // S9 +10
  if ((rssi > -83) and (rssi <= -78))
    return 12; // S9 +20
  if ((rssi > -78) and (rssi <= -73))
    return 13; // S9 +30
  if ((rssi > -73) and (rssi <= -68))
    return 14; // S9 +40
  if ((rssi > -68) and (rssi <= -63))
    return 15; // S9 +50
  if ((rssi > -63) and (rssi <= -58))
    return 16; // S9 +60
  if (rssi > -58)
    return 17; //>S9 +60

  return 0;

}
//读出存储频道5807
void read_station_5807(){
  String myString="key";
  for(int i=0; i<20; i++){
    String result = myString + i;
    station_5807[i]=prefs.getInt(result.c_str());
  }
}
//读出存储频道1846
void read_station_1846_rx(){
  String myString="rx";
  for(int i=0; i<20; i++){
    String result = myString + i;
    station_1846_rx[i]=prefs.getInt(result.c_str());
  }
}
//关机或切换都保存一下退出时的频率
void off_5807(){
  uint32_t s_freq= radio5807.getFrequency();
  prefs.putInt("off_5807",s_freq);
}
void off_1846(){
  float s_freq= hamShield.getFrequency();
  uint32_t temp=static_cast<uint32_t>(round(s_freq));
  prefs.putInt("off_1846", temp);
}

//存储频率
void save_freq(uint32_t frequency,int sta){
  
  if(state_1846_5087){//5807
    String myString="key";
    String result = myString + sta;
    prefs.putInt(result.c_str(),frequency);
    station_5807[sta]=prefs.getInt(result.c_str());
    Serial.println(frequency);//长按中按钮
    Serial.println(result.c_str());//长按中按钮
    Serial.println(station_5807[sta]);//长按中按钮
  }else{//1846
    String myString="rx";
    String result = myString + sta;
    prefs.putInt(result.c_str(),frequency);
    station_1846_rx[sta]=prefs.getInt(result.c_str());
    Serial.println(frequency);//长按中按钮
    Serial.println(sta);//长按中按钮
  }
  drawSprite();
}

// 判断一个整数是否在数组中
bool isInArray(int target, int arr[], int size) {
    for (int i = 0; i < size; i++) {
        if (arr[i] == target) {
            return true;
        }
    }
    return false;
}

//处理偏频
void subtractFromArray(int arr[], int size,int factor_1846) {
    for (int i = 0; i < size; i++) {
        arr[i] -= factor_1846;
    }
}

// 对讲机向下搜台函数
uint32_t seek_1846_down() {
  uint32_t current_freq = hamShield.getFrequency();
  uint32_t start_freq, stop_freq;

  // 根据当前频段设置搜索范围（向下搜索2MHz范围）
  if(band_1846 == "UHF") {
    start_freq = (current_freq > uhf_low + 2000) ? current_freq : uhf_low + 2000;
    stop_freq = (current_freq > uhf_low + 2000) ? current_freq - 2000 : uhf_low;
  } else if(band_1846 == "VHF") {
    start_freq = (current_freq > vhf_low + 2000) ? current_freq : vhf_low + 2000;
    stop_freq = (current_freq > vhf_low + 2000) ? current_freq - 2000 : vhf_low;
  } else { // WHF
    start_freq = (current_freq > whf_low + 2000) ? current_freq : whf_low + 2000;
    stop_freq = (current_freq > whf_low + 2000) ? current_freq - 2000 : whf_low;
  }

  Serial.println("Seek down: current=" + String(current_freq) + ", start=" + String(start_freq) + ", stop=" + String(stop_freq) + ", step=" + String(step_1846));

  // 从当前频率向下搜索
  for(uint32_t freq = start_freq; freq >= stop_freq; freq -= step_1846) {
    hamShield.frequency(freq);
    delay(50); // 等待频率稳定
    int rssi = hamShield.readRSSI();
    Serial.println("Freq: " + String(freq) + ", RSSI: " + String(rssi));
    if(rssi > -130) { // 降低信号强度阈值，适应实际环境
      Serial.println("Signal found at: " + String(freq));
      return freq;
    }
  }
  Serial.println("No signal found in range");
  return 0; // 未找到信号
}

// 对讲机向上搜台函数
uint32_t seek_1846_up() {
  uint32_t current_freq = hamShield.getFrequency();
  uint32_t start_freq, stop_freq;

  // 根据当前频段设置搜索范围（向上搜索2MHz范围）
  if(band_1846 == "UHF") {
    start_freq = (current_freq < uhf_heigh - 2000) ? current_freq : uhf_heigh - 2000;
    stop_freq = (current_freq < uhf_heigh - 2000) ? current_freq + 2000 : uhf_heigh;
  } else if(band_1846 == "VHF") {
    start_freq = (current_freq < vhf_heigh - 2000) ? current_freq : vhf_heigh - 2000;
    stop_freq = (current_freq < vhf_heigh - 2000) ? current_freq + 2000 : vhf_heigh;
  } else { // WHF
    start_freq = (current_freq < whf_heigh - 2000) ? current_freq : whf_heigh - 2000;
    stop_freq = (current_freq < whf_heigh - 2000) ? current_freq + 2000 : whf_heigh;
  }

  Serial.println("Seek up: current=" + String(current_freq) + ", start=" + String(start_freq) + ", stop=" + String(stop_freq) + ", step=" + String(step_1846));

  // 从当前频率向上搜索
  for(uint32_t freq = start_freq; freq <= stop_freq; freq += step_1846) {
    hamShield.frequency(freq);
    delay(50); // 等待频率稳定
    int rssi = hamShield.readRSSI();
    Serial.println("Freq: " + String(freq) + ", RSSI: " + String(rssi));
    if(rssi > -130) { // 降低信号强度阈值，适应实际环境
      Serial.println("Signal found at: " + String(freq));
      return freq;
    }
  }
  Serial.println("No signal found in range");
  return 0; // 未找到信号
}
 
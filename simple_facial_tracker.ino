#include <SDHCI.h>
#include <stdio.h>

#include <Camera.h>

#define BAUDRATE (115200)
#define TOTAL_PICTURE_COUNT (10)

typedef enum Mode
{
  // アイドル状態
  Idle,
  // 訓練用画像撮影
  Training,
  // SDカードアクセス
  StorageAccess,
  // 表情認識実行
  Run,
} Mode;

SDClass SD;
int taken_picture_count = 0;
volatile bool d02_pushed = false;
bool is_usb_msc_enabled = false;

// 動作モード
Mode mode = Mode::Idle;

// void CamCB(CamImage img)
// {
// }

void D02PushedCallback()
{
  d02_pushed = true;
}

void setup()
{
  pinMode(LED0, OUTPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(PIN_D02, INPUT_PULLUP);
  pinMode(PIN_D03, INPUT_PULLUP);
  pinMode(PIN_D04, INPUT_PULLUP);
  pinMode(PIN_D05, INPUT_PULLUP);
  pinMode(PIN_D06, INPUT_PULLUP);

  Serial.begin(BAUDRATE);
  // 通信可能になるまで待機
  while (!Serial)
  {
    ;
  }

  Serial2.begin(BAUDRATE);

  // SDカードマウント待機
  while (!SD.begin())
  {
    digitalWrite(LED0, digitalRead(LED0) ^ 0x0001);
    delay(200);
  }
  digitalWrite(LED0, HIGH);

  digitalWrite(LED1, HIGH);
  CamErr err;
  err = theCamera.begin();
  if (err != CAM_ERR_SUCCESS)
  {
    digitalWrite(LED1, LOW);
  }

  // プレビュー画像の取得設定
  // err = theCamera.startStreaming(true, CamCB);
  // if (err != CAM_ERR_SUCCESS)
  // {
  //   digitalWrite(LED1, LOW);
  // }

  err = theCamera.setAutoWhiteBalanceMode(CAM_WHITE_BALANCE_DAYLIGHT);
  if (err != CAM_ERR_SUCCESS)
  {
    digitalWrite(LED1, LOW);
  }

  err = theCamera.setStillPictureImageFormat(
      CAM_IMGSIZE_QUADVGA_H,
      CAM_IMGSIZE_QUADVGA_V,
      CAM_IMAGE_PIX_FMT_JPG);
  if (err != CAM_ERR_SUCCESS)
  {
    digitalWrite(LED1, LOW);
  }

  // 写真撮影プッシュボタン押下時の割り込みを設定
  attachInterrupt(digitalPinToInterrupt(PIN_D02), D02PushedCallback, FALLING);

  Serial.println("Setup Completed!");
}

void loop()
{
  updateMode();

  switch (mode)
  {
  case Mode::Training:
    loopTraining();
    break;
  case Mode::StorageAccess:
    loopStorageAccess();
    break;
  case Mode::Run:
    loopRun();
  default:
    break;
  }
}

void updateMode()
{
  Mode new_mode = ReadMode();

  // モードの変化が無ければ何もしない
  if (new_mode == mode)
  {
    return;
  }

  // 状態遷移時の初期化処理実行
  // TODO 基盤上のLEDでモードを表示
  switch (new_mode)
  {
  case Mode::Training:
    initTrainingMode();
    break;
  case Mode::StorageAccess:
    initStorageAccessMode();
    break;
  case Mode::Run:
    initRunMode();
    break;
  default:
    break;
  }

  mode = new_mode;
}

void initTrainingMode()
{
  Serial.println("-> Training Mode");
  d02_pushed = false;
  SD.endUsbMsc();
  Serial.println("USB MSC Disabled.");
}

void initStorageAccessMode()
{
  Serial.println("-> StorageAccess Mode");
  SD.beginUsbMsc();
  Serial.println("USB MSC Enabled.");
}

void initRunMode()
{
  Serial.println("-> Run Mode");
  SD.endUsbMsc();
  Serial.println("USB MSC Disabled.");
}

void loopTraining()
{
  if (d02_pushed)
  {
    Serial.println("Button was pushed!");
    TakePicture();
    d02_pushed = false;
  }

  sleep(1);
}

void loopStorageAccess()
{
  // 何もしない
  sleep(1);
}

void loopRun()
{
  // TODO 実装
  // UARTの通信テスト 乱数を生成して送信
  if (Serial2.availableForWrite() > 0)
  {
    long value = random(0, INT32_MAX);
    char data[32];
    itoa(value, data, 10);
    Serial2.write(data);
  }

  sleep(1);
}

/**
 * @fn
 * @brief 写真を撮影してSDカードに保存します
 */
void TakePicture()
{
  static int count = 0;
  CamImage img = theCamera.takePicture();
  Serial.write("Taking picture...\n");

  if (img.isAvailable())
  {
    char filename[16] = {0};
    sprintf(filename, "PICT%03d.jpg", count++);

    SD.remove(filename);
    File picFie = SD.open(filename, FILE_WRITE);
    picFie.write(img.getImgBuff(), img.getImgBuffSize());
    picFie.close();

    Serial.printf("Successfully took picture. %s\n", filename);
  }
  else
  {
    Serial.println("Failed to take picture.");
  }
}

Mode ReadMode()
{
  byte value;
  value |= digitalRead(PIN_D03) << 3;
  value |= digitalRead(PIN_D04) << 2;
  value |= digitalRead(PIN_D05) << 1;
  value |= digitalRead(PIN_D06);
  value ^= 0b1111;

  switch (value)
  {
  case 0b1000:
    return Mode::Training;
  case 0b0100:
    return Mode::StorageAccess;
  case 0b0010:
    return Mode::Run;
  case 0b0001:
  default:
    return mode;
  }
}
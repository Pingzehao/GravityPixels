// ---------- НАСТРОЙКИ -----------
int offsets[6] = { -3291, -401, 1301, -12, -61, 3};   // оффсеты для акселерометра
#define BRIGHTNESS 20
#define MATR_X 16
#define MATR_Y 16
#define MATR_X_M 160
#define MATR_Y_M 160
#define PIN 6
#define PIXEL_AMOUNT 2
#define TRACE_LENGHT 5
#define G_CONST 9.81

#define DIST_TO_LED_X (int)MATR_X / MATR_X_M
#define DIST_TO_LED_Y (int)MATR_Y / MATR_Y_M
// ---------- НАСТРОЙКИ -----------

// Color definitions
#define BLUE     0x0000ff
#define RED      0xff0000
#define GREEN    0x00ff00
#define CYAN     0x007777
#define MAGENTA  0x770077
#define YELLOW   0x777700
#define WHITE    0xFFFFFF

uint32_t colors[] = {
  BLUE,
  RED,
  GREEN,
  CYAN,
  MAGENTA,
  YELLOW,
  WHITE,
};

// ---------- БИБЛИОТЕКИ -----------
#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel strip = Adafruit_NeoPixel(MATR_X * MATR_Y, PIN, NEO_GRB + NEO_KHZ800);
#include "I2Cdev.h"
#include "MPU6050.h"
MPU6050 mpu;
// ---------- БИБЛИОТЕКИ -----------

// --------------------- ДЛЯ РАЗРАБОТЧИКОВ ----------------------
int x_vel[PIXEL_AMOUNT];  // МИЛЛИМЕТРЫ В СЕКУНДУ
int y_vel[PIXEL_AMOUNT];  // МИЛЛИМЕТРЫ В СЕКУНДУ
int x_dist[PIXEL_AMOUNT];   // МИЛЛИМЕТРЫ
int y_dist[PIXEL_AMOUNT];   // МИЛЛИМЕТРЫ
byte friction[PIXEL_AMOUNT];
byte bounce[PIXEL_AMOUNT];
byte color[PIXEL_AMOUNT];
int traceX[PIXEL_AMOUNT][TRACE_LENGHT];
int traceY[PIXEL_AMOUNT][TRACE_LENGHT];

float mpuPitch = 0;
float mpuRoll = 0;
float mpuYaw = 0;
int16_t ax, ay, az;
int16_t gx, gy, gz;

unsigned long integrTimer, traceTimer;
float stepTime, loopTime;
// --------------------- ДЛЯ РАЗРАБОТЧИКОВ ----------------------

void setup() {
  Serial.begin(9600);
  mpuSetup();
  strip.begin();
  strip.setBrightness(BRIGHTNESS);

  for (byte i = 0; i < PIXEL_AMOUNT; i++) {
    x_vel[i] = 0;
    y_vel[i] = 0;
    x_dist[i] = (MATR_X_M / 2);
    y_dist[i] = (MATR_Y_M / 2);
  }
  randomSeed(analogRead(0));
  for (byte i = 0; i < PIXEL_AMOUNT; i++) {
    friction[i] = (float)random(1, 80);  // получить в диапазоне 1-7
    bounce[i] = (float)random(1, 60);
    color[i] = map(i, 0, PIXEL_AMOUNT, 0, 7);
  }
}

void loop() {

  if (millis() - traceTimer > 500) {
    traceTimer = millis();
    for (byte i = 0; i < PIXEL_AMOUNT; i++) {
      for (byte j = 0; j < TRACE_LENGHT; j++) {
        traceX[j][i] = traceX[j + 1][i];
        traceY[j][i] = traceY[j + 1][i];
      }
      traceX[TRACE_LENGHT][i] = x_dist[i] * DIST_TO_LED_X;
      traceY[TRACE_LENGHT][i] = y_dist[i] * DIST_TO_LED_Y;
    }
  }

  //loopTime = millis();
  integrate();
  strip.clear();
  for (byte i = 0; i < PIXEL_AMOUNT; i++) {
    byte nowDistX = floor(x_dist[i] * DIST_TO_LED_X);
    byte nowDistY = floor(y_dist[i] * DIST_TO_LED_X);
    pixelDraw(nowDistX, nowDistY, color[i]);
    for (byte j = 0; j < TRACE_LENGHT; j++) {
      pixelDraw(traceX[j][i], traceY[j][i], WHITE);
    }
  }
  strip.show();
  //Serial.println(millis() - loopTime);
}

void pixelDraw(byte x, byte y, byte colorNum) {
  if (y % 2 == 0)                                              // если чётная строка
    strip.setPixelColor(y * MATR_X + x, colors[colorNum]);                // заливаем в прямом порядке
  else                                                         // если нечётная
    strip.setPixelColor(y * MATR_X + MATR_X - x - 1, colors[colorNum]);    // заливаем в обратном порядке
}

void integrate() {
  // получить углы с mpu
  get_angles();

  // расчёт времени шага интегрирования
  stepTime = (float)((long)micros() - integrTimer) / 1000000;

  for (byte i = 0; i < PIXEL_AMOUNT; i++) {
    float thisAccel_x = (float)G_CONST * (sin(mpuPitch)) * (1 - (float)friction[i] / 100) * 1000;   // мм/с2
    float thisAccel_y = (float)G_CONST * (sin(mpuRoll)) * (1 - (float)friction[i] / 100) * 1000;   // мм/с2

    x_vel[i] = x_vel[i] + (float)thisAccel_x * stepTime;
    y_vel[i] = y_vel[i] + (float)thisAccel_y * stepTime;

    x_dist[i] = x_dist[i] + x_vel[i] * stepTime + (float)thisAccel_x * stepTime * stepTime / 2;
    y_dist[i] = y_dist[i] + y_vel[i] * stepTime + (float)thisAccel_y * stepTime * stepTime / 2;

    if (x_dist[i] < 0) {
      x_dist[i] = 0;
      x_vel[i] = -x_vel[i] * (float)bounce[i] / 100;
    }
    if (!(x_dist[i] < MATR_X_M - 1)) {
      x_dist[i] = MATR_X_M - 1;
      x_vel[i] = -x_vel[i] * (float)bounce[i] / 100;
    }
    if (y_dist[i] < 0) {
      y_dist[i] = 0;
      y_vel[i] = -y_vel[i] * (float)bounce[i] / 100;
    }
    if (!(y_dist[i] < MATR_Y_M - 1)) {
      y_dist[i] = MATR_Y_M - 1;
      y_vel[i] = -y_vel[i] * (float)bounce[i] / 100;
    }

    integrTimer = micros();
  }
}

void get_angles() {
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  mpuPitch = (float)ax / 16384;
  mpuPitch = constrain(mpuPitch, -1, 1);
  mpuPitch = asin(mpuPitch);

  mpuRoll = (float)ay / 16384;
  mpuRoll = constrain(mpuRoll, -1, 1);
  mpuRoll = asin(mpuRoll);
}

void mpuSetup() {
  Wire.begin();
  mpu.initialize();
  Serial.println(mpu.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");

  // ставим оффсеты
  mpu.setXAccelOffset(offsets[0]);
  mpu.setYAccelOffset(offsets[1]);
  mpu.setZAccelOffset(offsets[2]);
  mpu.setXGyroOffset(offsets[3]);
  mpu.setYGyroOffset(offsets[4]);
  mpu.setZGyroOffset(offsets[5]);
  /*
    // Acceleration range: ± 2   ± 4  ± 8  ± 16 g
    // 1G value:           16384 8192 4096 2048
    // MAX G value:        32768
    mpu.setFullScaleAccelRange(MPU6050_ACCEL_FS_2);

    // Gyroscope range: 250   500  1000 2000 °/s
    // MAX value:       32768
    mpu.setFullScaleGyroRange(MPU6050_GYRO_FS_250);
  */
}

#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>

// Định nghĩa chân cho TB6612FNG
#define MOTOR_A_IN1 4
#define MOTOR_A_IN2 5
#define MOTOR_B_IN1 18
#define MOTOR_B_IN2 19
#define MOTOR_C_IN1 12
#define MOTOR_C_IN2 13
#define MOTOR_D_IN1 14
#define MOTOR_D_IN2 15
#define STBY 21

// Định nghĩa chân cho TCRT5000L
#define SENSOR_1 25
#define SENSOR_2 26
#define SENSOR_3 27
#define SENSOR_4 32
#define SENSOR_5 33

// Định nghĩa chân cho RC522
#define RST_PIN 10
#define SS_PIN 9

MFRC522 rfid(SS_PIN, RST_PIN);

// Cấu hình UART
#define RXD2 17
#define TXD2 16
#define BAUD_RATE 115200

// Biến lưu trữ trạng thái
String command = "";
int target_x = -1, target_y = -1;
int current_x = 0, current_y = 0;

// Hàm điều khiển động cơ
void move_forward() {
  digitalWrite(STBY, HIGH);
  digitalWrite(MOTOR_A_IN1, HIGH); digitalWrite(MOTOR_A_IN2, LOW);
  digitalWrite(MOTOR_B_IN1, HIGH); digitalWrite(MOTOR_B_IN2, LOW);
  digitalWrite(MOTOR_C_IN1, HIGH); digitalWrite(MOTOR_C_IN2, LOW);
  digitalWrite(MOTOR_D_IN1, HIGH); digitalWrite(MOTOR_D_IN2, LOW);
}

void move_backward() {
  digitalWrite(STBY, HIGH);
  digitalWrite(MOTOR_A_IN1, LOW); digitalWrite(MOTOR_A_IN2, HIGH);
  digitalWrite(MOTOR_B_IN1, LOW); digitalWrite(MOTOR_B_IN2, HIGH);
  digitalWrite(MOTOR_C_IN1, LOW); digitalWrite(MOTOR_C_IN2, HIGH);
  digitalWrite(MOTOR_D_IN1, LOW); digitalWrite(MOTOR_D_IN2, HIGH);
}

void turn_left() {
  digitalWrite(STBY, HIGH);
  digitalWrite(MOTOR_A_IN1, LOW); digitalWrite(MOTOR_A_IN2, HIGH);
  digitalWrite(MOTOR_B_IN1, LOW); digitalWrite(MOTOR_B_IN2, HIGH);
  digitalWrite(MOTOR_C_IN1, HIGH); digitalWrite(MOTOR_C_IN2, LOW);
  digitalWrite(MOTOR_D_IN1, HIGH); digitalWrite(MOTOR_D_IN2, LOW);
}

void turn_right() {
  digitalWrite(STBY, HIGH);
  digitalWrite(MOTOR_A_IN1, HIGH); digitalWrite(MOTOR_A_IN2, LOW);
  digitalWrite(MOTOR_B_IN1, HIGH); digitalWrite(MOTOR_B_IN2, LOW);
  digitalWrite(MOTOR_C_IN1, LOW); digitalWrite(MOTOR_C_IN2, HIGH);
  digitalWrite(MOTOR_D_IN1, LOW); digitalWrite(MOTOR_D_IN2, HIGH);
}

void stop_motors() {
  digitalWrite(STBY, LOW);
}

// Đọc cảm biến đường
bool read_line_sensors() {
  int s1 = digitalRead(SENSOR_1);
  int s2 = digitalRead(SENSOR_2);
  int s3 = digitalRead(SENSOR_3);
  int s4 = digitalRead(SENSOR_4);
  int s5 = digitalRead(SENSOR_5);

  // Logic theo đường: s3 (giữa) = 1 là trên đường
  if (s3 == 1 && s2 == 0 && s4 == 0) {
    move_forward();
    return true;
  } else if (s2 == 1 || s1 == 1) {
    turn_right();
    return false;
  } else if (s4 == 1 || s5 == 1) {
    turn_left();
    return false;
  } else {
    stop_motors();
    return false;
  }
}

// Đọc thẻ RFID
bool read_rfid(int expected_x, int expected_y) {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return false;
  }
  String tag_data = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    tag_data += String(rfid.uid.uidByte[i], HEX);
  }
  rfid.PICC_HaltA();
  // Giả sử thẻ lưu tọa độ dạng "x,y"
  return tag_data == String(expected_x) + "," + String(expected_y);
}

void setup() {
  // Khởi tạo UART
  Serial2.begin(BAUD_RATE, SERIAL_8N1, RXD2, TXD2);

  // Khởi tạo chân động cơ
  pinMode(MOTOR_A_IN1, OUTPUT);
  pinMode(MOTOR_A_IN2, OUTPUT);
  pinMode(MOTOR_B_IN1, OUTPUT);
  pinMode(MOTOR_B_IN2, OUTPUT);
  pinMode(MOTOR_C_IN1, OUTPUT);
  pinMode(MOTOR_C_IN2, OUTPUT);
  pinMode(MOTOR_D_IN1, OUTPUT);
  pinMode(MOTOR_D_IN2, OUTPUT);
  pinMode(STBY, OUTPUT);

  // Khởi tạo chân cảm biến
  pinMode(SENSOR_1, INPUT);
  pinMode(SENSOR_2, INPUT);
  pinMode(SENSOR_3, INPUT);
  pinMode(SENSOR_4, INPUT);
  pinMode(SENSOR_5, INPUT);

  // Khởi tạo RFID
  SPI.begin();
  rfid.PCD_Init();
}

void loop() {
  // Nhận lệnh từ UART
  if (Serial2.available()) {
    command = Serial2.readStringUntil('\n');
    command.trim();

    // Phân tích lệnh: "agv1:move_to:x,y"
    if (command.startsWith("agv1:move_to:")) {
      String coords = command.substring(13); // Bỏ "agv1:move_to:"
      int comma = coords.indexOf(',');
      target_x = coords.substring(0, comma).toInt();
      target_y = coords.substring(comma + 1).toInt();
    }
  }

  // Di chuyển đến mục tiêu
  if (target_x != -1 && target_y != -1) {
    // Theo đường bằng cảm biến TCRT5000L
    if (read_line_sensors()) {
      // Kiểm tra vị trí bằng RFID
      if (read_rfid(target_x, target_y)) {
        stop_motors();
        current_x = target_x;
        current_y = target_y;
        target_x = -1; // Đã đến mục tiêu
        target_y = -1;
        Serial2.println("Reached:" + String(current_x) + "," + String(current_y));
      }
    }
  }
}
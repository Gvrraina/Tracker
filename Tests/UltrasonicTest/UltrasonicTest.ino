// 11_ultrasonic_test — read both HY-SRF05 sensors
#define TRIG_L 26
#define ECHO_L 27
#define TRIG_R 14
#define ECHO_R 33

long readCm(int trig, int echo) {
  digitalWrite(trig, LOW);  delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long us = pulseIn(echo, HIGH, 25000);   // give up after ~4 m
  return us == 0 ? -1 : us / 58;          // -1 = nothing detected
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_L, OUTPUT); pinMode(ECHO_L, INPUT);
  pinMode(TRIG_R, OUTPUT); pinMode(ECHO_R, INPUT);
}

void loop() {
  long l = readCm(TRIG_L, ECHO_L);
  delay(30);                              // stop the two sensors hearing each other
  long r = readCm(TRIG_R, ECHO_R);
  delay(30);
  Serial.print("LEFT: ");  Serial.print(l);
  Serial.print(" cm    RIGHT: "); Serial.print(r); Serial.println(" cm");
}
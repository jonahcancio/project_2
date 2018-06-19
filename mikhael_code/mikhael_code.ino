
//PIN DEFINITIONS
#define startSwitch 4
#define clockSwitch 5
#define readyLED A0
#define dataHazardLED A1
#define controlHazardLED A2
#define invalidExceptionLED A3
#define arithmeticExceptionLED A4

//GLOBAL VARIABLES
bool hasStarted;
long timeLastClockPress;
int lastClockRead;
long timeLastStartPress;
int lastStartRead;
int f;

void setup() {

  //PIN MODES
  int i;
  pinMode(startSwitch, INPUT);
  pinMode(clockSwitch, INPUT);
  for(i=6;i<14;i++){
    pinMode(i, OUTPUT);
  }
  pinMode(readyLED, OUTPUT);
  pinMode(dataHazardLED, OUTPUT);
  pinMode(controlHazardLED, OUTPUT);
  pinMode(invalidExceptionLED, OUTPUT);
  pinMode(arithmeticExceptionLED, OUTPUT);

  //SERIAL BEGIN
  Serial.begin(9600);

  char* p = "00fff";
  f = (int)strtol(p, 0, 16);
}

void loop() {
  if(!hasStarted){
    if(digitalRead(startSwitch) == HIGH){
      debounceStartPoll();
    }
  }else{
    digitalWrite(readyLED, HIGH);
    Serial.println(f);
  }
}

void outputBCD(int num, int pin0){
  int i;
  for(i = 0; i < 4; i++){
    digitalWrite(pin0+i, num%2);
    num /= 2;
  }
}

void debounceClockPoll(){
  int threshold = 30;
  int clockRead = digitalRead(clockSwitch);
  if(clockRead == HIGH && lastClockRead == LOW){
    timeLastClockPress = millis();
  }
  if(millis()-timeLastClockPress > threshold && clockRead == HIGH){
    //do action
  }  
  lastClockRead = clockRead;
}

void debounceStartPoll(){
  int threshold = 30;
  int startRead = digitalRead(clockSwitch);
  if(startRead == HIGH && lastStartRead == LOW){
    timeLastStartPress = millis();
  }
  if(millis()-timeLastStartPress > threshold && startRead == HIGH){
    hasStarted = true;
    Serial.print("START\0");
  }  
  lastStartRead = startRead;
}

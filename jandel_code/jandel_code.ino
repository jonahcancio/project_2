#define CLOCK_PIN A0

void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}

void outputBCD(int num, int pin0){
  int i;
  for(i = 0; i < 4; i++){
    digitalWrite(pin0+i, num%2);
    num /= 2;
  }
}

long timeLastClockPress;
int lastClockRead;
void debounceClockPoll(){
  int threshold = 30;
  int clockRead = digitalRead(CLOCK_PIN);
  if(clockRead == HIGH && lastClockRead == LOW){
    timeLastClockPress = millis();
  }
  if(millis()-timeLastClockPress > threshold && clockRead == HIGH){
    //do action
  }  
  lastClockRead = clockRead;
}

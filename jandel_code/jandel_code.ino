#define CLOCK_PIN A0


bool isPendingInput;
char input[20];
char output[20];

void setup() {
  Serial.begin(9600);
  Serial.println("STARTO");
//  memset(input, 0, 20);
//  memset(output, 0, 20);
  isPendingInput = true;
  Serial.setTimeout(200);
}

void loop() {
  if(isPendingInput){
    if(Serial.available()){
      Serial.readStringUntil(0).toCharArray(input, 20);
      //parse input here
      isPendingInput = false;
    }
  }else{
    long decimal = strtol(input, NULL, 16);
    Serial.println(decimal);
    sprintf(output, "0x%08x", decimal);
    Serial.println(output);
    //reverse-parse output here
    isPendingInput = true;
  }
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

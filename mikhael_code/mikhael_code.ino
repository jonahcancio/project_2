//SIZE DEFINITIONS
#define MAX_STRLEN 50
#define MAX_SUBLEN 9
#define TERMINATOR '\n'
//PIN DEFINITIONS
#define startSwitch 4
#define clockSwitch 5
#define readyLED A0
#define dataHazardLED A1
#define controlHazardLED A2
#define invalidExceptionLED A3
#define arithmeticExceptionLED A4

//Loop Boolean Hooks
bool hasStartRequested;
bool hasStartCommenced;
bool cycle;
bool start;
bool pendingInput;

//Debouncing Hooks
long timeLastClockPress;
int lastClockRead;
long timeLastStartPress;
int lastStartRead;

//string variables from server
char stringFromServer[MAX_STRLEN];
char stringToServer[MAX_STRLEN];

//separate strings from server
char pFromServer[MAX_SUBLEN];
char iFromServer[MAX_SUBLEN];
char aFromServer[MAX_SUBLEN];
char bFromServer[MAX_SUBLEN];
char cFromServer[MAX_SUBLEN];

//num variables to server
int pNewPcInput;
int aReadReg1Input;
int bReadReg2Input;
int cWriteRegInput;
int dWriteDataReg;
int eRegWrite;
int fAddressMem;
int gWriteDataMem;
int hMemRead;
int iMemWrite; 

//variables from server
int iNewInstruction;
int aReadDataReg1Output;
int bReadDataReg2Output;
int cReadDataMemOutput;

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
  Serial.setTimeout(200);
  
  //BOOLEANS
  hasStartRequested = false;
  hasStartCommenced = false;
  cycle = false;
  pendingInput = true;

  //OTHER SETUP
  memset(stringFromServer, 0, MAX_STRLEN);
  memset(stringToServer, 0 , MAX_STRLEN);
}

void loop() {
  int i;
  if(!hasStartRequested){
    debounceStartPoll();
  }else if(!hasStartCommenced){
    if(Serial.available() > 0){
      Serial.readStringUntil(0).toCharArray(stringFromServer, MAX_STRLEN);
      hasStartCommenced = true;
      strcpy(pFromServer, stringFromServer);
      initToServerVars();
      initFromServerVars();
      printToServerVars();
    }
  }else{
//    debounceClockPoll();
  }
}

void initToServerVars(){
  pNewPcInput = (int)strtol(pFromServer, NULL, 16) + 4;
  aReadReg1Input = 0;
  bReadReg2Input = 0;
  cWriteRegInput = 0;
  dWriteDataReg = 0;
  eRegWrite = 0;
  fAddressMem = 0;
  gWriteDataMem = 0;
  hMemRead = 0;
  iMemWrite = 0; 
}

void initFromServerVars(){
 iNewInstruction = 0;
 aReadDataReg1Output = 0;
 bReadDataReg2Output = 0;
 cReadDataMemOutput = 0;
}

void printToServerVars(){
    sprintf(stringToServer, "%08x ", pNewPcInput);
    Serial.print(stringToServer);
    print5BitBinary(aReadReg1Input);
    print5BitBinary(bReadReg2Input);
    print5BitBinary(cWriteRegInput);
    sprintf(stringToServer, "%08x %1d %08x %08x %1d %1d",
      dWriteDataReg, eRegWrite, fAddressMem, gWriteDataMem, hMemRead, iMemWrite);
    Serial.print(stringToServer);
    Serial.write(TERMINATOR);
}

void print5BitBinary(int num){
  int mult = 1;
  int bcd = 0;
  int i;
  for(i = 0; i < 5; i++){
    bcd += (num%2)*mult;
    num /= 2;
    mult *= 10;
  }
  char output[7];
  sprintf(output, "%05d ", bcd);
  Serial.print(output);
}

void updateToServerVars(){
  
}

void outputBCD(int num, int pin0){
  int i;
  for(i = 0; i < 4; i++){
    digitalWrite(pin0+i, num%2);
    num /= 2;
  }
}

//void debounceClockPoll(){
//  int threshold = 300;
//  int clockRead = digitalRead(clockSwitch);
//  if(clockRead != clockState){
//    timeLastClockPress = millis();
//  }
//  if(millis()-timeLastClockPress > threshold && clockRead != clockState){
//    clockState = !clockState;
//  }  
//  if(clockState == HIGH and lastClockState == LOW){
//  }
//  
//  lastClockState = clockState;
//}

void serverOutputDecode(char stringFromServer[MAX_STRLEN]){//Decodes instructions sent by the server
  char *token;

  token = strtok(stringFromServer, " ");
  strcpy(iFromServer,token);//IIIIIIII
  
  token = strtok(NULL, " ");
  strcpy(aFromServer, token);//AAAAAAAA

  token = strtok(NULL, " ");
  strcpy(bFromServer,token);//BBBBBBBB

  token = strtok(NULL, " ");
  strcpy(cFromServer,token);//CCCCCCCC
}
//
//void loop() {
//  int i;
//  if(!hasStartRequested){
//    debounceStartPoll();
//  }else if(!hasStartCommenced){
//    if(Serial.available() > 0){
//      Serial.readStringUntil(0).toCharArray(stringFromServer, MAX_STRLEN);
//      hasStartCommenced = true;
////      strcpy(pFromServer, stringFromServer);
////      initToServerVars();
////      printToServerVars();
//    }
//  }else{
//    debounceClockPoll();
//  }
//}
////
////void outputBCD(int num, int pin0){
////  int i;
////  for(i = 0; i < 4; i++){
////    digitalWrite(pin0+i, num%2);
////    num /= 2;
////  }
////}
////
//void debounceClockPoll(){
//  int threshold = 30;
//  int clockRead = digitalRead(clockSwitch);
//  if(clockRead == HIGH && lastClockRead == LOW){
//    Serial.print("HI");
//    timeLastClockPress = millis();
//  }
//  if(clockRead == HIGH && lastClockRead == LOW){
//    //Clock actions here
//    Serial.print("HELLO");
//    digitalWrite(readyLED, LOW);
//  }
//  lastClockRead = clockRead;
//}
//
void debounceStartPoll(){
  int threshold = 30;
  int startRead = digitalRead(startSwitch);
  if(startRead == HIGH && lastStartRead == LOW){
    timeLastStartPress = millis();
  }
  if(millis()-timeLastStartPress > threshold && startRead == HIGH){
    hasStartRequested = true;
    Serial.print("START");
    Serial.write(0);
    digitalWrite(readyLED, HIGH);
  }  
  lastStartRead = startRead;
}

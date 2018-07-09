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
long iNewInstruction;
long aReadDataReg1Output;
long bReadDataReg2Output;
long cReadDataMemOutput;

//pipeline instruction queue
int iPipeLineQueue[5];

//Others
char operation[4];

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
      initVars();
      printToServerVars();
    }
  }else{
//    debounceClockPoll();
  }
}

void initVars(){
  //init to server vars
  pNewPcInput = strtoul(stringFromServer, NULL, 16);
  aReadReg1Input = 0;
  bReadReg2Input = 0;
  cWriteRegInput = 0;
  dWriteDataReg = 0;
  eRegWrite = 0;
  fAddressMem = 0;
  gWriteDataMem = 0;
  hMemRead = 0;
  iMemWrite = 0; 

  //init from server vars
  iNewInstruction = 0;
  aReadDataReg1Output = 0;
  bReadDataReg2Output = 0;
  cReadDataMemOutput = 0;
  memset(iPipeLineQueue, 0, 5);
}

void enqueue(){
  int j;
  for(j=4; j>0; j--){
    iPipeLineQueue[j] = iPipeLineQueue[j-1];
  }
  iPipeLineQueue[0] = iNewInstruction;
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

void serverOutputSegregate(char stringFromServer[MAX_STRLEN]){//Segregate instructions sent by the server
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

void  serverOutputDecode(){
  iNewInstruction = strtol(iFromServer, NULL, 16);
  aReadDataReg1Output = strtol(aFromServer, NULL, 16);
  bReadDataReg2Output = strtol(bFromServer, NULL, 16);
  cReadDataMemOutput = strtol(cFromServer, NULL, 16);
}

long extractOpcode(){
  long opcode;
  opcode = iNewInstruction&4227858432;
  opcode = opcode>>25;
  return opcode;
}

void identifyInstructionType(long opcode){//Identify the instruction type based on the given opcode
  long func;
  func = instruction&2047;
  if(opcode == 0){//0 is R-type
    switch(func){//identify operation
      case 32:{
        strcpy(operation, "add");
        break;
      }
      case 34:{
        strcpy(operation, "sub");
        break;
      }case 36:{
        strcpy(operation, "and");
        break;
      }case 37:{
        strcpy(operation, "or");
        break;
      }case 42:{
        strcpy(operation, "slt");
        break;
      }
    }
  }else if(opcode == 2){//1 is J-type
    strcpy(operation, "j");
  }else{//2 is I-type
    switch(opcode){//identify operation
      case 8:{
        strcpy(operation, "addi");
        break;
      }
      case 35:{
        strcpy(operation, "lw");
        break;
      }case 43:{
        strcpy(operation, "sw");
        break;
      }case 4:{
        strcpy(operation, "beq");
        break;
    }
  }
}

void Execute(){//executes R-type instructions
  long s;
  long t;
  long S;
  long T;
  long D;
  long imm;
  long address;
 // long mem;
  
  s = aReadDataReg1Output;
  t = bReadDataReg2Output;
 // mem = cReadDataMemOutput
 
  S = iNewInstruction&65011712;//rs
  T = iNewInstruction&2031616;//rt
  D = iNewInstruction&63488;//rd
  imm = iNewInstruction&65535;//immediate 
  address = iNewInstruction&67108863;//address
  
  if(strcmp(operation, "add") == 0){
    aluResultEx = s + t;
    //aluResultEx = DDDDDDDD goes to d register that will be saved in CCCCC
  }else if(strcmp(operation, "sub") == 0){
    aluResultEx = s - t;
  }else if(strcmp(operation, "and") == 0){
    aluResultEx = s&t;
  }else if(strcmp(operation, "or") == 0){
    aluResultEx = s or t;
  }else if(strcmp(operation, "slt") == 0){
    if(s < t){
      aluResultEx = 1;
    }else{
      aluResultEx = 0;
    }
  }else if(strcmp(operation, "addi") == 0){
    return = s + imm;
    //aluResultEx = DDDDDDDD goes to d register that will be saved in CCCCC
  }else if(strcmp(operation, "lw") == 0){
    return s + imm;
  }else if(strcmp(operation, "sw") == 0){
    return s + imm;
  }else if(strcmp(operation, "beq") == 0){
    if(s == t){
      return 1;
    }else{
      return 0;
    }
  }else if(strcmp(operation, "j") == 0){
     return -1;
  }
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

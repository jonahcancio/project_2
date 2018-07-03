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
long pNewPcInput;
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

//pipeline instruction queue
int iPipelineQueue[5];

//processed variables
char operation[4];
int aluResultEx;
int aluResultMem;
int writeBackResult;

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
    //debounceStartPoll();
    hasStartRequested = true;
    Serial.print("START");
    Serial.write(0);
  }else if(!hasStartCommenced){
    if(Serial.available() > 0){
      Serial.readStringUntil(0).toCharArray(stringFromServer, MAX_STRLEN);
      hasStartCommenced = true;
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
  memset(iPipelineQueue, 0, 5);
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

void outputBCD(int num, int pin0){
  int i;
  for(i = 0; i < 4; i++){
    digitalWrite(pin0+i, num%2);
    num /= 2;
  }
}


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

void serverOutputDecode(){
  iNewInstruction = strtol(iFromServer, NULL, 16);
  aReadDataReg1Output = strtol(aFromServer, NULL, 16);
  bReadDataReg2Output = strtol(bFromServer, NULL, 16);
  cReadDataMemOutput = strtol(cFromServer, NULL, 16);
}

int extractOpcode(){
  long opcode;
  opcode = iNewInstruction&4227858432;
  opcode = opcode>>25;
  return opcode;
}

void enqueuePipeline(){
  int j;
  for(j = 4; j > 1; j--){
    iPipelineQueue[j] = iPipelineQueue[j-1];
  }
  iPipelineQueue[1] = iNewInstruction;
  aluResultMem = aluResultEx;
}

void RType(long instruction){}


void pipelineClockCycle(){
  enqueuePipeline();
  aReadReg1Input = (iPipelineQueue[1]>>21)&0x1f;
  bReadReg2Input = (iPipelineQueue[1]>>16)&0x1f;
  cWriteRegInput = (iPipelineQueue[1]>>11)&0x1f;
  RType(iPipelineQueue[4]);
  if(strcmp(operation, "lw")==0){
    dWriteDataReg = cReadDataMemOutput;
  }else{
    dWriteDataReg = aluResultMem;
  }
  eRegWrite = strcmp(operation, "j")!=0 
    && strcmp(operation, "beq")!=0
    && strcmp(operation, "sw")!=0;
  fAddressMem = aluResultEx;
  gWriteDataMem = iPipelineQueue[4]
  hMemRead = strcmp(operation, "lw") == 0;
  iMemWrite = strcmp(operation, "sw") == 0;
  
}

long timeLastStartPress;
bool isDebouncing;

void debounceStartPoll(){
  int threshold = 30;
  if(digitalRead(startSwitch) == HIGH && !isDebouncing){
    timeLastStartPress = millis();
    isDebouncing = true;
  }
  if(digitalRead(startSwitch) == LOW && isDebouncing){
    isDebouncing = false;
  }
  if(millis() - timeLastStartPress > threshold && isDebouncing){
    hasStartRequested = true;
    Serial.print("START");
    Serial.write(0);
    digitalWrite(readyLED, HIGH);
  }
}


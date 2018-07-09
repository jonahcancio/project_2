//SIZE DEFINITIONS
#define MAX_STRLEN 50
#define MAX_SUBLEN 9
#define TERMINATOR '\n'
#define DEBOUNCE_THRESHOLD 30
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
bool hasServerInput;
bool hasClockCycle;


//string variables for input and output to server
char stringFromServer[MAX_STRLEN];
char stringToServer[MAX_STRLEN];

//variables inputted to server
int aReadReg1Input;
int bReadReg2Input;
int cWriteRegInput;
long dWriteDataReg;
int eRegWrite;
long fAddressMem;
long gWriteDataMem;
int hMemRead;
int iMemWrite; 

//variables outputted by server
long iNewInstruction;
long aReadDataReg1Output;
long bReadDataReg2Output;
long cReadDataMemOutput;

//pipeline arrays
long pcQueue[5];
long insQueue[4];
long aluQueue[3];


//mutable strings for function returns
char iString[MAX_SUBLEN];

//hazard and exception check;
bool toBranchOrNot;
int controlHazard;
int dataHazard;
int invalidInstruction;
int arithmeticOverflow;


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
  hasServerInput = false;
  hasClockCycle = false;

  //OTHER SETUP
  memset(stringFromServer, 0, MAX_STRLEN);
  memset(stringToServer, 0 , MAX_STRLEN);

  //init to server vars
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
  memset(insQueue, 0, 4);
  memset(aluQueue, 0, 3);
  memset(pcQueue, 0, 5);
}

void loop() {
  int i;
  if(!hasStartRequested){
    //debounceStartPoll();
    Serial.println("imagine the START switch was pressed at this moment");    
    Serial.print("START");
    Serial.write(TERMINATOR);
    hasStartRequested = true;
  }else if(!hasStartCommenced){
    if(Serial.available() > 0){
      Serial.readStringUntil(TERMINATOR).toCharArray(stringFromServer, MAX_STRLEN);      
      pcQueue[0] = strtoul(stringFromServer, NULL, 16);
      
      printPipelineInput();
      hasStartCommenced = true;
    }
  }else if(!hasServerInput){
    if(Serial.available() > 0){
      //get output of ALU
      insIdentify(insQueue[1]);
      aluQueue[0] = aluExecute(iString);

      //get output of instruction mem, register file, data mem
      Serial.readStringUntil(TERMINATOR).toCharArray(stringFromServer, MAX_STRLEN); 
      storeServerOutput();

      //get next pC

      //debugging
      printPipelineQueues();
      
      hasServerInput = true;
    }
  }else if(!hasClockCycle){
    //debounceClockPoll();
    Serial.println("imagine the CLOCK switch was pressed at this moment");
    hasClockCycle = true;
  }else{
    movePipelineQueue();
    updatePipelineInput();
    printPipelineInput();

    hasServerInput = false;
    hasClockCycle = false;
  }
}


//print it pipeline queue for debugging 
void printPipelineQueues(){
  Serial.println("\n<< DEBUGGING PART - START  >>");
  char output[MAX_STRLEN];
  sprintf(output, "PC at IF: %08lx", pcQueue[0]);
  Serial.println(output);
  sprintf(output, "PC at ID: %08lx\tIns at ID: %08lx", pcQueue[1], insQueue[0]);
  Serial.println(output);
  sprintf(output, "PC at EX: %08lx\tInst at EX: %08lx\tALU at EX: %08lx", pcQueue[2], insQueue[1], aluQueue[0]);
  Serial.println(output);
  sprintf(output, "PC at MEM: %08lx\tInst at MEM: %08lx\tALU at MEM: %08lx", pcQueue[3], insQueue[2], aluQueue[1]);
  Serial.println(output);
  sprintf(output, "PC at WB: %08lx\tInst at WB: %08lx\tALU at WB: %08lx", pcQueue[4], insQueue[3], aluQueue[2]);
  Serial.println(output);
  Serial.println("<< DEBUGGING PART - END  >>\n");
}

//prints num into a binary string of length 5
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

//prints the pipeline variables as input to server
void printPipelineInput(){
    sprintf(stringToServer, "%08lx ", pcQueue[0]);
    Serial.print(stringToServer);
    print5BitBinary(aReadReg1Input);
    print5BitBinary(bReadReg2Input);
    print5BitBinary(cWriteRegInput);
    sprintf(stringToServer, "%08lx %1d %08lx %08lx %1d %1d",
      dWriteDataReg, eRegWrite, fAddressMem, gWriteDataMem, hMemRead, iMemWrite);
    Serial.print(stringToServer);
    Serial.write(TERMINATOR);
}

//parses stores the server input variables
void storeServerOutput(){
  char *token;  
  token = strtok(stringFromServer, " ");
  insQueue[0] = strtoul(token, NULL, 16);//IIIIIIII  
  token = strtok(NULL, " ");
  aReadDataReg1Output = strtoul(token, NULL, 16);//AAAAAAA
  token = strtok(NULL, " ");
  bReadDataReg2Output = strtoul(token, NULL, 16);//BBBBBBBB
  token = strtok(NULL, " ");
  cReadDataMemOutput = strtoul(token, NULL, 16);//CCCCCCCC
}


long getNextPc(){
  long nextPc = pcQueue[0]+4;
  insIdentify(insQueue[1]);
  if(strcmp(iString, "j")==0 ){
    if(aluExecute("j")==1){//determine whether to jump
      nextPc = (insQueue[1]&&0x3ffffff)<<2;
    }
  }else if(strcmp(iString, "beq")==0){  
    if(aluExecute("b")==1){//determine whether to branch
      nextPc = pcQueue[2] + (insQueue[1]&&0xffff)<<2;
    }
  }
  return nextPc;
}


//test with 01294820 000000a0 0000000f 00000005

//Call every clock cycle to move pipeline and update pipeline variables to input to server
void updatePipelineInput(){     
  pcQueue[0] = getNextPc();
  
  aReadReg1Input = (insQueue[0]>>21)&0x1f;
  bReadReg2Input = (insQueue[0]>>16)&0x1f;
  cWriteRegInput = (insQueue[0]>>11)&0x1f;

  insIdentify(insQueue[3]);
  if(strcmp(iString, "lw")==0 || strcmp(iString, "nop")==0){
    dWriteDataReg = cReadDataMemOutput;
  }else{
    dWriteDataReg = aluQueue[2];
  }
  eRegWrite = strcmp(iString, "j")!=0 && strcmp(iString, "beq")!=0 && strcmp(iString, "sw")!=0;
  fAddressMem = aluQueue[1];
  gWriteDataMem = insQueue[3]&0xffff;

  insIdentify(insQueue[2]);
  hMemRead = strcmp(iString, "lw") == 0;
  iMemWrite = strcmp(iString, "sw") == 0;  
}

//move each instruction in the pipeline one stage to the right
void movePipelineQueue(){
  int j;
  for(j = 4; j > 0; j--){
    pcQueue[j] = pcQueue[j-1];
  }
  
  for(j = 3; j > 0; j--){
    insQueue[j] = insQueue[j-1];
  }
  
  for(j = 2; j > 0; j--){
    aluQueue[j] = aluQueue[j-1];
  }  
}

void insIdentify(long iCode){//identifies the specific operation  
  long opCode;
  long func;
  opCode = (iCode>>26)&0x3f;
  func = iCode & 0x3f;
  
  if(opCode == 0){//R-type
    switch(func){//identify operation
      case 0x20:{
        strcpy(iString, "add");
        break;
      }
      case 0x22:{
        strcpy(iString, "sub");
        break;
      }case 0x24:{
        strcpy(iString, "and");
        break;
      }case 0x25:{
        strcpy(iString, "or");
        break;
      }case 0x2a:{
        strcpy(iString, "slt");
        break;
      }
      default:
        strcpy(iString, "nop");
    }
  }else if(opCode == 2){//J-type
  
  }else{
  }
}

//executes R-type instructions
long aluExecute(char *iString){
  long rs;
  long rt;
  long rd;
  
  rs = aReadDataReg1Output;
  rt = bReadDataReg2Output;
  
  if(strcmp(iString, "add") == 0){
    rd = rs + rt;
  }else if(strcmp(iString, "sub") == 0){
    rd = rs - rt;
  }else if(strcmp(iString, "and") == 0){
    rd = rs & rt;
  }else if(strcmp(iString, "or") == 0){
    rd = rs | rt;
  }else if(strcmp(iString, "slt") == 0){
    if(rs < rt){
      rd = 1;
    }else{
      rd = 0;
    }
  }
  return rd;
}


long timeLastClockToggle;
bool isClockDebouncing;
int currClockState;
int prevClockState; 

void debounceClockPoll(){
  //if check if switch has been toggled and start debounce timer if haven't debounced yet
  if(digitalRead(clockSwitch) != currClockState && !isClockDebouncing){
    timeLastClockToggle = millis();
    isClockDebouncing = true;
  }

  //restart debounce timer if fluctuation lasted too short
  if(digitalRead(clockSwitch) == currClockState){
    isClockDebouncing = false;
  }

  //check if debounce DEBOUNCE_THRESHOLD has been reached during debouncing
  if(millis() - timeLastClockToggle > DEBOUNCE_THRESHOLD && isClockDebouncing){
    currClockState = !currClockState;
    isClockDebouncing = false;
  }

  //check for rising edge
  if(currClockState == HIGH && prevClockState == LOW){
     Serial.println("Debounced rising detected");
  }

  //check for falling edge
  if(currClockState == LOW && prevClockState == HIGH){
     Serial.println("Debounced falling detected");
  }
}


void outputBCD(int num, int pin0){
  int i;
  for(i = 0; i < 4; i++){
    digitalWrite(pin0+i, num%2);
    num /= 2;
  }
}


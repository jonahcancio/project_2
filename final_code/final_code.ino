/*test with:
    00400000
    01294820 00000000 00000001 00000005
    1252ffff 00000001 00000000 00000005
    01294820 00000002 00000002 00000005
*/
//SIZE DEFINITIONS
#define MAX_STRLEN 50
#define MAX_SUBLEN 9
#define TERMINATOR '\n'
#define DEBOUNCE_THRESHOLD 100
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
long aReadReg1Input;
long bReadReg2Input;
long cWriteRegInput;
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
long insQueue[5];
//int hazardQueue[4];
long aluQueue[3];

//Conditional Branching
bool ToBranchJump;
bool toStall;
int stallIndex;

//mutable strings for function returns
char operation[MAX_SUBLEN];

//hazard and exception check;
int controlHazard;
int dataHazard;
int invalidInstruction;
int arithmeticOverflow;

//Debouncing
long timeLastStartToggle;
bool isStartDebouncing;
int currStartState;
int prevStartState;
long timeLastClockToggle;
bool isClockDebouncing;
int currClockState;
int prevClockState;


//Last Two PC digits
int pclast;
int pcsecondlast;

void setup() {
  //PIN MODES
  int i;
  pinMode(startSwitch, INPUT);
  pinMode(clockSwitch, INPUT);
  for (i = 6; i < 14; i++) {
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
  hasClockCycle = false;
  hasServerInput = false;
  ToBranchJump = false;

  //Debounce Variables
  currStartState = LOW;
  currClockState = LOW;


  //hazards and exceptions
  controlHazard = 0;
  dataHazard = 0;
  invalidInstruction = 0;
  arithmeticOverflow = 0;

  
  //OTHER SETUP
  memset(stringFromServer, 0, MAX_STRLEN);
  memset(stringToServer, 0 , MAX_STRLEN);
  stallIndex = 0;

  //TO SERVER VARS
  aReadReg1Input = 0;
  bReadReg2Input = 0;
  cWriteRegInput = 0;
  dWriteDataReg = 0;
  eRegWrite = 0;
  fAddressMem = 0;
  gWriteDataMem = 0;
  hMemRead = 0;
  iMemWrite = 0;

  //FROM SERVER VARS
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
  if (!hasStartRequested) {
    //debounceStartPoll();
    noBreadboardStart();
  } else if (!hasStartCommenced) {
    if (Serial.available() > 0) {
      Serial.readStringUntil(TERMINATOR).toCharArray(stringFromServer, MAX_STRLEN);
      pcQueue[1] = strtoul(stringFromServer, NULL, 16);
      pcQueue[0] = pcQueue[1]+4;
      printPipelineInput();
      
      digitalWrite(readyLED, HIGH);
      hasStartCommenced = true;
    }
  } else if (!hasServerInput) {
    if (Serial.available() > 0) { //get server input
      Serial.println("Server output received");
      //get output of ALU
      insIdentify(insQueue[2]);
      Execute();
      Serial.print("Finished executing ");
      Serial.print(operation);
      Serial.write(TERMINATOR);

      //get output of instruction mem, register file, data memM
      Serial.readStringUntil(TERMINATOR).toCharArray(stringFromServer, MAX_STRLEN);
      storeServerOutput();

      //check if to branch or not to branch
      idBeqResolve();
      Serial.print("Finished checking for branch. ");
      if(ToBranchJump){
        Serial.print("YES, It will branch on next clock cycle");
      }else{
        Serial.print("NO, It wont branch on next clock cycle");
      }
      Serial.write(TERMINATOR);
      
      //debugging
      debugPipelineQueues();

      hasServerInput = true;
    }
  } else if (!hasClockCycle) {
    //debounceClockPoll();
    noBreadboardClock();
  } else if(hasClockCycle){
    digitalWrite(readyLED, LOW);

    //reset hazards and exceptions
    controlHazard = 0;
    dataHazard = 0;
    invalidInstruction = 0;
    arithmeticOverflow = 0;

    movePipelineQueue();
    pcQueue[0] = getNextPc();    
    updatePipelineInput();
    printPipelineInput();

    if(stallIndex != 0){
      Serial.println("The stalling has ended");
      stallIndex = 0;
    }

    
    lastTwoPcDigits();
    outputBCD(pclast, 6);
    outputBCD(pcsecondlast, 10);
    hasServerInput = false;
    hasClockCycle = false;
  }
}

void print5BitBinary(int num) {
  int mult = 1;
  int bcd = 0;
  int i;
  for (i = 0; i < 5; i++) {
    bcd += (num % 2) * mult;
    num /= 2;
    mult *= 10;
  }
  char output[7];
  sprintf(output, "%05d ", bcd);
  Serial.print(output);
}

void lastTwoPcDigits(){//obtain last two PC digits of current instruction
  long pc;
  pc = pcQueue[0];
  pc = pc/4;
  pclast = pc%10;
  pc = pc/10;
  pcsecondlast = pc%10;
}

void outputBCD(int num, int pin0) {
  int i;
  for (i = 0; i < 4; i++) {
    digitalWrite(pin0 + i, num % 2);
    num /= 2;
  }
}

void debounceStartPoll() {
  //if check if switch has been toggled and start debounce timer if haven't debounced yet
  if (digitalRead(startSwitch) != currStartState && !isStartDebouncing) {
    timeLastStartToggle = millis();
    isStartDebouncing = true;
  }

  //restart debounce timer if fluctuation lasted too short
  if (digitalRead(startSwitch) == currStartState) {
    isStartDebouncing = false;
  }

  //check if debounce DEBOUNCE_THRESHOLD has been reached during debouncing
  if (millis() - timeLastStartToggle > DEBOUNCE_THRESHOLD && isStartDebouncing) {
    currStartState = !currStartState;
    isStartDebouncing = false;
  }

  //check for rising edge
  if (currStartState == HIGH && prevStartState == LOW) {
    Serial.print("START");
    Serial.write(TERMINATOR);
    hasStartRequested = true;
    Serial.println("Debounced rising detected");
  }

  //check for falling edge
  if (currStartState == LOW && prevStartState == HIGH) {
    Serial.println("Debounced falling detected");
  }

  prevStartState = currStartState;
}

void debounceClockPoll() {
  //if check if switch has been toggled and start debounce timer if haven't debounced yet
  if (digitalRead(clockSwitch) != currClockState && !isClockDebouncing) {
    timeLastClockToggle = millis();
    isClockDebouncing = true;
  }

  //restart debounce timer if fluctuation lasted too short
  if (digitalRead(clockSwitch) == currClockState) {
    isClockDebouncing = false;
  }

  //check if debounce DEBOUNCE_THRESHOLD has been reached during debouncing
  if (millis() - timeLastClockToggle > DEBOUNCE_THRESHOLD && isClockDebouncing) {
    currClockState = !currClockState;
    isClockDebouncing = false;
  }

  //check for rising edge
  if (currClockState == HIGH && prevClockState == LOW) {
    hasClockCycle = true;
    Serial.println("Debounced rising detected");
  }

  //check for falling edge
  if (currClockState == LOW && prevClockState == HIGH) {
    Serial.println("Debounced falling detected");
  }

  prevClockState = currClockState;
}

//move each instruction in the pipeline one stage to the right
void movePipelineQueue() {
  int j;
  for (j = 4; j > stallIndex && j > 0; j--) {
    pcQueue[j] = pcQueue[j - 1];
  }

  for (j = 4; j > stallIndex && j > 0; j--) {
    insQueue[j] = insQueue[j - 1];
  }
  if(stallIndex > 0){
    insQueue[stallIndex] = 0;
  }
  
  for (j = 2; j > stallIndex-2 && j > 0; j--) {
    aluQueue[j] = aluQueue[j - 1];
  }

  
}

long getNextPc() {
  if(stallIndex > 0){
    Serial.println("next PC is the same PC");
    return pcQueue[0];
  }
  char output[MAX_STRLEN];
  long nextPc = pcQueue[0] + 4;
  
  insIdentify(insQueue[2]);
  if (strcmp(operation, "j") == 0 ) {
    nextPc = (insQueue[2] && 0x3ffffff) << 2;
    flushIfId();
  }else if (strcmp(operation, "beq") == 0 && ToBranchJump) {    
    nextPc = pcQueue[2] + 4;
    long imm = insQueue[2] & 0xffff;
    if(imm >> 15 == 1){      
      nextPc += (imm - 0x10000) << 2;
    }else{
      nextPc += imm << 2;
    }
    
    flushIfId();
  }

  return nextPc;
}

void flushIfId(){
    insQueue[1] = 0;
    controlHazard = 1;
}


void resetHazardsExceptions(){
  controlHazard = 0;
  dataHazard = 0;
  invalidInstruction = 0;
  arithmeticOverflow = 0;
}
//Call every clock cycle to move pipeline and update pipeline variables to input to server
void updatePipelineInput() {

  aReadReg1Input = (insQueue[1] >> 21) & 0x1f;
  bReadReg2Input = (insQueue[1] >> 16) & 0x1f;
  cWriteRegInput = (insQueue[1] >> 11) & 0x1f;

  insIdentify(insQueue[4]);
  if (strcmp(operation, "lw") == 0 || strcmp(operation, "nop") == 0) {
    dWriteDataReg = cReadDataMemOutput;
  } else {
    dWriteDataReg = aluQueue[2];
  }
  eRegWrite = strcmp(operation, "j") != 0 && strcmp(operation, "beq") != 0 && strcmp(operation, "sw") != 0;
  fAddressMem = aluQueue[1];
  gWriteDataMem = insQueue[4] & 0xffff;

  insIdentify(insQueue[3]);
  hMemRead = strcmp(operation, "lw") == 0;
  iMemWrite = strcmp(operation, "sw") == 0;
}


//prints the pipeline variables as input to server
void printPipelineInput() {
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
void storeServerOutput() {
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

void insIdentify(long instruction) { //Identify the instruction type based on the given opcode
  long func;
  long opcode = (instruction >> 26) & 0x3f;
  func = instruction & 0x3f;;
  if (opcode == 0) { //0 is R-type
    switch (func) { //identify operation
      case 32: {
          strcpy(operation, "add");
          break;
        }
      case 34: {
          strcpy(operation, "sub");
          break;
      } case 36: {
          strcpy(operation, "and");
          break;
      } case 37: {
          strcpy(operation, "or");
          break;
      } case 42: {
          strcpy(operation, "slt");
          break;
      } default:
        strcpy(operation, "r-nop");
    }
  } else if (opcode == 2) { //1 is J-type
    strcpy(operation, "j");
  } else { //2 is I-type
    switch (opcode) { //identify operation
      case 8: {
          strcpy(operation, "addi");
          break;
        }
      case 35: {
          strcpy(operation, "lw");
          break;
      } case 43: {
          strcpy(operation, "sw");
          break;
      } case 4: {
          strcpy(operation, "beq");
          break;
      } default:
        strcpy(operation, "i-nop");
    }
  }
}

long getPossibleForwardedValue(char* reg, char* stage){
  long rdMem;//rd in MEM
  long rdExWb;//rd in EX if stage is ID or WB if stage is EX
  long rsRtIdEx;//rs or rt in ID if stage is ID or EX if stage is EX
  bool isMemHaveRd = false;
  bool isExWbHaveRd = false;
  bool isExHaveLw = false;

  //set i, index for instruction to 1 or 3 depending on whether forwarding done on ID or EX such that values are looked for in EX or WB
  int i = 2;
  if(strcmp(stage, "EX") == 0){
    i = 4;
  }

  //get rd from EX or WB depending on stage
  insIdentify(insQueue[i]);
  if(strcmp(operation, "add") == 0 || strcmp(operation, "sub") == 0|| strcmp(operation, "and") == 0 || strcmp(operation, "or") == 0 || strcmp(operation, "slt") == 0){
      rdExWb = (insQueue[i] >> 11) & 0x1f;
      isExWbHaveRd = true;
      
  }else if(strcmp(operation, "addi") == 0 ||  strcmp(operation, "lw") == 0){
      rdExWb = (insQueue[i] >> 16) & 0x1f;
      isExWbHaveRd = true;
  }

  //check if load word found in EX, by checking if stage trying to forward from is ID
  if(strcmp(stage, "ID") == 0 && strcmp(operation, "lw") == 0){
      isExHaveLw = true;
  }

  //get rd from MEM
  insIdentify(insQueue[3]);
  if(strcmp(operation, "add") == 0 || strcmp(operation, "sub") == 0|| strcmp(operation, "and") == 0 || strcmp(operation, "or") == 0 || strcmp(operation, "slt") == 0){
      rdMem = (insQueue[3] >> 11) & 0x1f;
      isMemHaveRd = true;
      
  }else if(strcmp(operation, "addi") == 0){
      rdMem = (insQueue[3] >> 16) & 0x1f;
      isMemHaveRd = true;
  }

  //set j shift value to 16 or 21 depending on whether we want rt or rs
  int j = 16;
  if(strcmp(reg, "rs") == 0){
    j = 21;
  }

  //get the appropriate rs or rt from ID or EX depending on i(stage) and j(shift value)
  rsRtIdEx = (insQueue[i/2] >> j) & 0x1f;

  //initiate stalling if load-word hazard found in EX
  if(isExHaveLw && rsRtIdEx == rdExWb){//if EX has lw and (rsRt of ID) = (rd of EX)
      Serial.println("Let the stalling begin!");
      stallIndex = 2;
  }

  if(isMemHaveRd && rsRtIdEx == rdMem){//data hazard found in MEM
    if(strcmp(stage, "EX") == 0){
      dataHazard = 1;
    }
    return aluQueue[1];
  }else if(isExWbHaveRd && rsRtIdEx == rdExWb){//data hazard found in EX or WB
    if(strcmp(stage, "EX") == 0){
      dataHazard = 1;
    }
    return aluQueue[i-2];
  }else{        
    if(strcmp(reg, "rs") == 0){//no hazards found
      return aReadDataReg1Output;
    }else{
      return bReadDataReg2Output;
    }
  }
  
}//end of getPossibleForwardedValue function

void Execute() { //executes instructions
  long s;
  long t;

  long imm;
  long address;

  s = aReadDataReg1Output;
  t = bReadDataReg2Output;

  imm = iNewInstruction & 65535; //immediate
  address = iNewInstruction & 67108863; //address

  s = getPossibleForwardedValue("rs", "EX");
  t = getPossibleForwardedValue("rt", "EX");  

  //ALU Execution
  insIdentify(insQueue[2]);
  if (strcmp(operation, "add") == 0) {
    aluQueue[0] = s + t;
    //aluResultEx = DDDDDDDD goes to d register that will be saved in CCCCC
  } else if (strcmp(operation, "sub") == 0) {
    aluQueue[0] = s - t;
  } else if (strcmp(operation, "and") == 0) {
    aluQueue[0] = s & t;
  } else if (strcmp(operation, "or") == 0) {
    aluQueue[0] = s | t;
  } else if (strcmp(operation, "slt") == 0) {
    if (s < t) {
      aluQueue[0] = 1;
    } else {
      aluQueue[0] = 0;
    }
  } else if (strcmp(operation, "addi") == 0) {
    aluQueue[0] = s + imm;
    //aluResultEx = DDDDDDDD goes to d register that will be saved in CCCCC
  } else if (strcmp(operation, "lw") == 0) {

  } else if (strcmp(operation, "sw") == 0) {

  } else if (strcmp(operation, "beq") == 0) {

  } else if (strcmp(operation, "j") == 0) {

  }
}

void idBeqResolve(){
  long s;
  long t;
  ToBranchJump = false;
  
  insIdentify(insQueue[1]);
  if(strcmp(operation, "j") == 0){
    ToBranchJump = true;
  }else if(strcmp(operation, "beq") == 0) {
    
    //Check for hazards
    s = getPossibleForwardedValue("rs", "ID");
    t = getPossibleForwardedValue("rt", "ID");

    char output[MAX_STRLEN];
    sprintf(output, "beq found in ID: s = %ld and t = %ld", s, t);
    Serial.println(output);
    if(s == t && stallIndex == 0) {
      ToBranchJump = true;
    }
  }  
}

void debugPipelineQueues() {
  Serial.println("\n<< DEBUGGING PART - START  >>");
  char output[MAX_STRLEN];
  sprintf(output, "PC at IF: %08lx\tIns at IF/ID: %08lx", pcQueue[0], insQueue[0]);
  Serial.println(output);
  sprintf(output, "PC at ID: %08lx\tIns at ID/EX: %08lx", pcQueue[1], insQueue[1]);
  Serial.println(output);
  sprintf(output, "PC at EX: %08lx\tInst at EX/MEM: %08lx\tALU at EX: %08lx", pcQueue[2], insQueue[2], aluQueue[0]);
  Serial.println(output);
  sprintf(output, "PC at MEM: %08lx\tInst at MEM/WB: %08lx\tALU at MEM: %08lx", pcQueue[3], insQueue[3], aluQueue[1]);
  Serial.println(output);
  sprintf(output, "PC at WB: %08lx\tInst at WB/tossed out: %08lx\tALU at WB: %08lx", pcQueue[4], insQueue[4], aluQueue[2]);
  Serial.println(output);
  Serial.println("<< DEBUGGING PART - END  >>\n");
}

void noBreadboardStart(){
  Serial.println("imagine the START switch was pressed at this moment");
  Serial.print("START");
  Serial.write(TERMINATOR);
  hasStartRequested = true;
}

void noBreadboardClock(){
  Serial.println("imagine the CLOCK switch was pressed at this moment");
  hasClockCycle = true;
}

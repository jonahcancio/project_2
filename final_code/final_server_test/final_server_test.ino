/*test with:
    00400000
    01294820 7fffffff 7fffffff 00000005
    212afffe 7fffffff 7fffffff 00000005
    1252ffff 00000001 00000000 00000005
    01294820 00000002 00000002 00000005
*/
//SIZE DEFINITIONS
#define MAX_STRLEN 70
#define MAX_SUBLEN 9
#define TERMINATOR '\0'
#define DEBOUNCE_THRESHOLD 60
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
bool hasRestartBaby;

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
long aReadDataReg1Output;
long bReadDataReg2Output;
long cReadDataMemOutput;

//pipeline arrays
long pcQueue[5];
long insQueue[5];
long aluQueue[3];

//Conditional Branching
bool ToBranchJump;
int toJumpOrBranch;//0 if none; 1 if jump; 2 if branch
int stallIndex;

//mutable strings for function returns
char operation[MAX_SUBLEN];

//forwarding
int rtExHazardCase;
int rsExHazardCase;
int rtBeqHazardCase;
int rsBeqHazardCase;
int rtLwHazardCase;
int rsLwHazardCase;

//hazard and exception check;
int hasControlHazard;
int hasDataHazard;
int hasInvalidInstruction;
int hasArithmeticOverflow;

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
  hasRestartBaby = false;
  toJumpOrBranch = false;


  //Debounce Variables
  currStartState = LOW;
  currClockState = LOW;


  //hazards and exceptions
  hasControlHazard = 0;
  hasDataHazard = 0;
  hasInvalidInstruction = 0;
  hasArithmeticOverflow = 0;

  
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
  aReadDataReg1Output = 0;
  bReadDataReg2Output = 0;
  cReadDataMemOutput = 0;
  memset(insQueue, 0, 5*sizeof(long));
  memset(aluQueue, 0, 3*sizeof(long));
  memset(pcQueue, 0, 5*sizeof(long));
}

void loop() {
  int i;
  if (!hasStartRequested || hasRestartBaby) {
    debounceStartPoll();
//    noBreadboardStart();
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
     // Serial.println("Server output received");

      //get output of instruction mem, register file, data memM
      Serial.readStringUntil(TERMINATOR).toCharArray(stringFromServer, MAX_STRLEN);
      storeServerOutput();

      //had to put beq resolving after server input since it needs aReadReg1 and bReadReg2
      toJumpOrBranch = 0;
      idBeqResolve();
      //  debugIdBeqResolve();
      
      hasServerInput = true;
    }
  } else if (!hasClockCycle) {
    debounceClockPoll();
    debounceStartPoll();
//    noBreadboardClock();
  } else if(hasClockCycle){
    digitalWrite(readyLED, LOW);
    

    movePipelineQueue();
    pcQueue[0] = getNextPc();//getNextPc will stall if stallIndex > 0    

    //reset hazards and exceptions
    hasControlHazard = 0;
    hasDataHazard = 0;

     //reset forwarding values
    rtExHazardCase = 0;
    rsExHazardCase = 0;
    rtBeqHazardCase = 0;
    rsBeqHazardCase = 0;
    rtLwHazardCase = 0;
    rsLwHazardCase = 0;

    //will be true if jump or valid beq instruction has reached EX
    if(toJumpOrBranch > 0){
        hasControlHazard = 1;
        //flush out ID/EX only
        insQueue[1] = 0;
    }  
    
    //check for invalid instruction exception
    insIdentify(insQueue[1]);
    if(strcpy(operation, "invalid") == 0){
      hasInvalidInstruction = 1;
      //flush from IF/ID to ID/EX
      insQueue[0] = 0;
      insQueue[1] = 0;
      pcQueue[0] = 0;
      pcQueue[1] = 0;
    }

    //check for data hazards
    stallIndex = 0;
    checkExHazards();
    checkBeqHazards();
    checkLwHazards();
    if(rtLwHazardCase > 0 || rsLwHazardCase > 0){
      stallIndex = 2;//activate stalling when lw hazard is found
    }
  
    aluExecute();
    if(hasArithmeticOverflow){
      //flush from IF/ID to EX/MEM
      insQueue[0] = 0;
      insQueue[1] = 0;
      insQueue[2] = 0;
      pcQueue[0] = 0;
      pcQueue[1] = 0;
      pcQueue[2] = 0;
    }
    //debugExecute();

    
    updatePipelineInput();
    printPipelineInput();

    //debugging
//    debugPipelineQueues();
//    debugHazardsExceptions();

    
    lastTwoPcDigits();
    outputBCD(pclast, 6);
    outputBCD(pcsecondlast, 10);

    digitalWrite(dataHazardLED, hasDataHazard);
    digitalWrite(controlHazardLED, hasControlHazard);
    digitalWrite(arithmeticExceptionLED, hasArithmeticOverflow);
    digitalWrite(invalidExceptionLED, hasInvalidInstruction);
  
    hasServerInput = false;
    hasClockCycle = false;
  }
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
  insQueue[stallIndex] = 0;
  
  for (j = 2; j > stallIndex-2 && j > 0; j--) {
    aluQueue[j] = aluQueue[j - 1];
  }
  aluQueue[0] = 0;
  
}

long getNextPc() {
  if(stallIndex > 0){
    return pcQueue[0];
  }

  if(hasArithmeticOverflow){
    return 0x80020000;
  }

  if(hasInvalidInstruction){
    return 0x80010000;
  }
  
  long nextPc = pcQueue[0] + 4;  
  
  if (toJumpOrBranch == 1) {
    nextPc = (insQueue[2] & 0x03ffffff) << 2;
  }
  
  if(toJumpOrBranch == 2) {    
    nextPc = pcQueue[2] + 4;
    long imm = insQueue[2] & 0xffff;
    if(imm >> 15 == 1){      
      nextPc += (imm - 0x10000) << 2;
    }else{
      nextPc += imm << 2;
    }
  }
  
  return nextPc;
}

//Call every clock cycle to move pipeline and update pipeline variables to input to server
void updatePipelineInput() {
  aReadReg1Input = (insQueue[1] >> 21) & 0x1f;
  bReadReg2Input = (insQueue[1] >> 16) & 0x1f;


  insIdentify(insQueue[4]);
  if(strcmp(operation, "addi") == 0 || strcmp(operation, "lw") == 0){
    cWriteRegInput = (insQueue[4] >> 16) & 0x1f;
  }else{
    cWriteRegInput = (insQueue[4] >> 11) & 0x1f;
  }
  
  if (strcmp(operation, "lw") == 0) {
    dWriteDataReg = cReadDataMemOutput;
  } else {
    dWriteDataReg = aluQueue[2];
  }

  eRegWrite = strcmp(operation, "j") != 0 && strcmp(operation, "beq") != 0 && strcmp(operation, "sw") != 0 && strcmp(operation, "nop") != 0 && strcmp(operation, "invalid") != 0;

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
      } case 0:{
          strcpy(operation, "nop");
          break;
      } default:
        strcpy(operation, "invalid");
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
        strcpy(operation, "invalid");
    }
  }
}

void checkExHazards(){
  long rdMem;//rd in MEM
  long rdWb;//rd in EX
  long rsEx;
  long rtEx;
  bool isMemHaveRd = false;
  bool isWbHaveRd = false;
  bool isExHaveRead = false;

  //review instruction and rd in MEM
  insIdentify(insQueue[3]);
  if(strcmp(operation, "add") == 0 || strcmp(operation, "sub") == 0|| strcmp(operation, "and") == 0 || strcmp(operation, "or") == 0 || strcmp(operation, "slt") == 0){
      rdMem = (insQueue[3] >> 11) & 0x1f;
      isMemHaveRd = true;
  }else if(strcmp(operation, "addi") == 0){
      rdMem = (insQueue[3] >> 16) & 0x1f;
      isMemHaveRd = true;
  }

  //review instruction and rd in WB
  insIdentify(insQueue[4]);
  if(strcmp(operation, "add") == 0 || strcmp(operation, "sub") == 0|| strcmp(operation, "and") == 0 || strcmp(operation, "or") == 0 || strcmp(operation, "slt") == 0){
      rdWb = (insQueue[4] >> 11) & 0x1f;
      isWbHaveRd = true;
  }else if(strcmp(operation, "addi") == 0 || strcmp(operation, "lw") == 0){
      rdWb = (insQueue[4] >> 16) & 0x1f;
      isWbHaveRd = true;
  }

  //review instruction rs and rt of EX
  insIdentify(insQueue[2]);
  if(strcmp(operation, "j") != 0 && strcmp(operation, "nop") != 0 && strcmp(operation, "invalid") != 0){
    rsEx = (insQueue[2] >> 21) & 0x1f;
    rtEx = (insQueue[2] >> 16) & 0x1f;
    isExHaveRead = true;
  }

  if(isExHaveRead){
    //check case 2 ex hazards first so that it can be overwritten by case 1
    if(isWbHaveRd){
      if(rsEx == rdWb){//data hazard found in in rsEx and rdMEM
        rsExHazardCase = 2;
        hasDataHazard = 1;
      }
      if(rtEx == rdWb){//data hazard found in in rtEX and rdMEM
        rtExHazardCase = 2;
        hasDataHazard = 1;
      }
    }  
    //check case 1 ex hazards
    if(isMemHaveRd){
      if(rsEx == rdMem){//data hazard found in in rsEx and rdMEM
        rsExHazardCase = 1;
        hasDataHazard = 1;
      }
      if(rtEx == rdMem){//data hazard found in in rtEX and rdMEM
        rtExHazardCase = 1;
        hasDataHazard = 1;
      }
    }
  }
}//end of checkExHazards

void checkBeqHazards(){
  long rdEx;//rd in EX
  long rdMem;//rd in MEM
  long rsId;
  long rtId;
  bool isExHaveRd = false;
  bool isMemHaveRd = false;
  bool isIdHaveBeq = false;

  //review instruction and rd in EX
  insIdentify(insQueue[2]);
  if(strcmp(operation, "add") == 0 || strcmp(operation, "sub") == 0|| strcmp(operation, "and") == 0 || strcmp(operation, "or") == 0 || strcmp(operation, "slt") == 0){
      rdEx = (insQueue[2] >> 11) & 0x1f;
      isExHaveRd = true;
  }else if(strcmp(operation, "addi") == 0){
      rdEx = (insQueue[2] >> 16) & 0x1f;
      isExHaveRd = true;
  }

  //review instruction and rd in MEM
  insIdentify(insQueue[3]);
  if(strcmp(operation, "add") == 0 || strcmp(operation, "sub") == 0|| strcmp(operation, "and") == 0 || strcmp(operation, "or") == 0 || strcmp(operation, "slt") == 0){
      rdMem = (insQueue[3] >> 11) & 0x1f;
      isMemHaveRd = true;
  }else if(strcmp(operation, "addi") == 0 || strcmp(operation, "lw") == 0){
      rdMem = (insQueue[3] >> 16) & 0x1f;
      isMemHaveRd = true;
  }

  //review instruction rs and rt of ID
  insIdentify(insQueue[1]);
  if(strcmp(operation, "beq") == 0){
    rsId = (insQueue[1] >> 21) & 0x1f;
    rtId = (insQueue[1] >> 16) & 0x1f;
    isIdHaveBeq = true;
  }

if(isIdHaveBeq){
  //check case 2 hazards first so that it can be overwritten by case 1
  if(isMemHaveRd){
    if(rsId == rdMem){//data hazard found in in rsEx and rdMEM
      rsBeqHazardCase = 2;
      hasDataHazard = 1;
    }
    if(rtId == rdMem){//data hazard found in in rtEX and rdMEM
      rtBeqHazardCase = 2;
      hasDataHazard = 1;
    }
  }
  //check case 1 hazards
  if(isExHaveRd){
    if(rsId == rdEx){//data hazard found in in rsEx and rdMEM
      rsBeqHazardCase = 1;
      hasDataHazard = 1;
    }
    if(rtId == rdEx){//data hazard found in in rtEX and rdMEM
      rtBeqHazardCase = 1;
      hasDataHazard = 1;
    }
  }
}

}//end of checkBeqHazards

void checkLwHazards(){
  long rdEx;//rd in EX
  long rdMem;//rd in MEM
  long rsId;
  long rtId;
  bool isExHaveLw = false;
  bool isMemHaveLw = false;
  bool isIdHaveRead = false;

  //review instruction and rd in EX
  insIdentify(insQueue[2]);
  if(strcmp(operation, "lw") == 0){
      rdEx = (insQueue[2] >> 16) & 0x1f;
      isExHaveLw = true;
  }

  //review instruction and rd in MEM
  insIdentify(insQueue[3]);
  if(strcmp(operation, "lw") == 0){
      rdMem = (insQueue[3] >> 16) & 0x1f;
      isMemHaveLw = true;
  }

  //review instruction rs and rt of Id
  insIdentify(insQueue[1]);
  if(strcmp(operation, "j") != 0 && strcmp(operation, "nop") != 0 && strcmp(operation, "invalid") != 0){
    rsId = (insQueue[1] >> 21) & 0x1f;
    rtId = (insQueue[1] >> 16) & 0x1f;
    isIdHaveRead = true;
  }

  if(isIdHaveRead){
    //check case 2 hazards first so that it can be overwritten by case 1
    if(isMemHaveLw){
      if(rsId == rdMem){//data hazard found in in rsEx and rdMEM
        rsLwHazardCase = 2;
        hasDataHazard = 1;
      }
      if(rtId == rdMem){//data hazard found in in rtEX and rdMEM
        rtLwHazardCase = 2;
        hasDataHazard = 1;
      }
    }  
    //check case 1 hazards
    if(isExHaveLw){
      if(rsId == rdEx){//data hazard found in in rsEx and rdMEM
        rsLwHazardCase = 1;
        hasDataHazard = 1;
      }
      if(rtId == rdEx){//data hazard found in in rtEX and rdMEM
        rtLwHazardCase = 1;
        hasDataHazard = 1;
      }
    }
  }
}//end of checkLwHazards

void aluExecute() { //executes instructions
  long s;
  long t;

  long imm;
  long address;

  imm = insQueue[2] & 65535; 
  //sign-extend the immediate
  if(imm >> 15 == 1){      
    imm = imm - 0x10000;
  }

  //get possible forwarded value of rs
  if(rsExHazardCase == 2){
    s = aluQueue[2];
  }else if(rsExHazardCase == 1){
    s = aluQueue[1];
  }else{
    s = aReadDataReg1Output;
  }

  //get possible forwarded value of rt
  if(rtExHazardCase == 2){
    t = aluQueue[2];
  }else if(rtExHazardCase == 1){
    t = aluQueue[1];
  }else{
    t = bReadDataReg2Output;
  }
  char output[40];
  //ALU Execution
  insIdentify(insQueue[2]);
  if (strcmp(operation, "add") == 0) {
    aluQueue[0] = s + t;
    if(s >> 31 == t >> 31 && aluQueue[0] >> 31 != s >> 31){
       hasArithmeticOverflow = 1;
    }
//    sprintf(output, "result of ADD: %08lx", aluQueue[0]);
//    Serial.print(output);
//    Serial.write(0);
  } else if (strcmp(operation, "sub") == 0) {
    aluQueue[0] = s - t;
    if(s >> 31 != t >> 31 && aluQueue[0] >> 31 != s >> 31){
       hasArithmeticOverflow = 1;
    }
  } else if (strcmp(operation, "and") == 0) {
    aluQueue[0] = s & t;
  } else if (strcmp(operation, "or") == 0) {
    aluQueue[0] = s | t;
  } else if (strcmp(operation, "slt") == 0) {
    aluQueue[0] = s < t;
  } else if (strcmp(operation, "addi") == 0) {
    aluQueue[0] = s + imm;
    if(s >> 31 == imm >> 31 && aluQueue[0] >> 31 != s >> 31){
       hasArithmeticOverflow = 1;
    }    
//    sprintf(output, "result of ADDI: %08lx", aluQueue[0]);
//    Serial.print(output);
//    Serial.write(0);
  }else if (strcmp(operation, "sw") == 0 || strcmp(operation, "lw") == 0){
    aluQueue[0] = s + imm;
  }

}//end of alualuExecute function

void idBeqResolve(){
  long s;
  long t;
  
  insIdentify(insQueue[1]);
  if(strcmp(operation, "j") == 0){//jump if jump is read
    toJumpOrBranch = 1;
  }else if(strcmp(operation, "beq") == 0) {//branch if beq is read and rs == rt
    //get possible forwarded value of rs
    if(rsBeqHazardCase == 2){
      s = aluQueue[1];
    }else if(rsBeqHazardCase == 1){
      s = aluQueue[0];
    }else{
      s = aReadDataReg1Output;
    }  
    //get possible forwarded value of rt
    if(rtBeqHazardCase == 2){
      t = aluQueue[1];
      Serial.print("Hello");
      Serial.write(TERMINATOR);
    }else if(rtBeqHazardCase == 1){
      t = aluQueue[0];
      Serial.print("Hi");
      Serial.write(TERMINATOR);
    }else{
      t = bReadDataReg2Output;
    }
    if(s == t && stallIndex == 0) {
      toJumpOrBranch = 2;
    }
  }  
}

void debugPipelineQueues() {
  Serial.println("<< DEBUGGING PIPELINE QUEUES - START  >>");
  char output[MAX_STRLEN];
  sprintf(output, "PC at IF: %08lx\tIns at IF/ID: %08lx", pcQueue[0], insQueue[0]);
  Serial.println(output);
  sprintf(output, "PC at ID: %08lx\tIns at ID/EX: %08lx", pcQueue[1], insQueue[1]);
  Serial.println(output);  
  sprintf(output, "PC at EX: %08lx\tIns at EX/MEM: %08lx\tALU at EX: %08lx", pcQueue[2], insQueue[2], aluQueue[0]);
  Serial.println(output);  
  sprintf(output, "PC at MEM: %08lx\tIns at MEM/WB: %08lx\tALU at MEM: %08lx", pcQueue[3], insQueue[3], aluQueue[1]);
  Serial.println(output);
  sprintf(output, "PC at WB: %08lx\tIns at WB/NIL: %08lx\tALU at WB: %08lx", pcQueue[4], insQueue[4], aluQueue[2]);
  Serial.println(output);
  Serial.println("<< DEBUGGING PIPELINE QUEUES - END  >>");
}


void debugExecute(){
  Serial.print("<< FINISHED EXECUTING: ");
  Serial.print(operation);
  Serial.print(" >>");
  Serial.write(TERMINATOR);
}


void debugIdBeqResolve(){
  if(ToBranchJump){
  Serial.print("<< YES, It will branch on next clock cycle >>");
  }else{
  Serial.print("<< NO, It wont branch on next clock cycle >>");
  }  
  Serial.write(TERMINATOR);
}

void debugHazardsExceptions(){
  Serial.println("<< DEBUGGING HAZARDS AND EXCEPTIONS - START  >>");
  Serial.print("Data hazard: ");
  Serial.println(hasDataHazard);
  Serial.print("Control hazard: ");
  Serial.println(hasControlHazard);
  Serial.print("invalid Instruction: ");
  Serial.println(hasInvalidInstruction);
  Serial.print("Arithmetic Overflow: ");
  Serial.println(hasArithmeticOverflow);
  Serial.println("<< DEBUGGING HAZARDS AND EXCEPTIONS - END  >>");
}
void noBreadboardStart(){
 // Serial.println("imagine the START switch was pressed at this moment");
  Serial.print("START");
  Serial.write(TERMINATOR);
  hasStartRequested = true;
}

void noBreadboardClock(){
 // Serial.println("imagine the CLOCK switch was pressed at this moment");
  hasClockCycle = true;
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
    hasStartCommenced = false;
    hasClockCycle = false;
    hasServerInput = false;
    hasRestartBaby = false;
    toJumpOrBranch = false;

    hasControlHazard = 0;
    hasDataHazard = 0;
    hasInvalidInstruction = 0;
    hasArithmeticOverflow = 0;
    digitalWrite(dataHazardLED, hasDataHazard);
    digitalWrite(controlHazardLED, hasControlHazard);
    digitalWrite(arithmeticExceptionLED, hasArithmeticOverflow);
    digitalWrite(invalidExceptionLED, hasInvalidInstruction);
   
  }

  //check for falling edge
  if (currStartState == LOW && prevStartState == HIGH) {
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
  }

  //check for falling edge
  if (currClockState == LOW && prevClockState == HIGH) {
  }

  prevClockState = currClockState;
}


void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

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
    aluResultEx = s + imm;
    //aluResultEx = DDDDDDDD goes to d register that will be saved in CCCCC
  }else if(strcmp(operation, "lw") == 0){
    
  }else if(strcmp(operation, "sw") == 0){
    
  }else if(strcmp(operation, "beq") == 0){
    
  }else if(strcmp(operation, "j") == 0){
  
  }
}

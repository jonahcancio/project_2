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

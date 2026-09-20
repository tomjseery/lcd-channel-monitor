//#define DEBUG
#ifdef DEBUG
#define debug_print(x) Serial.print(x)
#define debug_println(x) Serial.println(x)
#define debug_printarray(x) printArray(x)
#define debug_memoryoverload(x) if(x==NULL) Serial.println(F("ERROR: MEMORY OVERLOAD!"));
#define debug_printrecent(x) printRecent(x)
#else
#define debug_print(x) {}
#define debug_println(x) {}
#define debug_printarray(x) {}
#define debug_memoryoverload(x) {}
#define debug_printrecent(x) {}
#endif

/*
 * The scroll debug will print once every second, which may be distracting if you want to view the results of other
 * processes in the system.
 * So I have decided to seperate it.
 */
//#define DEBUG_SCROLL
#ifdef DEBUG_SCROLL
#define debugsc_print(x) Serial.print(x);
#define debugsc_println(x) Serial.println(x);
#else
#define debugsc_print(x) {}
#define debugsc_println(x) {}
#endif

#include <Wire.h>
#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>
#include <EEPROM.h>

Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

/*
 * PREREQUISITES OF MY PROGRAM:
 * In my commenting, I use the terms 'top' and 'bottom' in quotations to illustrate the top element, and bottom element currently being displayed in the lcd respectively
 */

byte upArrow[8] = {
  0b00100,
  0b01110,
  0b11111,
  0b00100,
  0b00100,
  0b00100,
  0b00100,
  0b00100
};
byte downArrow[8] = {
  0b00100,
  0b00100,
  0b00100,
  0b00100,
  0b00100,
  0b11111,
  0b01110,
  0b00100
};
byte sp=0; //stack pointer (points to the end of end of the stackLetters, and therefore orderedLetters array)
byte cp=0; //current pointer on the lcd
byte clp=0; //current pointer for the 'left' letters
byte crp=0;  //current pointer for the 'right' letters
byte lsp=0; //left stack pointer (points to the end of end of the leftLetters array)
byte rsp=0; //right stack pointer (points to the end of end of the rightLetters array)

short last_b=0;

struct LinkedList{
  byte val; //The value
  struct LinkedList *next; //along with a pointer towards the next element
};

struct Channel{
  char letter='.'; //full stop means null
  short value=-1; //would be a byte but we need to have -1 to represent a "NULL" value since you cant have a null number in C
  String desc;
  byte _max=255; //Default max is set to 255 for all channels
  byte _min=0; //Default min is set to 255 for all channels
  LinkedList *recent = NULL;
  short count=0; //No of values currently stored in recent
}; //each channel has the following above categories, so store all the attributes in a struct
Channel channels[26]; //Creates an array of the struct blueprint above for all the 26 channels to be stored on

char stackLetters[26]; //Stores the unordered list of created channels
char orderedLetters[26]; //ordered set of the stackLetters above so they can be displayed in alphabetical order on the lcd
char leftLetters[26]; //Stores the ordered set of letters below the minimum
char rightLetters[26]; //Stores the ordered set of letters above the maximum

enum State { 
  INITIALISATION = 1, 
  WAITING_RESPONSE = 2,
  UP_PRESSED = 3, 
  DOWN_PRESSED =4, 
  LEFT_PRESSED =5,
  RIGHT_PRESSED =6,
  SELECT_HELD=7,
  WAITING_RELEASE =8
  };
enum State state = INITIALISATION;
enum State innerState = WAITING_RESPONSE;

unsigned long pressTime=0;

static byte lp1=0; //Left and right pointer for the scrolling function for the top row
static byte rp1=6;

static byte lp2=0; //Left and right pointer for the scrolling function for the bottom row
static byte rp2=6;

void setup() {
  Serial.begin(9600);
  lcd.begin(16,2);
  lcd.createChar(0,upArrow); //Sets the byte '0' to be interpreted as a up arrow in the lcd
  lcd.createChar(1,downArrow); //Sets the byte '0' to be interpreted as a down arrow in the lcd
}

void loop() {
  switch(state){
    case INITIALISATION:
      lcd.setBacklight(5);
      {
      String input;
        do{
        input = Serial.readString(); //Read the input and store it
        if(millis()-pressTime>=1000){ //Wait 1 second
          pressTime=millis();
          Serial.print("Q"); 
        }
        }while(input.length()-1 != 1 || input[0]!='X'); //Continue running this until the input is equal to just 'X'
        Serial.println();
        Serial.println(F("RECENT,FREERAM,HCI,EEPROM,SCROLL,UDCHARS,NAMES")); //The tasks I have done
        lcd.setBacklight(7); //Change the backlight colour to white
        initChannels(); //Initialize the channels from the EEPROM
        state=WAITING_RESPONSE; //update to the main state
      }
      break;
    case WAITING_RESPONSE:
      checkInput(); //check if theres an input
      checkButton(); //check if theres a button press
      scroll(2); //Run the boolean query to test whether the lcd needs to scroll or not
      break;
    case DOWN_PRESSED:
      if(cp+2<=sp-1){ //If theres an element below the current bottom element on the lcd
        cp++; //increment the current pointer
        updateDisplay(); //refresh the display
      }
      state=WAITING_RESPONSE;
      break;
    case UP_PRESSED:
      if(cp-1>=0){ //If theres an element behind the letters array
        cp--; //decrement the pointer
        updateDisplay(); //reupdate the display with the new pointer information
      }
      state=WAITING_RESPONSE; //Once UP has been dealt with, go back to the main state
      break;
    case RIGHT_PRESSED:
      /*
       * If right has been pressed, continue running the same state enum, but as a substate, so that 
       * The system can continue running the states as normal in the substate, whilst storing the fact that 
       * The system is in the LEFT_PRESSED state
       */
      switch(innerState){ //Almost the same implementation as the above state, but some differences in the output in the display since the outer state is RIGHT_PRESSED, so will print the rightLetters instead of all
        case WAITING_RESPONSE:
          checkInput();
          checkButton();
          scroll(6); //Check the scroll function, but with the parameters associated with the 'right' array
          break;
        case UP_PRESSED:
          if(crp-1>=0){ //If there are any element above the current top element in the lcd
            crp--;
            updateDisplay(); 
          }
          innerState=WAITING_RESPONSE;
          break;
        case DOWN_PRESSED:
          if(crp+2<=rsp-1){
            crp++;
            updateDisplay();  
          }
          innerState=WAITING_RESPONSE;
          break;
          
        case LEFT_PRESSED:
          state=LEFT_PRESSED;
          innerState=WAITING_RESPONSE;
          updateDisplay();
          break;
        case RIGHT_PRESSED:
          innerState=WAITING_RESPONSE;
          state=WAITING_RESPONSE;
          updateDisplay();
          break;
        case SELECT_HELD:
          checkInput();
          {
          int b = lcd.readButtons();
          int released = ~b & last_b;
          last_b=b;
          if(released){
            innerState=WAITING_RESPONSE; //Update the substate back to the main state
          }else{
            if(millis()-pressTime>=1000){ 
              lcd.clear();
              lcd.setBacklight(5);
              lcd.setCursor(0,0);
              lcd.print(F("F117177"));
              lcd.setCursor(0,1);
              lcd.print(freeMemory());
              innerState=WAITING_RELEASE; //Change substate to WAITING_RELEASE of the SELECT button
            }
          }
          }
          break;
        case WAITING_RELEASE:
          checkInput();
          {
            int b = lcd.readButtons();
            int released = ~b & last_b;
            last_b=b;
            if(released){
              innerState=WAITING_RESPONSE; //substate goes back to the main state of the program
              updateDisplay(); //update the display with the state and updated substate information
            }
          }
          break;
      }
      break;
    case LEFT_PRESSED:
      /*
       * If left has been pressed, continue running the same state enum, but as a substate, so that 
       * The system can continue running the states as normal in the substate, whilst storing the fact that 
       * The system is in the LEFT_PRESSED state
       */
      switch(innerState){
        case WAITING_RESPONSE:
          checkInput();
          checkButton();
          scroll(5); //Check the scroll function, but with the parameters associated with the 'left' array
          break;
        case UP_PRESSED:
          if(clp-1>=0){
            clp--;
            updateDisplay(); 
          }
          innerState=WAITING_RESPONSE;
          break;
        case DOWN_PRESSED:
          if(clp+2<=lsp-1){
            clp++;
            updateDisplay();  
          }
          innerState=WAITING_RESPONSE;
          break;
        case LEFT_PRESSED: //If LEFT was pressed, whilst also being in the LEFT state
          innerState=WAITING_RESPONSE; //Reset the substate
          state=WAITING_RESPONSE; //Go back to the main state of the program
          updateDisplay();
          break;
        case RIGHT_PRESSED: //If RIGHT was pressed, whilst also being in the LEFT state
          state=RIGHT_PRESSED; //change state to RIGHT_PRESSED instead
          innerState=WAITING_RESPONSE; //reset the substate
          updateDisplay(); //update display with updated states
          break;
        case SELECT_HELD:
          checkInput();
          {
          int b = lcd.readButtons();
          int released = ~b & last_b;
          last_b=b;
          if(released){
            innerState=WAITING_RESPONSE;
          }else{
            if(millis()-pressTime>=1000){
              lcd.clear();
              lcd.setBacklight(5);
              lcd.setCursor(0,0);
              lcd.print(F("F117177"));
              lcd.setCursor(0,1);
              lcd.print(freeMemory());
              innerState=WAITING_RELEASE;
            }
          }
          }
          break;
        case WAITING_RELEASE:
          checkInput();
          {
            int b = lcd.readButtons(); 
            int released = ~b & last_b; 
            last_b=b; 
            if(released){ 
              innerState=WAITING_RESPONSE; 
              updateDisplay();
            }
          }
          break;
      }
      break;
    case SELECT_HELD:
      checkInput();
      {
      int b = lcd.readButtons();
      int released = ~b & last_b;
      last_b=b;
      if(released){ //if select was released
        state=WAITING_RESPONSE; //go back to the default state
      }else{
        if(millis()-pressTime>=1000){ //if select hasnt been released yet
          //clear the lcd and display information over it
          lcd.clear(); 
          lcd.setBacklight(5);
          lcd.setCursor(0,0);
          lcd.print(F("F117177"));
          lcd.setCursor(0,1);
          lcd.print(freeMemory());
          state=WAITING_RELEASE; 
        }
      }
      }
      break;
    case WAITING_RELEASE:
      /*
       * Continues staying in this state indefinitely until the select button is released
       */
      checkInput();
      {
        int b = lcd.readButtons(); //read current button press (if there is one)
        int released = ~b & last_b; //check whether button press has changed from before
        last_b=b; //update last button press to current
        if(released){ //if SELECT has been released
          state=WAITING_RESPONSE; //Go back to the WAITING_RESPONSE state
          updateDisplay(); //update the display with the state just being updated
        }
      }
      break;  
  }
}

void scroll(int currentState){
  char *letters; //define the pointer to the array to be used
  byte *p; //define pointer to current pointer 
  byte *ep; //define pointer to end pointer
  if(state==5){ //if state is LEFT_PRESSED, set the array and pointers to that associated with the leftLetters array
    letters=leftLetters;
    ep=&lsp;
    p=&clp;
  }else if(state==6){ //if state is RIGHT_PRESSED, set the array and pointers to that associated with the rightLetters array
    letters=rightLetters;
    ep=&rsp;
    p=&crp;
  }else{ //state was in WAITING_RESPONSE, so set the array and pointers to that associated with the orderedLetters array
    letters=orderedLetters;
    ep=&sp;
    p=&cp;
  }
  checkInput();
  checkButton();
  if(currentState!=state){ //If the state from when it was called doesn't match the current state of the system
    updateDisplay(); //update the display to allign with the new state
    return; //Exit this function
  }
  if(millis()-pressTime>=500){ //wait half a second before beginning scrolling
        debugsc_print("Current pointer: ");
        debugsc_println(p);
        debugsc_print("End pointer: ");
        debugsc_println(ep);
        pressTime=millis();
        if(channels[getPos(letters[*p])].desc.length()>6 && (*ep-1)>=*p){ //if the length of the description of the top channel is greater than 7, and that channel actually exists
          if(rp1>channels[getPos(letters[*p])].desc.length()){ //first checks whether the right pointer has reached the end of the description
            //if so, reset the pointers
            lp1=0;
            rp1=6;
            debugsc_println(F("Reset top pointers"));
          }
          updateRow(0,channels[getPos(letters[*p])].letter,(channels[getPos(letters[*p])].desc).substring(lp1++,rp1++),*p,*ep,letters); //update the top line of the lcd, with the substring of the description, along with its incremented pointers
          debugsc_print(F("Row: "));
          debugsc_println(0);
          debugsc_print(F("Left Pointer: "));
          debugsc_println(lp1);
          debugsc_print(F("Right Pointer: "));
          debugsc_println(rp1);
          debugsc_print(F("Substring: "));
          debugsc_println((channels[getPos(letters[*p])].desc).substring(lp1,rp1));
        }
        if(channels[getPos(letters[*p+1])].desc.length()>6 && (*ep-1)>=*p+1){ //Same as above but for the bottom channel on the lcd
          if(rp2>channels[getPos(letters[*p+1])].desc.length()){
            //If so reset the pointers
            lp2=0;
            rp2=6;
            debugsc_println(F("Reset bottom pointers"));
          }

            updateRow(1,channels[getPos(letters[*p+1])].letter,(channels[getPos(letters[*p+1])].desc).substring(lp2++,rp2++),*p+1,*ep,letters); //update the bottom line of the lcd, with the substring of the description, along with its incremented pointers  
          
          debugsc_print(F("Row: "));
          debugsc_println(1);
          debugsc_print(F("Left Pointer: "));
          debugsc_println(lp2);
          debugsc_print(F("Right Pointer: "));
          debugsc_println(rp2);
          debugsc_print(F("Substring: "));
          debugsc_println((channels[getPos(letters[p+1])].desc).substring(lp2,rp2));
        }
  }
}


void checkButton(){
  uint8_t b = lcd.readButtons(); //reads current button press
  int released = ~b & last_b; 
  int pressed = b & ~last_b; 
  /*
   * The two above variables both check whether the button is different to the button pressed before
   * released negates the current button press as if the current button press was false (no button currently pressed) and the previous button was
   * true (a button was pressed before) then that would be classed as true and NOT false, which is true (meaning the current button was just released)
   * pressed negates the previous button press, as if the previous button press was false (no button pressed), and the current button is true
   * (that button has JUST been pressed), then the logic statement would be NOT false AND true, which is true (meaning the current button was just pressed now)
   */
  last_b = b; //updates the last button press

    if ((released  & BUTTON_UP)) {
      if(state==2){ //Normal display (without left and right)
        state=UP_PRESSED; //update outer state
      }else if(state==5 || state==6){ //If the left or right button was pressed
        innerState=UP_PRESSED; //update inner state
      }
    }
    else if ((released & BUTTON_DOWN)) {
      if(state==2){
        state=DOWN_PRESSED;
      }else if(state==5 || state==6){
        innerState=DOWN_PRESSED;
      }
    }
    else if ((released & BUTTON_LEFT)) {
      if(state==2){ //if neither left or right was pressed before this
        state=LEFT_PRESSED; //update outer state
      }else if(state==5 || state==6){ //if left or right was pressed before this
        innerState=LEFT_PRESSED; //update the inner state
      }
      updateDisplay(); //reupdate the display with the new state information
    }
    else if ((released & BUTTON_RIGHT)) { //same process for right but for RIGHT_PRESSED state
      if(state==2){
        state=RIGHT_PRESSED;
      }else if(state==6 || state==5){
        innerState=RIGHT_PRESSED;
      }
      updateDisplay();
    }
    else if (pressed & BUTTON_SELECT) { //if the SELECT button has JUST been pressed now
      pressTime=millis(); //store the current runtime
        if(state==2){ //if neither left or right is pressed
          state=SELECT_HELD; //update outer state
        }else if(state==5 || state==6){ //if left or right is pressed
          innerState=SELECT_HELD; //update inner state
        }
    }  
}

void updateDisplay(){
  lcd.clear();
  resetPointers(); //If display was updated, it means a meaningful change was made in the system, so reset the pointers associated with the scroll function
  char *letters; //points to orderedList of channels depending on the current state of the system
  byte* p; //current pointer - points to the current pointer of the letters array
  byte* ep; //end pointer - points to the end of the letters array
  if(state==5){ //if state is LEFT_PRESSED, set the array and pointers to that associated with the leftLetters array
    letters=leftLetters;
    ep=&lsp;
    p=&clp;
  }else if(state==6){ //if state is RIGHT_PRESSED, set the array and pointers to that associated with the rightLetters array
    letters=rightLetters;
    ep=&rsp;
    p=&crp;
  }else{ //state was in WAITING_RESPONSE, so set the array and pointers to that associated with the orderedLetters array
    letters=orderedLetters;
    ep=&sp;
    p=&cp;
  }
  debug_print(F("Current state: "));
  debug_println(state);
  debug_print(F("letters on display: "));
  debug_printarray(letters);
  debug_print(F("Current pointer: "));
  debug_println(*p);
  debug_print(F("End pointer: "));
  debug_println(*ep);

  
  if((*ep-1)>=*p){ //If the current pointer is behind, or in the same position as the end pointer:
    Channel currentChannel = channels[getPos(letters[*p])];
    String line1 = (String)currentChannel.letter; //get the letter of the channel
    line1.concat(numberToSpace(currentChannel.value)+currentChannel.value+','+numberToSpace(getAvg(letters[*p]))+getAvg(letters[*p])+' '+currentChannel.desc); //concatenate the value, average, and description associated with that letter
    lcd.setCursor(1,0); //set the cursor to the top, 1 element across
    lcd.print(line1); //write the line to that position
    debug_println("Written to row 1");
    if((*ep-1)>=*p+1 && (*p+1!=*ep)){ //Current pointer + 1 points to the 'bottom', so if that is behind, or in the same position as end pointer:
      Channel currentChannel = channels[getPos(letters[*p+1])];
      String line2 = (String)currentChannel.letter;
      line2.concat(numberToSpace(currentChannel.value)+currentChannel.value+','+numberToSpace(getAvg(letters[*p+1]))+getAvg(letters[*p+1])+' '+currentChannel.desc); 
      lcd.setCursor(1,1); //set the cursor to the bottom, 1 element across
      lcd.print(line2);
      debug_println("Written to row 2");
    }
    if((*ep-1)>=*p+2){ //If there is at least one more element above the current pointer
      renderDown();
    }if(*p-1>=0){ //If there is at least one more element below the current pointer
      renderUp();
    }
  }
  lcd.setBacklight(flag(letters)); //set the backlight based on the max's and min's
}

void resetPointers(){ //Used when page is refreshed and pointers to both string descriptions get reset for the next display
  lp1=0;
  rp1=6;
  lp2=0;
  rp2=6;
}

void printArray(char arr[]){
  for(int i=0; i<strlen(arr); i++){
    Serial.print(arr[i]);
    Serial.print(",");
  }
  Serial.println("");
}

void printArray(int arr[]){
  for(int i=0; i<arraysize(arr); i++){
    Serial.print(arr[i]);
    Serial.print(",");
  }
  Serial.println("");
}

void createRecent(char c, int v){
  int i = getPos(c); //Convert letter to its Channel index
  
  channels[i].recent = (LinkedList*)malloc(sizeof(LinkedList)); //since its currently null, you want to allocate memory to it
  debug_memoryoverload(channels[i].recent);
  channels[i].recent->val=v; //add the value to the allocated block of memory
  channels[i].recent->next=NULL; //set the next pointer to null
  channels[i].count++; 
}

void push(char c, int v){
  int i = getPos(c);
  if(channels[i].count<63){ //If that counter hasnt passed 63
    LinkedList *current = channels[i].recent; //gets head and sets it to current element
    while(current->next != NULL){ //Run this until the next element in the LinkedList is null
        current=current->next; //Keep going to the next element
    } //current now equals the end of the LinkedList
    current->next = (LinkedList*)malloc(sizeof(LinkedList)); //since its null, you want to allocate memory to it
    debug_memoryoverload(current->next);
    current->next->val=v; //Add the value to the allocated block of memory
    current->next->next=NULL; //Set the next pointer to null
    channels[i].count++; //Increment the recent counter
  }else{ //If the list has passed its storage limit
    pop(c); //Pop the first element
    channels[i].count--; //Decrement the counter
    push(c,v); //Recall the push function
  }
}
  
int pop(char c){ //removes the first element of the linked list
  //channels[i].recent is the 'head' of the LinkedList associated with that channel
  int i = getPos(c);
  LinkedList *nextElement = NULL;
  if(channels[i].recent==NULL){ //If the head is null
    return -1; //nothing to pop
  }
  nextElement = channels[i].recent->next;
  int rtn = channels[i].recent->val;
  free(channels[i].recent); //Removes the head
  channels[i].recent = nextElement; //Replace the head with the element after it

  return nextElement->val;
}

void printRecent(char c){
  Serial.print(F("Recent: "));
  int i = getPos(c);
  LinkedList *current = channels[i].recent;
  while (current != NULL) {
      Serial.print(current->val);
      Serial.print(", ");
      current = current->next;
  }
  Serial.println();
}

int getAvg(char c){
  int sum=0; //Store a sum and set it initially to 0
  int i = getPos(c); //Get the position associated with that channels letters
  LinkedList *current = channels[i].recent; //Set the current element in the LinkedList to the head 
  while (current != NULL) { //untill there is no element after it
      sum+=current->val; //Add the elements value to the sum
      current = current->next; //Go to the next element
  }
  return sum/channels[i].count; //Average = Sum/No of elements
}

int arraysize(int list[]){ return((sizeof(list))/(sizeof(list[0])));}

void updateRow(int row, char c, String newDesc, int p, int ep, char letters[]){
  clearSpace(row); //Clear the top or bottom row based on row parameter
  lcd.setCursor(10,row); //Clear the entire row, excluding the arrow slot
  
  lcd.print(newDesc);
}

//Clears the space that will occupy the new description substring
void clearSpace(int row){
  lcd.setCursor(9,row);
  lcd.print(F("      "));
}

/*
 * For each number, concatentate a space and return the result
 */
String numberToSpace(int n){
  int noOfSpaces = 3 - String(n).length();
  String spaces = "";
  for(int i=0;i<noOfSpaces;i++){ 
    spaces.concat(" "); 
  }
  return spaces;
}

void renderUp(){
  lcd.setCursor(0,0);
  lcd.write((byte)0);
}

void renderDown(){
  lcd.setCursor(0,1);
  lcd.write((byte)1);
}

void checkInput(){ 
  while(Serial.available()){ //While the Serial has data to read
    String input = Serial.readString(); //Get the input 
    char function = input[0]; 
    char letter = input[1];
    if((input.length()-1)>2){ //If the input length is greater than 3
      switch(function){
        case 'C':{
          String desc;
          if(input.length()-1>=17){ //If the input length is greater than 17 (i.e. description is longer than 15)
            desc = input.substring(2,17); //Description is the 2nd to 17th character
          }else{
            desc = input.substring(2,input.length()-1); //otherwise, get the rest of the string as description
          }
          debug_print(F("Desc: "));
          debug_println(desc);
          if(validChannel(letter)){
            initChannel(letter,desc); //Initialise the channel
            updateDisplay(); //Update the display with the new description
            store(letter,desc); //Store channel in EEPROM
          }else{
            printError(input);
          }
        }
          break;
        case 'V':
          if(validChannel(letter) && channelSet(letter)){
            String data = input.substring(2);
            if(isNumber(data)){
              int value = data.toInt();
              if(inRange(value,letter)){
                setChannel(letter,value);  
                sortLetters();
                populateLeftAndRight();
              if(channels[getPos(letter)].recent==NULL){
                createRecent(letter,value);
              }else{
                push(letter,value);
              }
              debug_printrecent(letter);
              if(state!=8 && innerState !=8){
                updateDisplay();
              }
            }else{
              printError(input);
            }
            }else{
              printError(input);
            }
          }else{
            printError(input);
          }
          break;
        case 'X':
          if(validChannel(letter) && channelSet(letter)){
            String data = input.substring(2);
            if(isNumber(data)){
              int maximum = data.toInt();
              setMax(maximum,letter);
              populateLeftAndRight();
              if(state!=8 && innerState !=8){
                updateDisplay();
              }
              store(letter,maximum,1);
            }else{
              printError(input);
            }
          }else{
            printError(input);
          }
          break;
        case 'N':
          if(validChannel(letter) && channelSet(letter)){
            String data = input.substring(2);
            if(isNumber(data)){
              int minimum = input.substring(2).toInt();
              setMin(minimum,letter);
              populateLeftAndRight();
              if(state!=8 && innerState !=8){
                updateDisplay();
              }
              store(letter,minimum,0);
            }else{
              printError(input);
            }
            }else{
              printError(input);
            }
            
          break;
        default:
          printError(input);
          break;
      }
    }else if((input.length()-1)!=0){
      printError(input);
    }
  }
}

//Notify that an error has occured, followed by the text that caused the error
void printError(String text){
  Serial.print(F("ERROR "));
  Serial.println(text);
  }

bool isNumber(String data){
  bool valid=false;
  for(int i=0;i<data.length()-1;i++){ //Go through each index of the string
    if(isDigit(data[i])){ //If that value is a digit
      valid=true; //keep as vaid
    }else{
      return false; //Otherwise, not a number
      break;
    }
  }
  return valid;
}


//converts the letter to its position in the alphabet using its character code conversion
int getPos(char c){ 
  return ((int)c)-65;
}


//Returns whether the channels value in between 0 and 255
bool inRange(int v, char c){
  int i = getPos(c);
  return (v>=0) && (v<=255);
}


//Returns whether channel is in the range of A-Z
bool validChannel(char c){
  int i = getPos(c);
  return (i>=0 && i<=25);
}


//Returns whether the channel has been set and is ready to have a value inside it
bool channelSet(char c){
  int i = getPos(c);
  return (channels[i].letter!='.');
}

//Set the channel letter, along with the description
void initChannel(char c, String d){
  //Initialize position channel
  int i = getPos(c);
  channels[i].letter=c;
  channels[i].desc=d;

  /*
   * dont need to initialize stack leters here since this channel just gets the
   * channel ready for a value to be added to it. It doesnt actually add a value
   */
}

//Set the channels value
void setChannel(char c, int v){
  int i = getPos(c);
  if(channels[i].value==-1){
    //Set Stack channel while incrementing it
    stackLetters[sp++]=c; //Add letter to stack pointer, and increment stack pointer after
  }
  
  //Set position channel
  channels[i].value=v;

}

void sortLetters(){
  lsp=0; //Rest left and right end pointers
  rsp=0;
  int p=0; //Keeps track of the pointer towards the next position in the new array
  for(int i=0;i<26;i++){ //loops through channel names alphabetically
    for(int j=0;j<sp;j++){ //loops through the stack of current channels set
      if((channels[i].letter==stackLetters[j])&&(channels[i].value!=-1)){ //If the channel letter and current stack letter match, and the channel has been set
        orderedLetters[p++]=channels[i].letter; //Add to orderedLetters and increment its pointer
      }
    }
  }
}

void populateLeftAndRight(){ //can only be called once orderedLetters has been resorted since it adds elements based on the order of that array
  memset(leftLetters, 0, sizeof(leftLetters)); //Clear left array
  memset(rightLetters, 0, sizeof(rightLetters)); //Clear right array
  lsp=0; //Reset end pointers
  rsp=0;
  for(int i=0;i<26;i++){ //loops through channel names alphabetically
    for(int j=0;j<sp;j++){ //loops through the stack of current channels set
     if((channels[i].letter==stackLetters[j])&&(channels[i].value!=-1)&&(channels[i]._min<channels[i]._max)){
      if(channels[i].value<channels[i]._min){ //If the value of the channel is less than the min
          leftLetters[lsp++]=channels[i].letter;
      }if(channels[i].value>channels[i]._max){ //If the value of the channel is greater than the max
          rightLetters[rsp++]=channels[i].letter;
        }
     }
    }
  }
}

void setMax(int m, char c){
  if(m<=255){ 
    int i = getPos(c); //Get the position of the channel
    channels[i]._max=m; //Set its max
  }
}

void setMin(int m, char c){
  if(m>=0){
    int i = getPos(c); //Get the position of the channel
    channels[i]._min=m; //Set its min
  }
}

int flag(char letters[]){
  bool isMax = false;
  bool isMin = false;
  byte j;
  for(int i=0;i<strlen(letters);i++){ //Loop through array of letters
    j=getPos(letters[i]); //Get letters pointer to channel 
    if(channels[j].value!=-1 && channels[j]._min < channels[j]._max){ //cant equal -1 since thats the indication that it hasn't been set yet. Also min must be less than max in order to be considered in the colour change
    if(channels[j].value>channels[j]._max){ //If that value is greater than its max
      isMax=true; //Update the isMax
    }if(channels[j].value<channels[j]._min){ //If that value is less than its min
      isMin=true; //update the isMin
    }
    }
  }
  if(isMax || isMin){ 
    return (isMax && isMin) ? 3 : (isMax ? 1 : 2);
  }
  return 7;
}

void store(char c, String desc){
  int start = 18*getPos(c); //18 * the position of the character in the alphabet
  //18 since each letter occupies 18 blocks of space
  EEPROM.update(start,c);
  debug_print(F("Stored "));
  debug_print(c);
  debug_print(F(" in position "));
  debug_println(start);
  debug_print(F("Desc len: "));
  debug_println(desc.length());
  for(int i=0; i<desc.length();i++){
    debug_print(F("Current pos: "));
    debug_println(start+i+3);
    debug_print(F("Val: "));
    debug_println(desc[i]);
    EEPROM.update(start+i+3,byte(desc[i]));
  }
  debug_println(F("No of X's below = no of null spaces"));
  for(int i=desc.length();i<15;i++){ //For the rest 15 blocks that the description doesnt fill up
    debug_print(F("X"));
    EEPROM.update(start+i+3,255); //replace with 255 (represent null value)
  }
  debug_println();
}

void store(char c, int val, int pos){
  int start = 18*getPos(c);
  debug_print(F("Channel "));
  debug_print(c);
  if(pos==0 && val>=0){
    EEPROM.update(start+1,val); 
    debug_print(F("'s min: "));
    debug_println(val);
    debug_print(F("Pos: "));
    debug_println(start+1);
  }
  if(pos==1 && val<=255){
    EEPROM.update(start+2,val);
    debug_print(F("'s max: "));
    debug_println(val);
    debug_print(F("Pos: "));
    debug_println(start+2);
  }
}

void initChannels(){
  for(int i=0;i<26;i++){
    if(EEPROM.read(18*i)==65+i){ //check our data exists in the EEPROM
      channels[i].letter=char(65+i); //Convert to character and set as letter
      debug_print(channels[i].letter);
      debug_print(F(", "));
      channels[i]._min=EEPROM.read((18*i)+1); //Read position of min and set it
      debug_print(channels[i]._min); 
      debug_print(F(", "));
      channels[i]._max=EEPROM.read((18*i)+2); //Read position of max and set it
      debug_print(channels[i]._max);
      debug_print(F(", "));
      String desc=""; //Initially empty description
      for(int j=0; j<15;j++){ //description can occupy up to a max of 15 characters
        if(byte(EEPROM.read((18*i)+3+j))==255){ //If the current character is a 'null'
          break; //exit loop
        }
        desc+=char(EEPROM.read((18*i)+3+j)); //Add the current character to the descirption
      }
      channels[i].desc=((String)desc); //Set the description
      debug_print(channels[i].desc);
      debug_println();
    }
  }
}

//Source: Lab Worksheet 3
#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__

int freeMemory() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else  // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__
}

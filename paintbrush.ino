enum wipeStates {INERT, WIPING, RESOLVE};
byte wipeState = INERT;

byte colorHues[7] = {0, 40, 60, 100, 220};

byte prevFaceColors[6] = {0, 0, 0, 0, 0, 0};
byte faceColors[6] = {0, 0, 0, 0, 0, 0};
bool isBrush = false;

Timer brushCycle;
#define BRUSH_CYCLE_DURATION 100
byte brushFace = 0;

long timePainted[6];
#define PAINT_DURATION 1000

long timeBecamePaintBrush = 0;


void setup() {
  randomize();
}

void loop() {

  switch (wipeState) {
    case INERT:
      inertLoop();
      break;
    case WIPING:
      wipingLoop();
      break;
    case RESOLVE:
      resolveLoop();
      break;
  }

  //send data
  FOREACH_FACE(f) {
    byte sendData = (isBrush << 5) + (wipeState << 3) + (faceColors[f]);
    setValueSentOnFace(sendData, f);
  }

  //do display
  if (isBrush) {
    brushDisplay();
  } else {
    canvasDisplay();
  }
}

void inertLoop() {
  //do paint logic
  if (!isBrush) {

    //look for neighboring paint brushes
    // or neighboring paint
    // only paint brushes can paint over painted faces,
    // but paint can spread from painted canvas to blank canvas
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {//neighbor!
        byte neighborData = getLastValueReceivedOnFace(f);
        if (getBrush(neighborData) == 1) { //this neighbor is a brush
          byte neighborColor = getColor(neighborData);
          if (faceColors[f] != neighborColor) {
            prevFaceColors[f] = faceColors[f];
            faceColors[f] = neighborColor;
            timePainted[f] = millis();
          }
        }
        else {
          // not a brush, but could paint us
          if (faceColors[f] == 0) {
            // this is blank canvas, take on our neighbors color
            byte neighborColor = getColor(neighborData);
            prevFaceColors[f] = faceColors[f];
            faceColors[f] = neighborColor;
            timePainted[f] = millis();
          }
        }
      }
    }

    //now that I've maybe changed color, do I become a brush?
    if (faceColors[0] > 0) { //face 0 has a real color
      byte brushCheck = faceColors[0];//save that color as the reference
      byte brushCheckCount = 0;

      FOREACH_FACE(f) {//run through the faces and check for that color
        if (faceColors[f] == brushCheck) {//is this face the correct color
          brushCheckCount++;
        }
      }

      if (brushCheckCount == 6) {//hey, we got 6 hits! become brush!
        isBrush = true;
        timeBecamePaintBrush = millis();
      }
    }
  }

  //listen for button presses
  if (buttonSingleClicked()) {//create or recolor brushes
    if (isBrush) {//change to next brush color
      timeBecamePaintBrush = millis();
      byte nextColor = (faceColors[0] % 4) + 1;//determine the next color
      FOREACH_FACE(f) { //paint all faces that color
        faceColors[f] = nextColor;
      }
    } else {
      if (isBlank()) {//will only become a brush if blank
        isBrush = true;
        timeBecamePaintBrush = millis();
        byte randomColor = random(3) + 1;//just choose a random color
        FOREACH_FACE(f) { //paint all faces that color
          faceColors[f] = randomColor;
        }
      }
    }
  }

  if (buttonLongPressed()) {//reset the blink to a blank canvas
    isBrush = false;
    FOREACH_FACE(f) {
      prevFaceColors[f] = 0;
      faceColors[f] = 0;
      timePainted[f] = 0;
    }
  }

  if (buttonMultiClicked()) {//if long-pressed, begin field wiping
    if (buttonClickCount() == 3) {
      wipeState = WIPING;
    }
  }

  FOREACH_FACE(f) {//check around for anyone in WIPING
    if (!isValueReceivedOnFaceExpired(f)) {//neighbor!
      if (getWipeState(getLastValueReceivedOnFace(f)) == WIPING) {
        wipeState = WIPING;
      }
    }
  }
}

void wipingLoop() {
  bool canResolve = true;//we default to this, but revert it in the loop below if we need to

  FOREACH_FACE(f) {//check around for anyone still INERT
    if (!isValueReceivedOnFaceExpired(f)) {//neighbor!
      if (getWipeState(getLastValueReceivedOnFace(f)) == INERT) {
        canResolve = false;
      }
    }
  }

  if (canResolve) {
    wipeState = RESOLVE;
    isBrush = false;
    FOREACH_FACE(f) {
      faceColors[f] = 0;
    }
  }
}

void resolveLoop() {
  bool canInert = true;//we default to this, but revert it in the loop below if we need to

  FOREACH_FACE(f) {//check around for anyone still WIPING
    if (!isValueReceivedOnFaceExpired(f)) {//neighbor!
      if (getWipeState(getLastValueReceivedOnFace(f)) == WIPING) {
        canInert = false;
      }
    }
  }

  if (canInert) {
    wipeState = INERT;
  }
}

void canvasDisplay() {
  FOREACH_FACE(f) {
    if (faceColors[f] > 0) {//colored faces are at full brightness

      // animate the painting process
      long timeSincePainted = millis() - timePainted[f];
      if(timeSincePainted > PAINT_DURATION) timeSincePainted = PAINT_DURATION;

      if(prevFaceColors[f] == 0) {
        // fade up the color when recently painted from blank canvas
        byte bri = map(timeSincePainted, 0, PAINT_DURATION, 40, 255);
        byte sat = map(timeSincePainted, 0, PAINT_DURATION, 0, 255);
        setColorOnFace(makeColorHSB(colorHues[faceColors[f]], sat, bri), f);      
      }
      else {
        // blend into the new color from previous color
        byte startHue = colorHues[prevFaceColors[f]];
        byte endHue = colorHues[faceColors[f]];
        byte hue;
        if(startHue<endHue)
          hue = map(timeSincePainted, 0, PAINT_DURATION, startHue, endHue);
        else
          hue = map(timeSincePainted, 0, PAINT_DURATION, startHue, endHue + 255);
        
        // possibly fade to white in the middle, but maybe not necessary...
        byte sat;
        if(timeSincePainted<PAINT_DURATION/2)
          sat = 255 - map(timeSincePainted, 0, PAINT_DURATION/2, 0, 255);
        else
          sat = map(timeSincePainted, PAINT_DURATION/2, PAINT_DURATION, 0, 255);
        setColorOnFace(makeColorHSB(hue, sat, 255), f);
      }
      
    } else {//blank faces are at 0 brightness
      setColorOnFace(makeColorHSB(0, 0, 40), f);
    }
  }
}

void brushDisplay() {
  //  Option 1: simple pulse for brush
  //  setColor(makeColorHSB(colorHues[faceColors[0]], 255, 63 + (3 * sin8_C(millis()/15))/4));

  //  wipe the face with the color chasing it's tail
  //  slight hue offset to feel painterly
  if (brushCycle.isExpired()) {
    brushFace = (brushFace + 1) % 6;
    brushCycle.set(BRUSH_CYCLE_DURATION);
  }
  long timeSinceBrush = millis() - timeBecamePaintBrush;
  long duration;
  FOREACH_FACE(f) {
    duration = 1000*((brushFace + f) % 6);
    byte bri = map(timeSinceBrush, 0, duration, 0, 255);
    setColorOnFace(makeColorHSB((colorHues[faceColors[f]] + 2 * f) % 255, 255, bri), (brushFace + f) % 6);
  }
}

byte getBrush(byte data) {
  return (data >> 5);//only the first bit
}

byte getWipeState(byte data) {
  return ((data >> 3) & 3);//the second and third bit
}

byte getColor(byte data) {//the last 3 bits
  return (data & 7);
}

bool isBlank () {
  bool blankBool = true;

  FOREACH_FACE(f) {
    if (faceColors[f] > 0) {
      blankBool = false;
    }
  }

  return blankBool;
}

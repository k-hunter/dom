// Bit value for the keyboard events
const kKeyDown  = 0x01;
const kKeyPress = 0x02;
const kKeyUp    = 0x04;

// Pair the event name to its bit value
const kEventCode = {
  'keydown'   : kKeyDown,
  'keypress'  : kKeyPress,
  'keyup'     : kKeyUp
};

// Holding the current test case's infomation:
var gCurrentTest;

// The used input method of this test
var gInputMethod;

// A timer to wait for events
var gTimeoutID;
const kTimePeriod = 500;
const waitingTimes = 5;

function frameScript()
{
  function handler(e) {
    sendAsyncMessage("forwardevent", { type: e.type, key: e.key });
  }
  addEventListener('keydown', handler);
  addEventListener('keypress', handler);
  addEventListener('keyup', handler);
}

function loadTestFrame(useRemote) {
  let iframe = document.createElement('iframe');
  iframe.src = 'bug1110030_embedded.html?result=';
  iframe.setAttribute('mozbrowser', true);
  // iframe.setAttribute('remote', useRemote ? 'true' : 'false');

  iframe.addEventListener("mozbrowserloadend", function onloadend() {
    iframe.removeEventListener("mozbrowserloadend", onloadend);
    iframe.focus();
    var mm = SpecialPowers.getBrowserFrameMessageManager(iframe);
    mm.addMessageListener("forwardevent", function(msg) {
      inputtextEventReceiver(msg.json);
    });
    mm.loadFrameScript("data:,(" + frameScript.toString() + ")();", false);
    return;
  });

  document.body.appendChild(iframe);
}

function eventToCode (type)
{
  return kEventCode[type];
}

function hardwareEventReceiver(evt)
{
  if (!gCurrentTest) {
    return;
  }
  gCurrentTest.hardwareinput.receivedEvents |= eventToCode(evt.type);
  gCurrentTest.hardwareinput.receivedKeys += evt.key;
}

function inputtextEventReceiver(evt)
{
  if (!gCurrentTest) {
    return;
  }
  gCurrentTest.inputtext.receivedEvents |= eventToCode(evt.type);
  gCurrentTest.inputtext.receivedKeys += evt.key;
}


function verifyResults(callback)
{
  dump("@@@@@ verifyResults\n");
  if (!callback) {
    errorHandler('No callback set in verifyResults');
    return;
  }

  if (this.calledTimes === undefined) {
    this.calledTimes = 0;
  } else {
    this.calledTimes++;
  }

  dump("times: " + this.calledTimes + "\n");
  dump("@@@ rcv hw keys: " + gCurrentTest.hardwareinput.receivedKeys + "\n");
  dump("@@@ rcv txt keys: " + gCurrentTest.inputtext.receivedKeys + "\n");
  dump("@@@ exp hw keys: " + gCurrentTest.hardwareinput.expectedKeys + "\n");
  dump("@@@ exp txt keys: " + gCurrentTest.inputtext.expectedKeys + "\n");

  if (gTimeoutID) {
    window.clearTimeout(gTimeoutID);
  }

  // It may need more time to collect events
  // if the received events are different from the expected one
  if ((gCurrentTest.hardwareinput.receivedEvents != gCurrentTest.hardwareinput.expectedEvents) ||
      (gCurrentTest.inputtext.receivedEvents != gCurrentTest.inputtext.expectedEvents)) {

    // Stop the tests when we spent too long time to collect events
    if (this.calledTimes > waitingTimes) {
      errorHandler('Can not receive expected events in time!');
      return;
    }

    // Give some time to collect our expected events by calling
    // this function after a while
    gTimeoutID = window.setTimeout(function() {
      verifyResults(callback);
    }, kTimePeriod);

    return;
  }

  // Otherwise, if received events are same as the expected events,
  // then we can verify the received keys with the expected keys
  if ((gCurrentTest.hardwareinput.receivedKeys == gCurrentTest.hardwareinput.expectedKeys) &&
      (gCurrentTest.inputtext.receivedKeys == gCurrentTest.inputtext.expectedKeys)) {
    gCurrentTest.pass = true;
    this.calledTimes = 0;
    callback();
    return;
  }

  // Stop the test cases if the received keys are different from
  // the expected keys
  errorHandler('expected hardwareinput keys: ' + gCurrentTest.hardwareinput.expectedKeys +
               ', received hardwareinput keys: ' + gCurrentTest.hardwareinput.receivedKeys +
               ', expected inputtext keys: ' + gCurrentTest.inputtext.expectedKeys +
               ', received inputtext keys: ' + gCurrentTest.inputtext.receivedKeys);
  return;
}

function fireEvents()
{
  let key = gCurrentTest.key;
  synthesizeNativeKey(KEYBOARD_LAYOUT_EN_US, guessNativeKeyCode(key), {},
                      key, key);
}

function guessNativeKeyCode(key)
{
  let NativeCodeName = 'MAC_VK_ANSI_';
  if (/^[A-Z]$/.test(key)) {
    NativeCodeName += key;
  } else if (/^[a-z]$/.test(key)) {
    NativeCodeName += key.toUpperCase();
  } else if (/^[0-9]$/.test(key)) {
    NativeCodeName += key.toString();
  } else if (key == '=') {
    NativeCodeName += 'Equal';
  } else if (key == '+') {
    NativeCodeName += 'KeypadPlus';
  } else if (key == '-') {
    NativeCodeName += 'Minus';
  } else if (key == '*') {
    NativeCodeName += 'KeypadMultiply';
  } else if (key == '/') {
    NativeCodeName += 'Slash';
  } else if (key == '\\') {
    NativeCodeName += 'Backslash';
  } else if (key == ',') {
    NativeCodeName += 'Comma';
  } else if (key == ';') {
    NativeCodeName += 'Semicolon';
  } else {
    return 0;
  }

  return eval(NativeCodeName);
}

function addKeyEventListeners (eventTarget, handler)
{
  Object.keys(kEventCode).forEach(function(type) {
    eventTarget.addEventListener(type, handler);
  });
}

function errorHandler(msg)
{
  // Clear the test cases
  if (gTests) {
    gTests = [];
  }

  ok(false, msg);

  inputmethod_cleanup();
}

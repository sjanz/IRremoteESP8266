// Copyright 2017 Jonny Graham, David Conran
#include "ir_Fujitsu.h"
#include <algorithm>
#ifndef ARDUINO
#include <string>
#endif
#include "IRsend.h"
#include "IRutils.h"

// Fujitsu A/C support added by Jonny Graham & David Conran

// Equipment it seems compatible with:
//  * Fujitsu ASYG30LFCA with remote AR-RAH2E
//  * Fujitsu AST9RSGCW with remote AR-DB1
//  * Fujitsu AR-RAE1E remote.
//  * <Add models (A/C & remotes) you've gotten it working with here>

// Ref:
// These values are based on averages of measurements
const uint16_t kFujitsuAcHdrMark = 3324;
const uint16_t kFujitsuAcHdrSpace = 1574;
const uint16_t kFujitsuAcBitMark = 448;
const uint16_t kFujitsuAcOneSpace = 1182;
const uint16_t kFujitsuAcZeroSpace = 390;
const uint16_t kFujitsuAcMinGap = 8100;

#if SEND_FUJITSU_AC
// Send a Fujitsu A/C message.
//
// Args:
//   data: An array of bytes containing the IR command.
//   nbytes: Nr. of bytes of data in the array. Typically one of:
//           kFujitsuAcStateLength
//           kFujitsuAcStateLength - 1
//           kFujitsuAcStateLengthShort
//           kFujitsuAcStateLengthShort - 1
//   repeat: Nr. of times the message is to be repeated.
//          (Default = kFujitsuAcMinRepeat).
//
// Status: BETA / Appears to be working.
//
void IRsend::sendFujitsuAC(unsigned char data[], uint16_t nbytes,
                           uint16_t repeat) {
  sendGeneric(kFujitsuAcHdrMark, kFujitsuAcHdrSpace, kFujitsuAcBitMark,
              kFujitsuAcOneSpace, kFujitsuAcBitMark, kFujitsuAcZeroSpace,
              kFujitsuAcBitMark, kFujitsuAcMinGap, data, nbytes, 38, false,
              repeat, 50);
}
#endif  // SEND_FUJITSU_AC

// Code to emulate Fujitsu A/C IR remote control unit.

// Initialise the object.
IRFujitsuAC::IRFujitsuAC(const uint16_t pin,
                         const fujitsu_ac_remote_model_t model)
    : _irsend(pin) {
  setModel(model);
  stateReset();
}

void IRFujitsuAC::setModel(const fujitsu_ac_remote_model_t model) {
  _model = model;
  switch (model) {
    case ARDB1:
      _state_length = kFujitsuAcStateLength - 1;
      _state_length_short = kFujitsuAcStateLengthShort - 1;
      break;
    default:
      _state_length = kFujitsuAcStateLength;
      _state_length_short = kFujitsuAcStateLengthShort;
  }
}

fujitsu_ac_remote_model_t IRFujitsuAC::getModel(void) { return _model; }

// Reset the state of the remote to a known good state/sequence.
void IRFujitsuAC::stateReset(void) {
  _temp = 24;
  _fanSpeed = kFujitsuAcFanHigh;
  _mode = kFujitsuAcModeCool;
  _swingMode = kFujitsuAcSwingBoth;
  _cmd = kFujitsuAcCmdTurnOn;
  buildState();
}

// Configure the pin for output.
void IRFujitsuAC::begin(void) { _irsend.begin(); }

#if SEND_FUJITSU_AC
// Send the current desired state to the IR LED.
void IRFujitsuAC::send(const uint16_t repeat) {
  getRaw();
  _irsend.sendFujitsuAC(remote_state, getStateLength(), repeat);
}
#endif  // SEND_FUJITSU_AC

void IRFujitsuAC::buildState(void) {
  remote_state[0] = 0x14;
  remote_state[1] = 0x63;
  remote_state[2] = 0x00;
  remote_state[3] = 0x10;
  remote_state[4] = 0x10;
  bool fullCmd = false;
  switch (_cmd) {
    case kFujitsuAcCmdTurnOff:
      remote_state[5] = 0x02;
      break;
    case kFujitsuAcCmdStepHoriz:
      remote_state[5] = 0x79;
      break;
    case kFujitsuAcCmdStepVert:
      remote_state[5] = 0x6C;
      break;
    default:
      switch (_model) {
        case ARRAH2E:
          remote_state[5] = 0xFE;
          break;
        case ARDB1:
          remote_state[5] = 0xFC;
          break;
      }
      fullCmd = true;
      break;
  }
  if (fullCmd) {  // long codes
    uint8_t tempByte = _temp - kFujitsuAcMinTemp;
    // Nr. of bytes in the message after this byte.
    remote_state[6] = _state_length - 7;

    remote_state[7] = 0x30;
    remote_state[8] = (_cmd == kFujitsuAcCmdTurnOn) | (tempByte << 4);
    remote_state[9] = _mode | 0 << 4;  // timer off
    remote_state[10] = _fanSpeed | _swingMode << 4;
    remote_state[11] = 0;  // timerOff values
    remote_state[12] = 0;  // timerOff/On values
    remote_state[13] = 0;  // timerOn values
    if (_model == ARRAH2E)
      remote_state[14] = 0x20;
    else
      remote_state[14] = 0x00;

    uint8_t checksum = 0;
    uint8_t checksum_complement = 0;
    if (_model == ARRAH2E) {
      checksum = sumBytes(remote_state + _state_length_short,
                          _state_length - _state_length_short - 1);
    } else if (_model == ARDB1) {
      checksum = sumBytes(remote_state, _state_length - 1);
      checksum_complement = 0x9B;
    }
    // and negate the checksum and store it in the last byte.
    remote_state[_state_length - 1] = checksum_complement - checksum;
  } else {  // short codes
    if (_model == ARRAH2E)
      // The last byte is the inverse of penultimate byte
      remote_state[_state_length_short - 1] =
          ~remote_state[_state_length_short - 2];
    // Zero the rest of the state.
    for (uint8_t i = _state_length_short; i < kFujitsuAcStateLength; i++)
      remote_state[i] = 0;
  }
}

uint8_t IRFujitsuAC::getStateLength(void) {
  buildState();  // Force an update of the internal state.
  if ((_model == ARRAH2E && remote_state[5] != 0xFE) ||
      (_model == ARDB1 && remote_state[5] != 0xFC))
    return _state_length_short;
  else
    return _state_length;
}

// Return a pointer to the internal state date of the remote.
uint8_t* IRFujitsuAC::getRaw(void) {
  buildState();
  return remote_state;
}

void IRFujitsuAC::buildFromState(const uint16_t length) {
  switch (length) {
    case kFujitsuAcStateLength - 1:
    case kFujitsuAcStateLengthShort - 1:
      setModel(ARDB1);
      break;
    default:
      setModel(ARRAH2E);
  }
  switch (remote_state[6]) {
    case 8:
      setModel(ARDB1);
      break;
    case 9:
      setModel(ARRAH2E);
      break;
  }
  setTemp((remote_state[8] >> 4) + kFujitsuAcMinTemp);
  if (remote_state[8] & 0x1)
    setCmd(kFujitsuAcCmdTurnOn);
  else
    setCmd(kFujitsuAcCmdStayOn);
  setMode(remote_state[9] & 0b111);
  setFanSpeed(remote_state[10] & 0b111);
  setSwing(remote_state[10] >> 4);
  switch (remote_state[5]) {
    case kFujitsuAcCmdTurnOff:
    case kFujitsuAcCmdStepHoriz:
    case kFujitsuAcCmdStepVert:
      setCmd(remote_state[5]);
      break;
  }
}

bool IRFujitsuAC::setRaw(const uint8_t newState[], const uint16_t length) {
  if (length > kFujitsuAcStateLength) return false;
  for (uint16_t i = 0; i < kFujitsuAcStateLength; i++) {
    if (i < length)
      remote_state[i] = newState[i];
    else
      remote_state[i] = 0;
  }
  buildFromState(length);
  return true;
}

// Set the requested power state of the A/C to off.
void IRFujitsuAC::off(void) { _cmd = kFujitsuAcCmdTurnOff; }

void IRFujitsuAC::stepHoriz(void) {
  switch (_model) {
    case ARDB1:
      break;  // This remote doesn't have a horizontal option.
    default:
      _cmd = kFujitsuAcCmdStepHoriz;
  }
}

void IRFujitsuAC::stepVert(void) { _cmd = kFujitsuAcCmdStepVert; }

// Set the requested command of the A/C.
void IRFujitsuAC::setCmd(const uint8_t cmd) {
  switch (cmd) {
    case kFujitsuAcCmdTurnOff:
    case kFujitsuAcCmdTurnOn:
    case kFujitsuAcCmdStayOn:
    case kFujitsuAcCmdStepVert:
      _cmd = cmd;
      break;
    case kFujitsuAcCmdStepHoriz:
      if (_model != ARDB1)  // AR-DB1 remote doesn't have step horizontal.
        _cmd = cmd;
      // FALLTHRU
    default:
      _cmd = kFujitsuAcCmdStayOn;
      break;
  }
}

uint8_t IRFujitsuAC::getCmd(void) { return _cmd; }

bool IRFujitsuAC::getPower(void) { return _cmd != kFujitsuAcCmdTurnOff; }

// Set the temp. in deg C
void IRFujitsuAC::setTemp(const uint8_t temp) {
  _temp = std::max((uint8_t)kFujitsuAcMinTemp, temp);
  _temp = std::min((uint8_t)kFujitsuAcMaxTemp, _temp);
}

uint8_t IRFujitsuAC::getTemp(void) { return _temp; }

// Set the speed of the fan
void IRFujitsuAC::setFanSpeed(const uint8_t fanSpeed) {
  if (fanSpeed > kFujitsuAcFanQuiet)
    _fanSpeed = kFujitsuAcFanHigh;  // Set the fan to maximum if out of range.
  else
    _fanSpeed = fanSpeed;
}
uint8_t IRFujitsuAC::getFanSpeed(void) { return _fanSpeed; }

// Set the requested climate operation mode of the a/c unit.
void IRFujitsuAC::setMode(const uint8_t mode) {
  if (mode > kFujitsuAcModeHeat)
    _mode = kFujitsuAcModeHeat;  // Set the mode to maximum if out of range.
  else
    _mode = mode;
}

uint8_t IRFujitsuAC::getMode(void) { return _mode; }

// Set the requested swing operation mode of the a/c unit.
void IRFujitsuAC::setSwing(const uint8_t swingMode) {
  _swingMode = swingMode;
  switch (_model) {
    case ARDB1:
      // Set the mode to max if out of range
      if (swingMode > kFujitsuAcSwingVert) _swingMode = kFujitsuAcSwingVert;
      break;
    case ARRAH2E:
    default:
      // Set the mode to max if out of range
      if (swingMode > kFujitsuAcSwingBoth) _swingMode = kFujitsuAcSwingBoth;
  }
}

uint8_t IRFujitsuAC::getSwing(void) { return _swingMode; }

bool IRFujitsuAC::validChecksum(uint8_t state[], const uint16_t length) {
  uint8_t sum = 0;
  uint8_t sum_complement = 0;
  uint8_t checksum = state[length - 1];
  switch (length) {
    case kFujitsuAcStateLengthShort:  // ARRAH2E
      return state[length - 1] == (uint8_t)~state[length - 2];
    case kFujitsuAcStateLength - 1:  // ARDB1
      sum = sumBytes(state, length - 1);
      sum_complement = 0x9B;
      break;
    case kFujitsuAcStateLength:  // ARRAH2E
      sum = sumBytes(state + kFujitsuAcStateLengthShort,
                     length - 1 - kFujitsuAcStateLengthShort);
      break;
    default:        // Includes ARDB1 short.
      return true;  // Assume the checksum is valid for other lengths.
  }
  return checksum == (uint8_t)(sum_complement - sum);  // Does it match?
}

// Convert a standard A/C mode into its native mode.
uint8_t IRFujitsuAC::convertMode(const stdAc::opmode_t mode) {
  switch (mode) {
    case stdAc::opmode_t::kCool:
      return kFujitsuAcModeCool;
    case stdAc::opmode_t::kHeat:
      return kFujitsuAcModeHeat;
    case stdAc::opmode_t::kDry:
      return kFujitsuAcModeDry;
    case stdAc::opmode_t::kFan:
      return kFujitsuAcModeFan;
    default:
      return kFujitsuAcModeAuto;
  }
}

// Convert a standard A/C Fan speed into its native fan speed.
uint8_t IRFujitsuAC::convertFan(stdAc::fanspeed_t speed) {
  switch (speed) {
    case stdAc::fanspeed_t::kMin:
      return kFujitsuAcFanQuiet;
    case stdAc::fanspeed_t::kLow:
      return kFujitsuAcFanLow;
    case stdAc::fanspeed_t::kMedium:
      return kFujitsuAcFanMed;
    case stdAc::fanspeed_t::kHigh:
    case stdAc::fanspeed_t::kMax:
      return kFujitsuAcFanHigh;
    default:
      return kFujitsuAcFanAuto;
  }
}

// Convert a native mode to it's common equivalent.
stdAc::opmode_t IRFujitsuAC::toCommonMode(const uint8_t mode) {
  switch (mode) {
    case kFujitsuAcModeCool: return stdAc::opmode_t::kCool;
    case kFujitsuAcModeHeat: return stdAc::opmode_t::kHeat;
    case kFujitsuAcModeDry: return stdAc::opmode_t::kDry;
    case kFujitsuAcModeFan: return stdAc::opmode_t::kFan;
    default: return stdAc::opmode_t::kAuto;
  }
}

// Convert a native fan speed to it's common equivalent.
stdAc::fanspeed_t IRFujitsuAC::toCommonFanSpeed(const uint8_t speed) {
  switch (speed) {
    case kFujitsuAcFanHigh: return stdAc::fanspeed_t::kMax;
    case kFujitsuAcFanMed: return stdAc::fanspeed_t::kMedium;
    case kFujitsuAcFanLow: return stdAc::fanspeed_t::kLow;
    case kFujitsuAcFanQuiet: return stdAc::fanspeed_t::kMin;
    default: return stdAc::fanspeed_t::kAuto;
  }
}

// Convert the A/C state to it's common equivalent.
stdAc::state_t IRFujitsuAC::toCommon(void) {
  stdAc::state_t result;
  result.protocol = decode_type_t::FUJITSU_AC;
  result.model = this->getModel();
  result.power = this->getPower();
  result.mode = this->toCommonMode(this->getMode());
  result.celsius = true;
  result.degrees = this->getTemp();
  result.fanspeed = this->toCommonFanSpeed(this->getFanSpeed());
  uint8_t swing = this->getSwing();
  result.swingv = (swing & kFujitsuAcSwingVert) ? stdAc::swingv_t::kAuto :
                                                  stdAc::swingv_t::kOff;
  result.swingh = (swing & kFujitsuAcSwingHoriz) ? stdAc::swingh_t::kAuto :
                                                   stdAc::swingh_t::kOff;
  result.quiet = (this->getFanSpeed() == kFujitsuAcFanQuiet);
  // Not supported.
  result.turbo = false;
  result.light = false;
  result.filter = false;
  result.clean = false;
  result.econo = false;
  result.beep = false;
  result.sleep = -1;
  result.clock = -1;
  return result;
}

// Convert the internal state into a human readable string.
#ifdef ARDUINO
String IRFujitsuAC::toString(void) {
  String result = "";
#else
std::string IRFujitsuAC::toString(void) {
  std::string result = "";
#endif  // ARDUINO
  result.reserve(100);  // Reserve some heap for the string to reduce fragging.
  result += F("Power: ");
  if (getPower())
    result += F("On");
  else
    result += F("Off");
  result += F(", Mode: ");
  result += uint64ToString(getMode());
  switch (getMode()) {
    case kFujitsuAcModeAuto:
      result += F(" (AUTO)");
      break;
    case kFujitsuAcModeCool:
      result += F(" (COOL)");
      break;
    case kFujitsuAcModeHeat:
      result += F(" (HEAT)");
      break;
    case kFujitsuAcModeDry:
      result += F(" (DRY)");
      break;
    case kFujitsuAcModeFan:
      result += F(" (FAN)");
      break;
    default:
      result += F(" (UNKNOWN)");
  }
  result += F(", Temp: ");
  result += uint64ToString(getTemp());
  result += F("C, Fan: ");
  result += uint64ToString(getFanSpeed());
  switch (getFanSpeed()) {
    case kFujitsuAcFanAuto:
      result += F(" (AUTO)");
      break;
    case kFujitsuAcFanHigh:
      result += F(" (HIGH)");
      break;
    case kFujitsuAcFanMed:
      result += F(" (MED)");
      break;
    case kFujitsuAcFanLow:
      result += F(" (LOW)");
      break;
    case kFujitsuAcFanQuiet:
      result += F(" (QUIET)");
      break;
  }
  result += F(", Swing: ");
  switch (getSwing()) {
    case kFujitsuAcSwingOff:
      result += F("Off");
      break;
    case kFujitsuAcSwingVert:
      result += F("Vert");
      break;
    case kFujitsuAcSwingHoriz:
      result += F("Horiz");
      break;
    case kFujitsuAcSwingBoth:
      result += F("Vert + Horiz");
      break;
    default:
      result += F("UNKNOWN");
  }
  result += F(", Command: ");
  switch (getCmd()) {
    case kFujitsuAcCmdStepHoriz:
      result += F("Step vane horizontally");
      break;
    case kFujitsuAcCmdStepVert:
      result += F("Step vane vertically");
      break;
    default:
      result += F("N/A");
  }
  return result;
}

#if DECODE_FUJITSU_AC
// Decode a Fujitsu AC IR message if possible.
// Places successful decode information in the results pointer.
// Args:
//   results: Ptr to the data to decode and where to store the decode result.
//   nbits:   The number of data bits to expect. Typically kFujitsuAcBits.
//   strict:  Flag to indicate if we strictly adhere to the specification.
// Returns:
//   boolean: True if it can decode it, false if it can't.
//
// Status:  ALPHA / Untested.
//
// Ref:
//
bool IRrecv::decodeFujitsuAC(decode_results* results, uint16_t nbits,
                             bool strict) {
  uint16_t offset = kStartOffset;
  uint16_t dataBitsSoFar = 0;

  // Have we got enough data to successfully decode?
  if (results->rawlen < (2 * kFujitsuAcMinBits) + kHeader + kFooter - 1)
    return false;  // Can't possibly be a valid message.

  // Compliance
  if (strict) {
    switch (nbits) {
      case kFujitsuAcBits:
      case kFujitsuAcBits - 8:
      case kFujitsuAcMinBits:
      case kFujitsuAcMinBits + 8:
        break;
      default:
        return false;  // Must be called with the correct nr. of bits.
    }
  }

  // Header
  if (!matchMark(results->rawbuf[offset++], kFujitsuAcHdrMark)) return false;
  if (!matchSpace(results->rawbuf[offset++], kFujitsuAcHdrSpace)) return false;

  // Data (Fixed signature)
  match_result_t data_result =
      matchData(&(results->rawbuf[offset]), kFujitsuAcMinBits - 8,
                kFujitsuAcBitMark, kFujitsuAcOneSpace, kFujitsuAcBitMark,
                kFujitsuAcZeroSpace, kTolerance, kMarkExcess, false);
  if (data_result.success == false) return false;      // Fail
  if (data_result.data != 0x1010006314) return false;  // Signature failed.
  dataBitsSoFar += kFujitsuAcMinBits - 8;
  offset += data_result.used;
  results->state[0] = 0x14;
  results->state[1] = 0x63;
  results->state[2] = 0x00;
  results->state[3] = 0x10;
  results->state[4] = 0x10;

  // Keep reading bytes until we either run out of message or state to fill.
  for (uint16_t i = 5;
       offset <= results->rawlen - 16 && i < kFujitsuAcStateLength;
       i++, dataBitsSoFar += 8, offset += data_result.used) {
    data_result = matchData(
        &(results->rawbuf[offset]), 8, kFujitsuAcBitMark, kFujitsuAcOneSpace,
        kFujitsuAcBitMark, kFujitsuAcZeroSpace, kTolerance, kMarkExcess, false);
    if (data_result.success == false) break;  // Fail
    results->state[i] = data_result.data;
  }

  // Footer
  if (offset > results->rawlen ||
      !matchMark(results->rawbuf[offset++], kFujitsuAcBitMark))
    return false;
  // The space is optional if we are out of capture.
  if (offset < results->rawlen &&
      !matchAtLeast(results->rawbuf[offset], kFujitsuAcMinGap))
    return false;

  // Compliance
  if (strict) {
    if (dataBitsSoFar != nbits) return false;
  }

  results->decode_type = FUJITSU_AC;
  results->bits = dataBitsSoFar;

  // Compliance
  switch (dataBitsSoFar) {
    case kFujitsuAcMinBits:
      // Check if this values indicate that this should have been a long state
      // message.
      if (results->state[5] == 0xFC) return false;
      return true;  // Success
    case kFujitsuAcMinBits + 8:
      // Check if this values indicate that this should have been a long state
      // message.
      if (results->state[5] == 0xFE) return false;
      // The last byte needs to be the inverse of the penultimate byte.
      if (results->state[5] != (uint8_t)~results->state[6]) return false;
      return true;  // Success
    case kFujitsuAcBits - 8:
      // Long messages of this size require this byte be correct.
      if (results->state[5] != 0xFC) return false;
      break;
    case kFujitsuAcBits:
      // Long messages of this size require this byte be correct.
      if (results->state[5] != 0xFE) return false;
      break;
    default:
      return false;  // Unexpected size.
  }
  if (!IRFujitsuAC::validChecksum(results->state, dataBitsSoFar / 8))
    return false;

  // Success
  return true;  // All good.
}
#endif  // DECODE_FUJITSU_AC

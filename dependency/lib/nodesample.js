const ffi = require('ffi-napi');
const ref = require('ref-napi');

// Define the string type
const charPtr = ref.refType('string');

// Load the DLL
const iKnowULib = ffi.Library('iKnowULib.dll', {
  'startScanAndGetFingerID': [ 'string', [ 'string', 'string', 'string' ] ],
  'startScanAndRegisterFingerID': [ 'string', [ 'string', 'string', 'string', 'string' ] ]
});

// Function to start scan and get finger ID
function startScanAndGetFingerID(ipAddress, port, appId) {
  return iKnowULib.startScanAndGetFingerID(ipAddress, port, appId);
}

// Function to start scan and register finger ID
function startScanAndRegisterFingerID(ipAddress, port, appId, returnId) {
  return iKnowULib.startScanAndRegisterFingerID(ipAddress, port, appId, returnId);
}

// Example usage
const ipAddress = '82.197.71.75';
const port = '8899';
const appId = 'MyAppIdNode';
const returnId = 'HelloNode';

console.log('Start scanning...');
const fingerId = startScanAndGetFingerID(ipAddress, port, appId);
console.log('Finger ID:', fingerId);

// const registerResult = startScanAndRegisterFingerID(ipAddress, port, appId, returnId);
// console.log('Register Result:', registerResult);

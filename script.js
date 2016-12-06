var Bleacon = require('bleacon');
var execSync    = require('child_process').execSync;

// settings for Arduino iBeacon
var arduino_uuid          = "74278bdab64445208f0c720eaf059935";
var arduino_major_version = 4096; // 0x1000
var too_hot               = 257;
var too_cold              = 258;

// settings for eq3 Radiator thermostat
// gatttool is part of the bluez package for Linux
var boost_on              = 'gatttool -b "00:1A:22:08:99:8C" --char-write-req  -a "0x0411" -n "4501" || true'
var boost_off             = 'gatttool -b "00:1A:22:08:99:8C" --char-write-req  -a "0x0411" -n "4500" || true'

var room_status, last_observed;

function toggle_boost(room_status) {
    if(room_status == too_cold) {
      console.log("Room too cold: turning on boost")
      execSync(boost_on, { timeout: 5000 });
    } else {
      console.log("Room too warm: turning off boost")
      execSync(boost_off, { timeout: 5000 });
  }
  console.log("Done");
}

console.log("Starting discovery of iBeacon");

Bleacon.on('discover', function(bleacon) {
  console.log(JSON.stringify(bleacon));
  room_status = bleacon["minor"];
});

// Poll for changes in status every 15s
// Trigger boost mode on EQ3 Radiator thermostat if needed
setInterval(function() {
  // respond to each request
  while (room_status != last_observed) {
    try {
      // sometimes the Bluetooth dongle is contended and throws errors
      // silently fail and try again later
      toggle_boost(room_status);
    } finally {};
    last_observed = room_status;

    // lock down device - see below
    // also, scanning needs to be restarted after external process has 
    // accessed bluetooth device for some reason
    Bleacon.startScanning(arduino_uuid, arduino_major_version);
  }
}, 15000);

// Scan is locked down to a particular ID to ensure things can't be triggered by 3rd party
Bleacon.startScanning(arduino_uuid, arduino_major_version);

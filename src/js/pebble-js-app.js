var Btoa = function(string){
  var b64chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  return string.replace(/[\s\S]{1,3}/g, function(ccc) {
        var padlen = [0, 2, 1][ccc.length % 3],
        ord = ccc.charCodeAt(0) << 16
            | ((ccc.length > 1 ? ccc.charCodeAt(1) : 0) << 8)
            | ((ccc.length > 2 ? ccc.charCodeAt(2) : 0)),
        chars = [
            b64chars.charAt( ord >>> 18),
            b64chars.charAt((ord >>> 12) & 63),
            padlen >= 2 ? '=' : b64chars.charAt((ord >>> 6) & 63),
            padlen >= 1 ? '=' : b64chars.charAt(ord & 63)
        ];
        return chars.join('');
    });
};

var watch = {
  platform: "aplite"
};
if(Pebble.getActiveWatchInfo) {
  watch = Pebble.getActiveWatchInfo();
} else {
  console.log ("Could not get watch info");
}

console.log (watch.platform);

var sendNotifications = function (i, notifications){
  console.log (i + " " + notifications.length);
  var notification = notifications[i];
  notification = {
    "ID_KEY": (notification._id), 	
    "TIMESTAMP_KEY": Math.floor(+(new Date(notification.sendTime))/1000) - (watch.platform === "aplite"? (new Date()).getTimezoneOffset()*60:0),
    "TITLE_KEY": notification.title,
    "MESSAGE_KEY": notification.message
  };
  console.log (JSON.stringify(notification));
  Pebble.sendAppMessage(notification,
        function(e) {
          console.log ("Notification sent successfully");
          if (i < notifications.length - 1){
            sendNotifications(++i, notifications);
          }
        },
        function(e) {
          console.log ("Notification sending failed");
  });
};

var username = localStorage.getItem("USERNAME_KEY") || "test";
var BASILISK_KEY = localStorage.getItem("BASILISK_KEY") || "MVNOVlYwSUdROTBFTVNIOVNQQlBDNUxGOTpOamR5V3oxTmFScWhhcG02Vzd3ak9NdllPR0s1SUNBdHQ1ZHZrT2ZIWGR3";
// var BASILISK_KEY = localStorage.getItem("BASILISK_KEY")

var ws = null;

function socketStuff (){
  ws = new WebSocket('ws://162.244.25.137:2999');   
  ws.onopen = function(){
    console.log ("open");
    ws.send (BASILISK_KEY);
    setTimeout(function(){
      ws.send("notification request");
    }, 1000*60*60);
  };
  
  ws.onmessage = function (message) { 
    message = JSON.parse (message.data);
    if (message.event === "notification"){
        // Send to Pebble
      if (message.data && message.data.length){
        sendNotifications (0, message.data);
      }
    }
  };

  ws.onclose = function(){
    console.log ("Socket is closed");
    setTimeout(socketStuff, 1000*60*60);
  };
}
socketStuff();

function resetSocket (){
  console.log ("resetting" + ws.readyState);
  if (ws && ws.readyState === 1){
    ws.close();
    socketStuff();
  }
}

function millisUntilMidnight() {
    var midnight = new Date();
    midnight.setHours(24,0,0,0);
    return ( +midnight - (+new Date()));
}

setTimeout(function dailyReset(){
  resetSocket();
  setTimeout(dailyReset,(1000*60*60*24));
}, millisUntilMidnight());
// Function to send a message to the Pebble using AppMessage API
function sendMessage() {
// 	Pebble.sendAppMessage({"status": 0});
	
	// PRO TIP: If you are sending more than one message, or a complex set of messages, 
	// it is important that you setup an ackHandler and a nackHandler and call 
	// Pebble.sendAppMessage({ /* Message here */ }, ackHandler, nackHandler), which 
	// will designate the ackHandler and nackHandler that will be called upon the Pebble 
	// ack-ing or nack-ing the message you just sent. The specified nackHandler will 
	// also be called if your message send attempt times out.
}


// Called when JS is ready
Pebble.addEventListener("ready",
							function(e) {
							});
												
// Called when incoming message from the Pebble is received
Pebble.addEventListener("appmessage", function(e) {
  console.log("Received Status: " + JSON.stringify(e.payload));
  console.log (JSON.stringify({event:e.payload.EVENT_KEY, data:e.payload.ID_KEY}));
  
  if (e.payload.EVENT_KEY.toLowerCase() === "socketreset"){
    return resetSocket();
  }
  
  ws.send(JSON.stringify({event:e.payload.EVENT_KEY, data:e.payload.ID_KEY}));
});


Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL('https://pebblenotifyme.herokuapp.com/pebble/login');
});
Pebble.addEventListener('webviewclosed', function(e) {
  var returned = JSON.parse (e.response);
  console.log (typeof e.response);
console.log("Configuration window returned: " + (e.response));
  localStorage.setItem("USERNAME_KEY", returned.username);
  console.log (returned.id +":"+ returned.secret);
  localStorage.setItem("BASILISK_KEY", Btoa(returned.id +":"+ returned.secret));
  console.log (Btoa(returned.id +":"+ returned.secret));
});
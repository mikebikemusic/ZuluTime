Pebble.addEventListener("ready", function(e) {
	var d = new Date();
	var n = d.getTimezoneOffset();
	Pebble.sendAppMessage({ "TZOffset": n});
                                    		 });
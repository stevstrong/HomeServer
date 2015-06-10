function DoPHP()
{
	var httpRequest;
	var callBack;
	this.phpfile;
	this.postparam;
	this.postvalue;
	//this.callByReply;
	this.repl = "";
	this.RequestData = function (fReply) {
		callBack = fReply;
		if (window.XMLHttpRequest) { // Mozilla, Safari, ...
			httpRequest = new XMLHttpRequest();
		} else if (window.ActiveXObject) { // IE
			try { httpRequest = new ActiveXObject("Msxml2.XMLHTTP"); } 
			catch (e) {
				try { httpRequest = new ActiveXObject("Microsoft.XMLHTTP"); } catch (e) {}
			}
		}
		if (!httpRequest) {
			alert('Giving up: cannot create an XMLHTTP instance');
			return false;
		}
		httpRequest.onreadystatechange = alertContents;
		//httpRequest.open('POST', this.phpfile);
		httpRequest.open('POST', this.postparam + "=" + encodeURIComponent(this.postvalue));
		//httpRequest.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
		httpRequest.setRequestHeader('Content-Type', 'text/plain');
		//httpRequest.send(this.postparam + "=" + encodeURIComponent(this.postvalue));
		httpRequest.send();
	}
	function alertContents() {
		try {
			if (httpRequest.readyState === 4) {
				if (httpRequest.status === 200) {
					this.repl = httpRequest.responseText;
					//alert(this.repl);
					callBack(this.repl);
				} else { alert('There was a problem with the request: '+this.repl); }
			}
		} catch( e ) {
			alert("A2P: Caught Exception: " + e.message + ", file: '" + e.fileName + "', line: '" + e.lineNumber + "'");
		}
	}
}

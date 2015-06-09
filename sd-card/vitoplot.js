////////////////////////////////////////////////////////////////////////////////////////////////////////
function MakeDraw()
{
	const SPAN = 5;
	var canvas1,canvas2;
	var ctx1,ctx2;
	var dataURL,img;
	var snapit;

	this.id;
	this.data;
	this.labels;
	this.replot;
	this.replot;
	
	this.fSize = 10;
	this.lineWidthG = 1;
	this.textColor = 'rgba(100,100,100,1)';
	this.gridColor = 'rgba(0,0,0,0.1)'; 
	this.bgColor = 'rgba(255,255,255,1)';
	
	///////////////////////////////////////////////
	this.prepUI = function ()
	{
		canvas1 = document.getElementById(this.id);
		ctx1 = canvas1.getContext('2d');
		ctx1.font = (this.fSize+"px sans-serif");
		var eltern = canvas1.parentElement;
		// create second canvas
		var elem = "<canvas id='canvas_2' width="+(canvas1.width-300)+" height="+(canvas1.height-100)+
			" style='z-index: 2; position:absolute;left:30px;top:30px;border:1px solid;box-shadow: inset 0px 0px 10px 5px #D5D5D5'></canvas>";
		eltern.insertAdjacentHTML("BeforeEnd", elem);	// add element to the end of parent element
		canvas2 = document.getElementById("canvas_2");
		ctx2 = canvas2.getContext('2d');
		ctx2.lineWidth = 1;
		elem = "<div id='dataForm' style='z-index: 3; position:absolute;left:"+(canvas1.width-250)+"px;top:30px;width:220px'></div>";
		eltern.insertAdjacentHTML("BeforeEnd", elem);
		frm = document.getElementById("dataForm");
		elem = "<table style='border:1px solid #A5CFE9' rules='all'><tbody id='dataTable'></tbody></table>";
		frm.insertAdjacentHTML("BeforeEnd", elem);
		dtable = document.getElementById("dataTable");
		elem = "<tr id='tpar' style='background-color: #D5EBF9'><th>Parameter</th><th id='tajm'>"+ConvertMinutesToTime(this.data[this.data.length-1][0])+"</th></tr>";
		dtable.insertAdjacentHTML("BeforeEnd", elem);
		for (var i=1; i<this.data[0].length; i++) {
			elem = "<tr><td><input type='checkbox' id='checkbox"+i+"' name='plot"+i+"name' checked></input></td></tr>";
			dtable.insertAdjacentHTML("BeforeEnd", elem);	// first column
			var inpId = document.getElementById("checkbox"+i)
			inpId.onclick = this.replot;
			elem = "<label for='checkbox"+i+"' style='color:"+this.plotColor[i-1]+"'> "+this.labels[i]+"</label>";
			inpId.parentElement.insertAdjacentHTML("BeforeEnd", elem);
			elem = "<td id='myData"+i+"' align='center'>"+this.data[this.data.length-1][i]+"</td>";	// second column
			inpId.parentElement.parentElement.insertAdjacentHTML("BeforeEnd", elem);
		}
	}
	///////////////////////////////////
	this.drawGrid = function()
	{
		const NR_X = 24;
		var precalc;
		var hei = canvas2.height;
		var wid = canvas2.width;
		var xlen = this.data.length;
		var spacingHorizontal = wid/NR_X;
		var gridrangeV = this.getDataRangeY();
		var spacingVertical = SPAN*hei/(gridrangeV[1]-gridrangeV[0]);	// make vertical steps of 5

		ctx2 = canvas2.getContext('2d');
		ctx2.lineWidth = this.lineWidthG;
		ctx2.strokeStyle = this.gridColor;
		ctx2.beginPath();

		for(var i=0;i<xlen+1; i++)
		{
			precalc = i*spacingHorizontal;
			ctx2.moveTo(precalc,1);
			ctx2.lineTo(precalc,hei);
		}

		for(var i=0;i<spacingVertical+1; i++)
		{
			precalc = i*spacingVertical;
			ctx2.moveTo(1,precalc);
			ctx2.lineTo(wid,precalc);
		}

		ctx2.stroke();
		ctx2.closePath();

		// draw x- and y-axis indexes
		this.enumerateIt(NR_X, spacingHorizontal, spacingVertical, gridrangeV);
	}
	///////
	this.enumerateIt = function(nrH, spacH, spV, rangeV)
	{	// display index values
		ctx1 = canvas1.getContext('2d');
		ctx1.fillStyle = this.textColor;
		var posY = canvas2.offsetTop + canvas2.height;
		// horizontal, x-axis
		for(var i=0; i<nrH+1; i++)
			if ( (i%3)==0 )	// draw only each 3rd x-value
				ctx1.fillText( ConvertTo2Digits(i)+":00", 30+spacH*i-10, posY+20);	// offset of 30 due to canvas2 left position
		// vertical, y-axis
		var r = (rangeV[1]-rangeV[0]);
		for(var i=0; i<(r/5)+1; i++)
			ctx1.fillText( rangeV[0]+i*SPAN, 10, posY-i*spV);
	}
	////////////////////////////////////////
	this.getDataRangeY = function()
	{
		var arr = new Array(0,0);
		arr[0] = arr[1] = SPAN/2;
		for (var j=1; j<this.data[0].length; j++)
		{
			var chckId = document.getElementById("checkbox"+j);
			if ( chckId && chckId.checked )
			{// plot only checked plots. get min and max values
				for (var i=0; i<this.data.length; i++)
				{
					var dat = this.data[i][j]*1.0;	// make float
					if ( dat<arr[0] ) arr[0] = dat;	// get minimum value
					if ( dat>arr[1] ) arr[1] = dat;	// get maximum value
				}
			}
		}
		var delta = (arr[1]-arr[0])*0.05;	// margin for range end values
		if ( delta==0 ) {
			// avoid script hangup cased by dividing later by 0
			arr[0] = 0; arr[1] = SPAN;
		} else {
			// round to neareset multiple of SPAN
			arr[1] = RoundTo(arr[1]+delta, SPAN);
			if ( arr[0]<SPAN ) arr[0] = RoundTo(arr[0]-delta, SPAN);
		}
		return arr;
	}
	///////////////////////////////////
	this.getDataRangeX = function()
	{
		var arr = new Array(0,0);
		arr[0] = this.data[0][0];	// get minimum value
		arr[1] = this.data[this.data.length-1][0];	// get maximum value
		return arr;
	}
	//////// draws linear graph and enumerates axes/curve
	this.drawGraphLinear = function()
	{
		var len = this.data.length;
		var hei = canvas2.height;
		var wid = canvas2.width;
		var dataX = this.getDataRangeX();
		var spacingHorizontal = wid/(24*60);	// 24 hours, 60 minutes => 1440 minutes per day
		var dataY = this.getDataRangeY();
		var totalRange = dataY[1]-dataY[0];
		var verticalCoefficient = hei/totalRange;
		var mov,dat;
		var xp,yp;

		for (var j=1; j<this.data[0].length; j++)
		{
			// actualize the last values in the table
			var ldataId = document.getElementById("myData"+j);
			ldataId.innerHTML = this.data[this.data.length-1][j];
			ldataId = document.getElementById("tajm");
			ldataId.innerHTML = ConvertMinutesToTime(this.data[this.data.length-1][0]);
			// check check-box status
			var elemid = document.getElementById("checkbox"+j);
			mov = elemid.checked;
			if ( mov )	// plot data only if check-box is checked
			{
				ctx2.beginPath();
				ctx2.strokeStyle = this.plotColor[j-1];
				ctx2.fillStyle = this.plotColor[j-1];
				for (var i=0; i<len; i++)
				{
					dat = this.data[i][j];
					if ( dat=='' ) continue;
					xp = this.data[i][0]*spacingHorizontal;
					yp = hei-(dat-dataY[0])*verticalCoefficient;
					ctx2.lineTo(xp, yp);
				}
				ctx2.stroke();
				ctx2.closePath();
			}
		}
		dataURL = canvas2.toDataURL();
		img = new Image();
		img.src = dataURL;
	}
	/////////////////////
	this.refresh = function()
	{
		canvas2 = document.getElementById("canvas_2");
		canvas2.width = canvas2.width;	// reset plot
		canvas1 = document.getElementById("canvas_1");
		canvas1.width = canvas1.width;	// reset plot
		this.plot();
	}
	/**********************************************/
	function ConvertTo2Digits(dig)
	{
		var ret = dig.toString();
		if ( ret.length==1 ) ret = "0"+ret;
		return ret;
	}
	/**********************************************/
	function ConvertMinutesToTime(tim)
	{
		return ConvertTo2Digits(Math.floor(tim/60)) + ":" + ConvertTo2Digits(Math.floor(tim%60));
	}
	/**********************************************/
	function IsTimeInArray(ar,tim)
	{
		var i = ar.length;
		while (i--) {
			if (ar[i][0] == tim)
				return i;
		}
		return -1;
	}
	/**********************************************/
	function FindClosestAvailableTime(ar,tim)
	{
		var inOk;
		for (var i=0; i<60*24; i++) {
			if ( (tim-i)>=0 ) {
				inOk = IsTimeInArray(ar, tim-i);
				if ( inOk>=0 ) return (tim-i);//inOk;
			}
			if ( (tim+i)<60*24) {
				inOk = IsTimeInArray(ar, tim+i);
				if ( inOk>=0 ) return (tim+i);//inOk;
			}
		}
		return -1;
	}
	/**********************************************/
	function GetCoordinates(ev, dat)
	{
		var x = ev.hasOwnProperty('offsetX') ? ev.offsetX : ev.layerX;
		var y = ev.hasOwnProperty('offsetY') ? ev.offsetY : ev.layerY;
		t = Math.floor(60*24*x/canvas2.width);
		var t = FindClosestAvailableTime(data_array, t);
		// refresh time info
		document.getElementById("tajm").innerHTML = ConvertMinutesToTime(t);
		// refresh parameter values according to selected time
		tim = IsTimeInArray(data_array, t);
		if ( tim<0) {
			alert("Data does not exist!");
		} else {
			for (var j=1; j<data_array[0].length; j++)
				document.getElementById("myData"+j).innerHTML = data_array[tim][j];
		}
		// display tooltip
		var precalc = t*canvas2.width/(60*24);
		canvas2.width = canvas2.width;	// reset plot
		ctx2.drawImage(img,0,0);
		ctx2.beginPath();
		ctx2.moveTo(precalc,0);
		ctx2.lineTo(precalc,canvas2.height);
		ctx2.stroke();
		ctx2.closePath();
		if (snapit==0) {
			var tltip = document.getElementById("dataForm");
			tltip.style.left = x+50+"px";
			tltip.style.top = y+50+"px";
			tltip.style.opacity = 0.85;
			tltip.style.visibility = "visible";
		}
	}
	////////////////////// 
	this.plot = function()
	{
//		this.prepUI();	// shall be done in caller function
		this.drawGrid();
		this.drawGraphLinear();
		document.getElementById("canvas_2").onmousemove = function(event) { GetCoordinates(event);};
	}
}
/**********************************************/
function RoundTo(x, coef)
{
	if ( 0) {
		var amount = coef * Math.round( x / coef);
	} else {
		if (x>0) x = Math.ceil( x/ coef);
		else x = Math.floor( x/ coef);
		var amount = coef * x;
	}
	return amount;
}

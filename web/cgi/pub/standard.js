var child = new Object();
child.win = null;
var reloadtimer = null
var resizer = null;
var resized = null;
var resizex = 0;
var resizey = 0;

if (navigator.appName.indexOf('Netscape') >= 0) {
    if (navigator.appVersion.substr(0,1) < 5) {
	isW3C = false;
	isIE = false;
	sEvent='e';
	sKey='.which';
	yRef='.top';
	xRef='.left';
	xOff='.clip.width';
	yOff='.clip.height';
	xEvent='.pageX';
	yEvent='.pageY';
	dS='document.';
	sD='';
	sHideV="'hide'";
	sShowV="'show'";
	dRef='self';
	hRef='.innerHeight';
	wRef='.innerWidth';
	xSRef='.screenX';
	ySRef='.screenY';
	xPos=dRef+'.pageXOffset';
	yPos=dRef+'.pageYOffset';
	eSrc='.target';
    } else {
	isW3C = true;
	isIE = false;
	sEvent='e';
	sKey='.which';
	yRef='.top';
	xRef='.left';
	xOff='.offsetWidth';
	yOff='.offsetHeight';
	xEvent='.pageX';
	yEvent='.pageY';
	dS='document.';
	sD='.style';
	sHideV="'hidden'";
	sShowV="'visible'";
	dRef='self';
	hRef='.innerHeight';
	wRef='.innerWidth';
	xSRef='.screenX';
	ySRef='.screenY';
	xPos=dRef+'.pageXOffset';
	yPos=dRef+'.pageYOffset';
	eSrc='.target';
    }
}
else {
    isIE = true;
    isW3C = 0;
    sEvent='window.event';
    sKey='.keyCode';
    yRef='.pixelTop';
    xRef='.pixelLeft';
    xOff='.offsetWidth';
    yOff='.offsetHeight';
    xEvent='.clientX';
    yEvent='.clientY';
    dS='';
    sD='.style';
    sHideV="'hidden'";
    sShowV="'visible'";
    dRef='document.body';
    hRef='.clientHeight';
    wRef='.clientWidth';
    xPos=dRef+'.scrollLeft';
    yPos=dRef+'.scrollTop';
    xSRef='.screenLeft';
    ySRef='.screenTop';
    eSrc='.srcElement';
}

function setLayout() {
    updateLayout();
}

function updateLayout() {
    var yy = eval(yPos);
    yy += Math.max(64 - yy, 0);
    moveLayerY(getLayer('tasks'),yy);
    setTimeout('updateLayout()',100);
}

function setResize(f,w) {
    document.resized = (f) ? f : doResize;

    document.resizex = getDisplayWidth();
    document.resizey = getDisplayHeight();

    if (isIE){
	document.resizer = (w) ? w : null;
	window.onresize = armResize;
    }
    else{
	window.onresize = document.resized;
    }
}

function armResize(e) {
    if(document.resizer) document.resizer(e);
    document.onmouseover = document.resized;
}

function doResize(e) {
    if(!(document.resizex == getDisplayWidth() && document.resizey == getDisplayHeight())){
      var newurl = window.location.href.replace(/\?.*$/, '');
      document.onmouseover = null;
      window.location.replace(newurl+'?ppg='+getResizedLines(e));
    }
}

function getResizedLines(e) {
    var h = (isW3C || document.all) ? eval(dRef+hRef) : e.height;
    return Math.max(Math.floor((h - 66) / getIndexHeight()) - 1, 2);
}

function getDisplayWidth() {
    return eval(dRef+wRef);
}

function getDisplayHeight() {
    return eval(dRef+hRef);
}

function getWindowX() {
    return eval('window'+xSRef);
}

function getWindowY() {
    return eval('window'+ySRef);
}

function showElement(o) {
    eval('o'+sD+'.visibility = '+sShowV);
}

function hideElement(o) {
    eval('o'+sD+'.visibility = '+sHideV);
}

function isElementDisplayed(o) {
    var d = eval('o'+sD+'.display');
    return (d == 'block');
}

function displayElement(o) {
    eval('o'+sD+'.display = "block"');
}

function concealElement(o) {
    eval('o'+sD+'.display = "none"');
}

function getScrollAbove() {
    return(eval(yPos));
}

function getScrollLeft() {
    return(eval(xPos));
}

function layerWalk(l,t) {
    var o;
    for(var i = 0; i < l.length; i++){
	o = eval('l[i]'+t);
	if(o) return o;
	o = layerWalk(l[i].document.layers,t);
	if(o) return o;
    }

    return null;
}

function getImage(id) {
    if(isW3C)
      return document.getElementById(id);
    else if(isIE)
      return document.all[id];
    else if(window.document.images[id])
      return window.document.images[id];
    else
      return layerWalk(window.document.layers,'.document.images.'+id);
}

function getImageX(o) {
    if(isIE || isW3C){
	var x = o.offsetLeft;
	for(var p = o.offsetParent; p != null; p = p.offsetParent)
	  x += p.offsetLeft;

	return x;
    }
    else
      return o.x;
}

function getImageY(o) {
    if(isIE || isW3C){
	var y = o.offsetTop;
	for(var p = o.offsetParent; p != null; p = p.offsetParent)
	  y += p.offsetTop;

	return y;
    }
    else
      return o.y;
}

function getLayer(id) {
    if(isW3C)
      return document.getElementById(id);
    else if(isIE)
      return document.all[id];
    else if(window.document.layers[id])
      return window.document.layers[id];
    else
      return layerWalk(window.document.layers,'.document.layers.'+id);
}

function getElementWidth(l){
    return l.width;
}

function getLayerHeight(l){
    return eval('l'+yOff);
}

function getLayerWidth(l){
    return eval('l'+xOff);
}

function getLayerX(l) {
    return eval('l'+sD+xRef);
}

function getLayerY(l) {
    return eval('l'+sD+yRef);
}

function moveLayerX(l,left) {
    eval ('l'+sD+xRef+'='+left);
}

function moveLayerY(l,top) {
    eval ('l'+sD+yRef+'='+top);
}

function moveLayer(l,left,top) {
    moveLayerX(l,left);
    moveLayerY(l,top);
}

function setLayerText(l,t) {
    if(isIE || isW3C){
	l.innerHTML = t;
    }
    else{
	l.document.open();
	l.document.write(t);
	l.document.close();
    }
}

function getEvent(e) {
    return eval(sEvent);
}

function getEventElement(e) {
    return eval('getEvent(e)'+eSrc);
}

function getEventX(e) {
    return eval('getEvent(e)'+xEvent);
}

function getEventY(e) {
    return eval('getEvent(e)'+yEvent);
}

function getKeyCode(e) {
    return eval('getEvent(e)'+sKey);
}

function getControlKey(e) {
    if(isW3C || isIE){
	return getEvent(e).ctrlKey;
    }
    else{
    	return (e.modifiers & Event.CONTROL_MASK) != 0;
    }
}

function getKeyStr(e) {
    return String.fromCharCode(getKeyCode(e)).toLowerCase();
}

function focuschild() {
    var rv = false;
    var ourif = 'child.win.focus(); rv = true;';
    var ourelse = 'child.win = null; window.onfocus = null;';

    if(isIE && js_version >= 1.3){
	eval('try {'+ourif+'} catch (all) {'+ourelse+'}');
    }
    else{
	eval('if((child.win == null) || (child.win.closed)){'+ourelse+'}else{'+ourif+'}');
    }

    return rv;
}

function cOpen(u,n,a,w,h) {
    if(!focuschild()){
	var xWin = getWindowX(), yWin = getWindowY();
	var wPage = getDisplayWidth(), hPage = getDisplayHeight();;
	var x = Math.min(window.screen.width - wPage, Math.max(0, xWin + ((wPage - w)/2)));
	var y = Math.min(window.screen.height - hPage, Math.max(0, yWin + ((hPage - h)/2)));
	var f = a ? a+',' : '';
	var now = new Date();

	child.win = window.open(u,n+now.getTime(),f+'width='+w+',height='+h+',screenX='+x+',screenY='+y+',top='+y+',left='+x);
	window.onfocus = focuschild;
    }

    return child.win;
}

function cnOpen(u,n,a,w,h) {
    var xOffset = (window.screen.width - w)/2, yOffset = (window.screen.height - h)/2;
    window.open(u,n,a+',width='+w+',height='+h+',screenX='+xOffset+',screenY='+yOffset+',top='+yOffset+',left='+xOffset);
}

function composeMsg(f,d,k) {
    var s = 'compose.tcl', replace = 0;
    switch(f) {
      //case 'reply' : if(confirm('Include all recipients in Reply') == true) c = '?style=ReplyAll&uid='+d ; else c = '?style=Reply&uid='+d ; break;
      case 'reply' : s='reply.tcl'; c = '?style=ReplyAll&uid='+d+'&cid='+k ; break;
      case 'fwd' : c = '?style=Forward&uid='+d+'&cid='+k ; break;
      case 'mailto' : c = '?to='+d+'&cid='+k ; break;
      case 'nickto' : c = '?nickto='+d+'&cid='+k ; replace = 1; break;
      default : c = '?cid='+k ; break;
    }
    if(!replace)
      cOpen(s+c, 'compose', 'scrollbars=yes,resizable=yes', 800, 560);
    else
      window.location.href = s + c;
    return false;
}

function isanum(s) {
    if(s.length == 0)
      return false;

    for(var i = 0; i < s.length; i++)
      if(s.charCodeAt(i) < 48 || s.charCodeAt(i) > 57)
	return false;

    return true;
}

function confOpen(s) {cOpen(s,'config','scrollbars=yes',800,560); return false;}
function abookOpen(s) {cOpen(s,'addrbook','scrollbars=yes,resizable=yes',800,560); return false;}
function helpOpen(s) {cOpen(s,'help','scrollbars=yes,resizable=yes',600,600); return false;}
function aeOpen(s) {cOpen(s, 'addredit', 'scrollbars=yes,resizable=yes', 700, 500); return false}
function taOpen(s) {cOpen(s, 'takeaddr', 'scrollbars=yes,resizable=yes', 700, 500); return false}
function quitOpen(s) {cOpen(s, 'quit', '', 420, 200); return false}

function doReload(ival) {
    if((child.win == null) || (child.win.closed)){
	var newurl = window.location.href.replace(/\?.*$/, '');
	window.location.replace(newurl+'?reload=1');
    }
    else{
	var i = getImage('logo');
	if(i){
	    var now = new Date();
	    var t = new String();

	    t += now.getYear();
	    t += now.getMonth();
	    t += now.getDate();
	    t += now.getHours();
	    t += now.getMinutes();
	    t += now.getSeconds();
	    i.src = location.href.replace(/(.*)\/[^\/]*\.tcl.*/,'$1/ping.tcl/'+t+'/'+ival+'/'+(t - (t % ival))+'.gif');
	}
    }
}

function reloadTimer(s) {
    reloadtimer = window.setInterval('doReload('+s+')', s * 1000);
}

function wp_escape(s) {
    var t = escape(s);
    t = t.replace(/\+/, '%2b');
    return t;
}

function flipCheck(eid){
    var cb = window.document.getElementById(eid);
    if(cb) cb.checked = !cb.checked;
    return false;
}

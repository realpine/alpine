/* $Id: common.js 1160 2008-08-22 01:12:33Z mikes@u.washington.edu $
 * ========================================================================
 * Copyright 2008 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */


// Globals
YAHOO.namespace('alpine');
YAHOO.alpine.current = { c:0, f:null, u:0 };
YAHOO.alpine.mailcheck = { interval:null, timer:null };
YAHOO.alpine.status = { timeout:null, message:[] };
YAHOO.alpine.panel = { count: 0, width: 0 };
YAHOO.alpine.containers = {};


// INIT
function browserDetect() {
	if (YAHOO.env.ua.webkit > 0) {
		document.write("<style type='text/css'> body {font-family:Helvetica,Tahoma,Sans Serif;} td,th {font-size:.75em;} </style>");
	}
}

// GENERAL

function clearTextField(txt, promptText) {
	if (txt.value == promptText) {
		txt.value = "";
	}
}

function recallTextField(txt, promptText) {
	if (txt.value == "") {
		txt.value = promptText;
	}
}

function showDisplayDiv(id) {
    YAHOO.util.Dom.setStyle(id,'display','block');
}

function hideDisplayDivOrSpan(id) {
    YAHOO.util.Dom.setStyle(id,'display','none');
}

function showDisplaySpan(id) {
    YAHOO.util.Dom.setStyle(id,'display','inline');
}

function showLoading(){
    var bp = document.getElementById("bePatient");
    if(bp){
	bp.style.top = "auto";
	bp.style.left = "auto";
    }
}

function hideLoading(){
    var bp = document.getElementById("bePatient");
    if(bp){
	bp.style.top = "-999px";
	bp.style.left = "-999px";
    }
}

// MENU
function menuSwap() {
	this.className = "sfhover";
}

function menuBack() {
	this.className = "menuHdr";
}

function menuFocus() {
	this.parentNode.parentNode.parentNode.className = "sfhover";
}

function menuBlur() {
	this.parentNode.parentNode.parentNode.className = "menuHdr";
}

function initMenus() {
    if (document.getElementById) {
	var LI = document.getElementsByTagName("li");
	var zLI = LI.length;
	for (var i = 0; i < zLI; i++) {
	    if (LI[i].parentNode.parentNode.className == "menuHdr") {
		if (LI[i].firstChild.href) {
		    LI[i].firstChild.onfocus = menuFocus;
		    LI[i].firstChild.onblur = menuBlur;
		}
	    }
	    if (LI[i].className == "menuHdr") {
		LI[i].onmouseover = menuSwap;
		LI[i].onmouseout = menuBack;
	    }
	}
    }
}

function updateElementHtml(id,html){
    var el = document.getElementById(id);
    if(el){
	el.innerHTML = html;
	return true;
    }

    return false;
}

function updateElementHref(id,href){
    var el = document.getElementById(id);
    if(el) el.href = href;
}

function updateElementValue(id,value){
    var el = document.getElementById(id);
    if(el) el.value = value;
}

function quoteHtml(s){
    return s.replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

// SETTINGS

function showAdvancedSettings() { 
	hideDisplayDivOrSpan('normalSettings');
	showDisplayDiv('advancedSettings');
	return false;
}

function showNormalSettings() { 
	hideDisplayDivOrSpan('advancedSettings');
	showDisplayDiv('normalSettings');
	return false;
}

// COMPOSE

function showMoreComposeHeaders() { 
	hideDisplayDivOrSpan('moreComposeHeaderText');
	showDisplaySpan('lessComposeHeaderText');
	showDisplayDiv('moreComposeHeaders');
	return false;
}

function showLessComposeHeaders() { 
	hideDisplayDivOrSpan('lessComposeHeaderText');
	showDisplaySpan('moreComposeHeaderText');
	hideDisplayDivOrSpan('moreComposeHeaders');
	return false;
}

// URI  encode folder

function encodeURIFolderPath(path) {
    return encodeURIComponent(path).replace(/%2F/g,'/');
}

// HELP
function openMailboxHelp(){
    var d = YAHOO.util.Dom.getStyle('listTopMenubar','display');
    if(d && d == 'none') openHelpWindow('read.html');
    else openHelpWindow('inbox.html');
    return(false);
}

function openHelpWindow(url) {
	var top = 30;
	var left = Math.floor(screen.availWidth * 0.66) - 10;
	var width = Math.min(Math.floor(screen.availWidth * 0.33), 400);
	var height = Math.floor(screen.availHeight * 0.8) - 30;
	var win = window.open(document.getElementsByTagName('base')[0].href + "help/" + url, "helpwindow", "top=" + top + ",left=" + left + ",width=" + width + ",height=" + height + ",resizable=yes,scrollbars=yes,statusbar=no");
	win.focus();
}

// NEW MAIL CHECK FUNCTIONS
function setNewMailCheckInterval(interval,onNewMail){
    YAHOO.alpine.mailcheck.interval = interval;
    startNewMailTimer(onNewMail);
}

function startNewMailTimer(onNewMail){
    stopNewMailTimer();
    YAHOO.alpine.mailcheck.timer = window.setTimeout('newMailCheck(1,'+onNewMail+')', YAHOO.alpine.mailcheck.interval * 1000);
}

function stopNewMailTimer(){
    if(YAHOO.alpine.mailcheck.timer){
	window.clearTimeout(YAHOO.alpine.mailcheck.timer);
	YAHOO.alpine.mailcheck.timer = null;
    }
}

function newMailCheck(r,onNewMail){
    stopNewMailTimer();
    YAHOO.util.Connect.asyncRequest('GET',
				    'conduit/newmail.tcl' + encodeURI('?reload=' + r) + urlSalt('&'),
				    {
					customevents:{
					    onStart:function(eventType){ showLoading(); },
					    onComplete:function(eventType){ hideLoading(); }
					},
					success: function(o){
					    var nm = parseNewMail(o.responseText);
					    if(nm.length) addStatusMessage(nm,10);
					    else if(!r) addStatusMessage('No new mail has arrived',3);
					    displayStatusMessages();
					    if(onNewMail) eval(onNewMail);
					},
					failure: function(o){
					    addStatusMessage('newMailCheck Failure: '+o.statusText,15);
					    displayStatusMessages();
					}
				    });

    startNewMailTimer(onNewMail);
    return false;
}

function parseNewMail(s){
    // first four numbers: <new message count> <uid of most recent> <expunge count>
    var nmInfo = s.match(/^([^ ]*) ([^ ]*) ([^ ]*)$/);
    var nmMsg;
    if(nmInfo && nmInfo.length == 4 && nmInfo[1] > 0){
 	nmMsg = '<a href="view/' + YAHOO.alpine.current.c + '/' + encodeURIFolderPath(YAHOO.alpine.current.f.replace(/</g,'&lt;')) + '/' + nmInfo[2] + '" id="statuslink">You have ' + nmInfo[1] + ' new message';
	if (nmInfo[1] > 1) nmMsg += 's';
	nmMsg += '</a>';
	return({html:nmMsg});
    }

    return '';
}

// STATUS MESSAGES

function showStatusMessage(smText,smTimeout){
    if(updateElementHtml('statusMessageText', smText)){
	var dom = YAHOO.util.Dom;
	dom.setStyle('statusMessage','display','block');
	dom.setStyle('weatherBar','display','none');

	clearStatusMessageTimeout();
	// number arg to setTimeout is how long status messages are displayed
	YAHOO.alpine.status.timeout = window.setTimeout('hideStatusMessage()', smTimeout * 1000);
    }
}

function hideStatusMessage(){
    clearStatusMessageTimeout();
    // nothing else to display, then restore original content
    if(!displayStatusMessages()){
	var dom = YAHOO.util.Dom;
	dom.setStyle('statusMessage','display','none');
	dom.setStyle('weatherBar','display','block');
    }
}

function clearStatusMessageTimeout(){
    var t = YAHOO.alpine.status.timeout;
    if(t){
	window.clearTimeout(t);
	t = null;
    }
}

function displayStatusMessages(){
    var smq = YAHOO.alpine.status.message;
    if(smq.length){
	var statusMessage = smq[0];
	smq.splice(0,1);
	showStatusMessage(statusMessage.text,statusMessage.timeout);
	return true;
    }

    return false;
}

function addStatusMessage(smText,smTimeout){
    if(smText){
	var smEntry = {};
	var smq = YAHOO.alpine.status.message;
	if(typeof smText == 'object') smEntry.text = smText.html;
	else smEntry.text = smText.replace(/</g,'&lt;');
	if(smTimeout) smEntry.timeout = smTimeout;
	else smEntry.timeout = 10;
	smq[smq.length] = smEntry;
    }
}

// LIST SELECTION
function markAll(o,listId,mark) {
    var chk = document.getElementsByName(listId);
    var isChecked = o.checked;
    if (chk) {
	if (isNaN(chk.length)) {
	    chk.checked = isChecked;
	    mark(chk);
	}
	else {
	    for(var i = 0; i < chk.length; i++) {
		chk[i].checked = isChecked;
		mark(chk[i]);
	    }
	}
    }
}

function markOne(o) {
    var elTR = o.parentNode.parentNode;
    if(elTR.tagName == 'TR'){
	var dom = YAHOO.util.Dom;
	if(o.checked){
	    if(!dom.hasClass(elTR, 'choice')) dom.addClass(elTR, 'choice');
	}
	else{
	    if(dom.hasClass(elTR, 'choice')) dom.removeClass(elTR, 'choice');
	}
    }
}

// SCRIPT EVAL 
function evalScripts(htmlText){
    var reScript = '<script[^>]*>([\\S\\s]*?)<\/script>';
    var scriptOuter = new RegExp(reScript,'img');
    var scriptInner = new RegExp(reScript,'im');
    var scripts = htmlText.match(scriptOuter);
    if(scripts)
	for(var si = 0; si < scripts.length; si++){
	    var script = scripts[si].match(scriptInner);
	    if(script) eval(script[1]);
	}
}

// MAINTAIN UNREAD COUNT
function incUnreadCount(n){
    var unreadObj = document.getElementById('unreadCurrent');
    if(unreadObj){
	var unreadMatch = unreadObj.innerHTML.match(/ \((\d+)\)/m);
	if(unreadMatch && unreadMatch[1]){
	    var unreadCount = unreadMatch[1];
	    while(n-- > 0) unreadCount++;
	    if(unreadCount > 0) unreadObj.innerHTML = ' (' + unreadCount + ')';
	    else unreadObj.innerHTML = '';
	}
	else if(n > 0){
	    unreadObj.innerHTML = ' (' + n + ')';
	}
    }
}

function decUnreadCount(n){
    var unreadObj = document.getElementById('unreadCurrent');
    if(unreadObj){
	var unreadMatch = unreadObj.innerHTML.match(/ \((\d+)\)/m);
	if(unreadMatch && unreadMatch[1]){
	    var unreadCount = unreadMatch[1];
	    while(n-- > 0 && unreadCount > 0) unreadCount--;
	    if(unreadCount > 0) unreadObj.innerHTML = ' (' + unreadCount + ')';
	    else unreadObj.innerHTML = '';
	}
    }
}

function urlSalt(conjunction){
    var now = new Date();
    return conjunction + 'salt=' + now.getTime();
}

// EMPTY TRASH
function emptyFolder(c,f,u,oDone) {
    var eUrl = 'conduit/empty.tcl?c=' + encodeURIComponent(c) + '&f=' + encodeURIFolderPath(f);
    if(u) eUrl += '&u=' + encodeURIComponent(u);
    YAHOO.util.Connect.asyncRequest('GET',
				    eUrl + urlSalt('&'),
				    {
					customevents:{
					    onStart:function(eventType){ showLoading(); },
					    onComplete:function(eventType){ hideLoading(); }
					},
					success: function(o) {
					    // valid response is "<numApplied> <numSelected> <numTotalMsgs>"
					    var matchResult;
					    if((matchResult = o.responseText.match(/^(\d+)$/m)) == null){
						showStatusMessage('Error: ' + o.responseText, 10);
					    }
					    else if(oDone){
						if(oDone.status){
						    showStatusMessage(matchResult[1] + ' messages from ' + f + ' deleted forever', 3);
						    YAHOO.alpine.current.count -= matchResult[1];
						    if(u == 'selected' && matchResult[1] > 0){
							YAHOO.alpine.current.selected -= matchResult[1];
							hideSelectAllInfo();
						    }
						}
						if(oDone.fn) eval(oDone.fn);
					    }
					},
					failure: function(o) { showStatusMessage('Request Failure: ' + o.statusText + '. Please report this.', 10)}
				    });
    return false;
}

// ALERTS AND DIALOGS
function newPanel(width){
    var dom = YAHOO.util.Dom;
    YAHOO.alpine.panel.count++;
    var panelId = 'panel' + YAHOO.alpine.panel.count;
    var panel = new YAHOO.widget.Panel(panelId,
				       {
					   //fixedcenter: true,
					   xy:[210,120],
					   underlay: 'none',
					   width:width,
					   visible: false,
					   modal:true,
					   dragOnly:true,
					   close: true,
					   constraintoviewport: true
				       });

    // common footer
    var pFoot = document.createDocumentFragment();
    var div = document.createElement('div');
    dom.addClass(div,'bl');
    pFoot.appendChild(div);
    div = document.createElement('div');
    dom.addClass(div,'br');
    pFoot.appendChild(div);
    panel.setFooter(pFoot);
    return(panel);
}

function closePanel(panel){
    panel.hide();
    panel.destroy();
}

function panelHeader(content){
    var dom = YAHOO.util.Dom;
    var pHead = document.createDocumentFragment();

    var el = document.createElement('div');
    dom.addClass(el,'tl');
    pHead.appendChild(el);

    el = document.createElement('span');
    el.innerHTML = content;
    pHead.appendChild(el);

    el = document.createElement('div');
    dom.addClass(el,'tr');
    pHead.appendChild(el);

    return pHead;
}

function panelBody(kind,content,buttons){
    var frag, div;
    var dom = YAHOO.util.Dom;

    var frag = document.createDocumentFragment();

    div = document.createElement('div');
    dom.addClass(div,'dialogIcon');
    dom.addClass(div,kind);
    frag.appendChild(div);
    
    div = document.createElement('div');
    div.setAttribute('id',kind+'Body');
    dom.addClass(div,'dialogBody');
    if(content.html) div.innerHTML = content.html;
    else if(content.frag) div.appendChild(content.frag);

    frag.appendChild(div);

    div = document.createElement('div');
    dom.addClass(div,'yui-skin-sam');
    dom.addClass(div,'dialogButtons');
    frag.appendChild(div);
    if(buttons){
	for(var n = 0; n < buttons.length; n++){
	    var butCont = document.createElement('span');
	    butCont.setAttribute('id','pb' + (n + 1));
	    dom.addClass(butCont,'yuibutton');
	    div.appendChild(butCont);
	    var oButAtt = buttons[n];
	    oButAtt.container = butCont.id;
	    var b = new YAHOO.widget.Button(oButAtt);
	    if(oButAtt.disabled) YAHOO.alpine.panel.button = b;
	}
    }

    return frag;
}

function panelAlert(s,f){
    var buttons = [{ id:'butClose',type:'button',label:'OK',onclick:{ fn: function(){ closePanel(panel); }}}];
    var panel = newPanel('26em');
    panel.setHeader(panelHeader('Alert!'));
    if(f) buttons[0].onclick = function(){ f(); closePanel(panel); };
    panel.setBody(panelBody('alert',{html:s},buttons));
    panel.render(document.body);
    getPanelBodyWidth('alert');
    panel.show();
    return false;
}

function panelConfirm(s,oYes,oNo){
    var buttons = [{id:'butYes',type:'button',label:'OK'},{id:'butClose',label:'Cancel',onclick:{ fn:function(){ closePanel(panel); }}}];
    var panel = newPanel('30em');
    panel.setHeader(panelHeader('Confirm'));
    if(oYes){
	if(oYes.text) buttons[0].label = oYes.text;
	if(oYes.url){
	    buttons[0].type = 'link';
	    buttons[0].href = oYes.url;
	}
	else if(oYes.fn){
	    buttons[0].type = 'button';
	    buttons[0].onclick = { fn: function() { oYes.fn(oYes.args) ; closePanel(panel); }};
	}
    }

    if(oNo){
	if(oNo.text) buttons[1].label = oNo.text;
	if(oNo.fn) buttons[1].onclick = { fn: function(){ oNo.fn(); closePanel(panel); }};
    }

    panel.setBody(panelBody('prompt',{html:s},buttons));
    panel.render(document.body);
    panel.show();
    getPanelBodyWidth('prompt');
    return false;
}

function panelDialog(title,body,done,onClose){
    var buttons = [];
    var panel = newPanel('640px');
    panel.setHeader(panelHeader(title));
    if(done && done.fn){
	var i = buttons.length;
	buttons[i] = {
	    type:'button',
	    id:done.label.replace(/ /g,'_').toLowerCase(),
	    label:done.label,
	    disabled:done.disabled,
	    onclick:{
		fn: function(){
		    done.fn();
		    closePanel(panel);
		}
	    }
	};
	if(done.buttonId) buttons[i].id = done.butId;
    }

    buttons[buttons.length] = {
			         id:'butClose',
				 type:'button',
				 label:'Cancel',
				 onclick:{ fn: function(){ if(onClose) onClose(); closePanel(panel); } }
    };

    panel.setBody(panelBody('dialog',{frag:body},buttons));
    panel.render(document.body);
    panel.show();
    getPanelBodyWidth('dialog');
    return false;
}

function drawFolderList(idContainer,defCollection){
    if(idContainer && idContainer.length){
	var el = document.getElementById(idContainer);
	if(el){
	    newFolderList(el,null,defCollection);
	    YAHOO.alpine.containers.folderlist = el;
	}
    }
}

function newFolderList(elContainer,controlObj,collection,path,objParm){
    var alp = YAHOO.alpine;
    alp.resubmitRequest = function(){ newFolderList(elContainer,controlObj,collection,path,objParm); };
    alp.cancelRequest = function(){ newFolderList(elContainer,controlObj,alp.current.c,"",objParm); };
    if(elContainer){
	var nUrl = 'conduit/folderlist.tcl/' + collection + '/';
	if(path && path.length) nUrl += encodeURIFolderPath(path) + '/';
	var conj = '?';
	var prop, parms = '';
	if(objParm){
	    for(prop in objParm){
		parms += conj + prop + '=' + encodeURIComponent(objParm[prop]);
		conj = '&';
	    }
	}
	YAHOO.util.Connect.asyncRequest('GET',
					nUrl + parms + urlSalt(conj),
					{
					    customevents:{
						onStart:function(eventType){ showLoading(); },
						onComplete:function(eventType){ hideLoading(); }
					    },
					    success: function(o){
						elContainer.innerHTML = o.responseText;
						evalScripts(o.responseText);
						window.scrollTo(0,0);
					    },
					    failure: function(o) { showStatusMessage('Error getting list: '+o.statusText,10); },
						argument: [elContainer]
					});
	if(controlObj) controlObj.blur();
    }

    return false;
}

function processAuthException(o){
    switch(o.code){
    case 'CERTQUERY' :
	panelConfirm('The SSL/TLS certificate for the server<p><center>' + o.server + '</center><br>cannot be validated.<p>The problem is: ' + o.reason,
		     {text:'Accept Certificate',fn:acceptCert,args:{server:o.server}},{fn:YAHOO.alpine.cancelRequest});
	break;
    case 'CERTFAIL' :
	panelAlert('The security certificate on server ' + o.server +'  is not acceptable<p>The problem is: ' + o.reason);
	break;
    case 'NOPASSWD' :
	getCredentials(o);
	break;
    case 'BADPASSWD' :
	getCredentials(o);
	break;
    default :
	addStatusMessage('Unknown Authorization Code: ' + o.code, 15);
	displayStatusMessages();
	break;
    }
}

function acceptCert(o){
    YAHOO.util.Connect.asyncRequest('GET',
				    'conduit/cert.tcl?server=' + encodeURIComponent(o.server) + '&accept=yes' + urlSalt('&'),
				    {
					customevents:{
					    onStart:function(eventType){ showLoading(); },
					    onComplete:function(eventType){ hideLoading(); }
					},
					success: function(o){ if(YAHOO.alpine.resubmitRequest) YAHOO.alpine.resubmitRequest(); },
					failure: function(o){
					    addStatusMessage('Certificate Acceptance Failure: '+o.statusText,15);
					    displayStatusMessages();
					}
				    });
}

function getCredentials(o){
    var dom = YAHOO.util.Dom;
    var alp = YAHOO.alpine;

    var div = document.createElement('div');
    dom.addClass(div,'getAuth');

    var d = document.createElement('div');
    var tail = (o.server) ? ' to '+ o.server + '.' : '...';
    var e = document.createTextNode('Authorization is required to ' + o.reason + '. Please login'+tail);
    d.appendChild(e);
    div.appendChild(d);

    e = document.createElement('span');
    e.innerHTML = 'Username:';
    div.appendChild(e);
    var u = createInputElement('text','u');
    div.appendChild(u);

    e = document.createElement('hr');
    div.appendChild(e);

    e = document.createElement('span');
    e.innerHTML = 'Password:';
    div.appendChild(e);
    var p = createInputElement('password','p');
    div.appendChild(p);

    hideLoading();

    panelDialog('Login',
		div,
		{
		    label:'Login',
		    fn:function(){
			YAHOO.util.Connect.asyncRequest('POST',
							alp.cgi_root + '/session/setauth2.tcl',
							{
							    customevents:{
								onStart:function(eventType){ showLoading(); },
								onComplete:function(eventType){ hideLoading(); }
							    },
							    success: function(o){ if(alp.resubmitRequest) alp.resubmitRequest(); },
							    failure: function(o){
										    addStatusMessage('Login Failure: '+o.statusText,15);
										    displayStatusMessages();
										}
							},
							'auths=Login&user=' + encodeURIComponent(u.value) + '&pass=' + encodeURIComponent(p.value) + '&c=' + encodeURIComponent(o.c) + '&f=' + encodeURIComponent(o.server) + '&sessid=' + encodeURIComponent(o.sessid)+ urlSalt('&'));
		    }
		},
		alp.cancelRequest);

    u.focus();
    return false;
}

function getSmimePassphrase(o){
    var dom = YAHOO.util.Dom;
    var alp = YAHOO.alpine;

    var div = document.createElement('div');
    dom.addClass(div,'getAuth');

    var d = document.createElement('div');
    var addr = (o.addr) ? ' for '+ o.addr : '';
    var e = document.createTextNode('Passphrase is required'+addr+' to decrypt.');
    d.appendChild(e);
    div.appendChild(d);

    e = document.createElement('span');
    e.innerHTML = 'Passphrase:';
    div.appendChild(e);
    var p = createInputElement('password','p');
    div.appendChild(p);

    hideLoading();

    panelDialog('Enter S/MIME Passphrase',
		div,
		{
		    label:'Done',
		    fn:function(){
			YAHOO.util.Connect.asyncRequest('POST',
							alp.cgi_root + '/session/setpassphrase.tcl',
							{
							    customevents:{
								onStart:function(eventType){ showLoading(); },
								onComplete:function(eventType){ hideLoading(); }
							    },
							    success: function(o){newMessageText(o);},
							    failure: function(o){}
							},
							'auths=Smime&pass=' + encodeURIComponent(p.value) + '&sessid=' + encodeURIComponent(o.sessid)+ urlSalt('&'));
		    }
		});

    return false;
}


function flistPick(elChoice,fn){
    elChoice.blur();
    var el = document.getElementById('flPick');
    if(el) el.removeAttribute('id');
    elChoice.parentNode.parentNode.setAttribute('id','flPick');
    updateElementValue('pickFolderName', decodeURIComponent(fn));
    panelDialogEnableButton(true);
    return false;
}

function flistPickPick(elChoice){
    var el = document.getElementById('butChoose');
    if(el){   // two contexts for this element: View/Manage, cp/mv dialog
	if('A' == el.nodeName) window.location.href = elChoice.href;
	else if('SPAN' == el.nodeName) el.firstChild.firstChild.click();
    }
    return false;
}

function panelDialogEnableButton(tf){
    if(YAHOO.alpine.panel.button) YAHOO.alpine.panel.button.set('disabled',!tf);
}

function panelDialogInputChange(e){
    panelDialogEnableButton(YAHOO.util.Event.getTarget(e).value.length > 0);
}

function showFolderList(e){
    hideDisplayDivOrSpan('showFolderList');
    showDisplaySpan('hideFolderList');
    showDisplayDiv('folderList');
    YAHOO.util.Event.preventDefault(e);
}

function hideFolderList(e){
    hideDisplayDivOrSpan('hideFolderList');
    showDisplaySpan('showFolderList');
    hideDisplayDivOrSpan('folderList');
    YAHOO.util.Event.preventDefault(e);
}

function pickFolder(containerID,label,defCollection,onDone){
    var dom = YAHOO.util.Dom;
    var dbody = document.createDocumentFragment();
    var div = document.createElement('div');
    dom.addClass(div,'flistInstructions');
    div.innerHTML = label + ' to: '

    var elFolderName = createInputElement('text','pickFolderName');
    elFolderName.setAttribute('id','pickFolderName');
    elFolderName.value = '';
    div.appendChild(elFolderName);
    YAHOO.util.Event.addListener(elFolderName,'keyup',panelDialogInputChange);

    el = document.createTextNode(' ');
    div.appendChild(el);

    var elFolderCollection = createInputElement('hidden','folderCollection');
    elFolderCollection.value = defCollection;
    elFolderCollection.setAttribute('id','pickFolderCollection');
    div.appendChild(elFolderCollection);

    var elFolderPath = createInputElement('hidden','pickFolderPath');
    elFolderPath.setAttribute('id','pickFolderPath');
    div.appendChild(elFolderPath);

    dbody.appendChild(div);
    div = document.createElement('div');
    div.setAttribute('id',containerID);
    div.innerHTML = 'Loading...';
    dbody.appendChild(div);

    panelDialog(label,
		dbody,
		{
		    buttonId:'butChoose',
		    label:label,
		    disabled: true,
		    fn: function(){
			  var fc = elFolderCollection.value;
			  var fp = elFolderPath.value;
			  var fn = elFolderName.value;
			  if(fp && fp.length) fn = fp + '/' + fn;
			  if(fc >= 0 && fn && fn.length) onDone({c:fc,f:fn});
			  else showStatusMessage('No folder name provided.  No messages moved or copied.',5);
		    }
		});

    elFolderName.focus();
    drawFolderList(containerID,defCollection);
    return false;
}

function getPanelBodyWidth(panelType){
    var region = YAHOO.util.Dom.getRegion(panelType+'Body');
    YAHOO.alpine.panel.width = region.right - region.left - 12;
}

function setPanelBodyWidth(className){
    if(YAHOO.env.ua.ie > 0 && YAHOO.alpine.panel.width){
	var dom = YAHOO.util.Dom;
	var elArray = dom.getElementsByClassName(className,'div');
	if(elArray){
	    for(var i = 0; i < elArray.length; i++){
		dom.setStyle(elArray[i],'width', YAHOO.alpine.panel.width);
	    }
	}
    }
}

// CONTACTS EDITOR
function contactEditor(oMode,onDone){
    var dom = YAHOO.util.Dom;
    var form, input, div, label, el;

    function newField(fieldName,input,inputId,defaultValue){
	var div = document.createElement('div');
	dom.addClass(div,'contactSection');
	var el = document.createElement('div');
	div.appendChild(el);
	dom.addClass(el,'contactField');
	el.innerHTML = fieldName +':';
	input.setAttribute('id',inputId);
	if(defaultValue) input.value = defaultValue;
	div.appendChild(input);
	return(div);
    }

    function validateFields(){
	var n = 0;
	var fields = {book:'',ai:'',contactNick:'',contactName:'',contactEmail:'',contactNotes:'',contactFcc:''};
debugger;
	for(var f in fields) {
	    var el = document.getElementById(f);
	    if(el && el.value && el.value.length){
		var val = el.value.replace(/\n/g, '').replace(/^[ \t]*/, '').replace(/[ \t]*$/, '');
		if(val.length){
		    fields[f] = val;
		    n++;
		}
	    }
	}

	return((n > 0) ? fields : null);
    }

    if(oMode.which == 'add'){
	label = 'Add';
    }
    else if(oMode.which == 'edit'){
	label = 'Change';
    }
    else return false;

    form = document.createElement('form');
    dom.addClass(form,'contactEditor');

    if(!oMode.books){
	el = createInputElement('hidden','book');
	form.appendChild(el);
	el.setAttribute('id','book');
	el.value = (oMode.book) ? oMode.book : 0;
    }

    el = createInputElement('hidden','ai');
    form.appendChild(el);
    el.setAttribute('id','ai');
    el.value = (oMode.index) ? oMode.index : -1;

    el = createInputElement('hidden','origNick');
    form.appendChild(el);
    el.setAttribute('id','origNick');
    if(oMode.nickname) el.value = oMode.nickname;

    el = document.createElement('div');
    form.appendChild(el);
    dom.addClass(el,'context');
    el.innerHTML = label + ' Contact';

    input = createInputElement('text','contactNick');
    form.appendChild(newField('Nickname',input,'contactNick',oMode.nickname));
    form.appendChild(newField('Display Name',createInputElement('text','contactName'),'contactName',oMode.personal));

    if(oMode.group)
	form.appendChild(newField('Group Email',createNameValueElement('textarea', 'name', 'contactEmail'),'contactEmail',oMode.email));
    else
	form.appendChild(newField('Email',createInputElement('text', 'contactEmail'),'contactEmail',oMode.email));

    form.appendChild(newField('Notes',createNameValueElement('textarea', 'name', 'contactNotes'),'contactNotes',oMode.note));
    form.appendChild(newField('Fcc Folder',createInputElement('text','contactFcc'),'contactFcc',oMode.fcc));

    if(oMode.books){
	var sel = document.createElement('select');
	sel.setAttribute('name','book');
	sel.setAttribute('id','book');
	for(var i = 0; i < oMode.books.length; i++){
	    var opt = new Option(oMode.books[i].name,oMode.books[i].book);
	    sel.options[sel.options.length] = opt;
	}

	sel.selectedIndex = 0;
	form.appendChild(newField('Address Book',sel,'contactAbook'));
    }

    if(!onDone) onDone = storeContact;
    panelDialog(label, form, { label:label, fn: function(){ onDone(validateFields()); }});
    input.focus();
    return false;
}

function storeContact(oFields){
    if(oFields){
	var sUrl = 'conduit/storecontact.tcl?';
	for(var f in oFields) sUrl += '&' + f + '=' + encodeURIComponent(oFields[f]);
	storeDS = new YAHOO.util.DataSource(sUrl);
	storeDS.responseType = YAHOO.util.DataSource.TYPE_XML;
	storeDS.responseSchema = {
	    resultNode: 'Result',
	    fields: ['StatusText']
	};
	// bug: if present, store parts should be passed in draw request
	storeDS.sendRequest('',
			    {
				success: function(oReq,oResp,oPayload){
				    for(var i = 0; i < oResp.results.length; i++) addStatusMessage(oResp.results[i].StatusText, 10);
				    displayStatusMessages();
				},
				    failure: function(oReq,oResp,oPayload){
				    showStatusMessage('Error Taking Address: ' + oResp.responseText, 10);
				},
				scope: this
			    });
    }
}


function contactsChecked(action){
    var chkd = [];
    var i, entry, l = document.getElementsByName('nickList');
    if(l){
	for(i = 0; i < l.length; i++){
	    if(l[i].checked){
		entry = l[i].value.match(/^(\d+)\.(\d+)\.(.*)$/);
		if(entry) chkd[chkd.length] = { book:entry[1], index:entry[2], nick:entry[3] };
	    }
	}
    }

    if(0 == chkd.length){
	var t = 'No Contacts Selected<p>Choose contacts by clicking checkbox to left of contact.';
	if(action) t += 'Then press '+action;
	panelAlert(t);
    }

    return(chkd);
}

// LOAD From: INTO CONTACT EDITOR
function takeAddressFrom(uid,o){
    var books = (o) ? o.books : null;
    // get personal and email based on uid
    takeDSRequest = 'conduit/take.tcl?op=from&u=';
    takeDS = new YAHOO.util.DataSource(takeDSRequest);
    takeDS.responseType = YAHOO.util.DataSource.TYPE_XML;
    takeDS.responseSchema = {
	resultNode: 'Result',
	fields: ['Type','Email','Personal','Nickname','Fcc','Note']
    };
    takeDS.sendRequest(uid,
		       {
			   customevents:{
			       onStart:function(eventType){ showLoading(); },
			       onComplete:function(eventType){ hideLoading(); }
			   },
			   success: function(oReq,oResp,oPayload){
			       if(oResp.results.length == 1){
				   var o = {
				       which:oResp.results[0].Type,
				       nickname:oResp.results[0].Nickname,
				       personal:oResp.results[0].Personal,
				       email:oResp.results[0].Email,
				       fcc:oResp.results[0].Fcc,
				       note:oResp.results[0].Note
				   };
				   if(books) o.books = books;
				   contactEditor(o);
			       }
			       else showStatusMessage('Too many From addresses, Use Take', 10);
			   },
			   failure: function(oReq,oResp,oPayload){
			       showStatusMessage('Error Taking Address: ' + oResp.responseText, 10);
			   },
			   scope: this,
			   arguments:[books]
		       });

    return true;
}

// CONTACT LISTER

function newContactList(elContainer,controlObj,idBook,objParm){
    if(elContainer){
	var nUrl = 'conduit/contactlist.tcl/' + idBook;
	var conj = '?';
	var prop, parms = '';
	if(objParm){
	    for(prop in objParm){
		parms += conj + prop + '=' + encodeURIComponent(objParm[prop]);
		conj = '&';
	    }
	}
	YAHOO.util.Connect.asyncRequest('GET',
					nUrl + parms + urlSalt(conj),
					{
					    customevents:{
						onStart:function(eventType){ showLoading(); },
						onComplete:function(eventType){ hideLoading(); }
					    },
					    success: function(o){
						elContainer.innerHTML = o.responseText;
						evalScripts(o.responseText);
						setPanelBodyWidth('clistContacts');
						window.scrollTo(0,0);
					    },
					    failure: function(o){
						showStatusMessage('Cannot build list: '+o.statusText,10);
					    },
					    argument: [elContainer]
					});
	if(controlObj) controlObj.blur();
    }
    return false;
}

function drawContactList(idContainer,idBook,objParm){
    if(idContainer && idContainer.length){
	var el = document.getElementById(idContainer);
	if(el){
	    newContactList(el,null,idBook,objParm);
	    YAHOO.alpine.containers.contactlist = el;
	}
    }
}

// Search within Content
function searchContent(context,treeId){
    var elSearch = document.getElementById('searchField');
    if(elSearch && elSearch.value && elSearch.value.length){
	var dom = YAHOO.util.Dom;
	var i, j, node, nodeParent;
	var oldMatches = dom.getElementsByClassName('searchMatch','span',treeId);
	for(i = 0; i < oldMatches.length; i++){
	    // BUG: coalesce node's text and prev/next sibling's text into single node
	    node = oldMatches[i];
	    nodeParent = node.parentNode;
	    for(j = 0; j < node.childNodes.length; j++) nodeParent.insertBefore(node.childNodes[j].cloneNode(true),node);
	    nodeParent.removeChild(node);
	}

	var lcSearch = elSearch.value.toLowerCase();
	var nodeText, matchOffset, found = {n:0};
	var removeNodes = [];
	var nodeArray = function(root){
	    var textNodes = [];

	    function appendTextNodes(el){
		for(var o = el.firstChild; o; o = o.nextSibling){
		    switch(o.nodeType){
		    case 3 : textNodes[textNodes.length] = o; break;
		    case 1 : appendTextNodes(o); break;
		    }
		}
	    }

	    appendTextNodes(root);
	    return(textNodes);
	}(document.getElementById(treeId));

	var nodeBefore, nodeAfter, span;
	for(i = 0; i < nodeArray.length; i++){
	    node = nodeArray[i];
	    nodeText = node.nodeValue;
	    matchOffset = nodeText.toLowerCase().indexOf(lcSearch);
	    if(matchOffset >= 0){
		parentNode = node.parentNode;
		nodeBefore = document.createTextNode(nodeText.substr(0,matchOffset));
		nodeAfter = document.createTextNode(nodeText.substr(matchOffset+lcSearch.length));
		matchText = nodeText.substr(matchOffset, lcSearch.length);
		span = document.createElement("span");
		dom.addClass(span,'searchMatch');
		span.appendChild(document.createTextNode(matchText));
		parentNode.insertBefore(nodeBefore,node);
		parentNode.insertBefore(span,node);
		parentNode.insertBefore(nodeAfter,node);
		removeNodes[removeNodes.length] = {child: node, parent: parentNode};
		found.n++;
		if(!found.o) found.o = parentNode;
	    }
	}

	for(i = 0; i < removeNodes.length; i++){
	    node = removeNodes[i];
	    node.parent.removeChild(node.child)
	}
    }

    if(found.n > 0){
	var plural = (found.n > 1) ? 'es' : '';
	showStatusMessage(found.n + ' match' + plural + ' found', 3);
	found.o.scrollIntoView();
    }
    else showStatusMessage('No ' + context + ' found matching your seach', 3);

    return(false);
}

// Advanced search
function advanceSearch(){
    var dom = YAHOO.util.Dom;
    var i, d, form, el, span, dates;
    var elScope, elWhichAddr, elWhichAddrText, elWhichSubj, elWhichSubjText, elWhichBody, elWhichBodyText, elWhichStatus, elWhichDate, elMonth, elDay, elYear;
    function newSearchDiv(cl,el){
	var div = document.createElement('div');
	dom.addClass(div,cl);
	if(el) div.appendChild(el);
	return(div);
    }

    function newSearch(title,elSelect,elCriteria){
	var div = document.createElement('div');
	dom.addClass(div,'searchSection');
	div.appendChild(newSearchDiv('searchField',document.createTextNode(title + ':')));
	div.appendChild(newSearchDiv('searchType',elSelect));
	div.appendChild(newSearchDiv('searchTerm',elCriteria));
	return(div);
    }

    function newSelect(id,opts,onchange){
	var elSel = document.createElement('select');
	elSel.setAttribute('name',id);
	elSel.setAttribute('id',id);
	if(onchange) YAHOO.util.Event.addListener(elSel,'change',onchange);
	for(var i = 0; i < opts.length; i++) elSel.options[elSel.options.length] = new Option(opts[i],i);
	return elSel;
    }

    function newInput(id){
	var el = createInputElement('text',id);
	el.setAttribute('id',id);
	YAHOO.util.Event.addListener(el,'keypress',searchOnEnter,'_search_');
	return(el);
    }

    form = document.createElement('form');
    dom.addClass(form,'advanceSearch');

    el = document.createElement('div');
    form.appendChild(el);
    dom.addClass(el,'context');
    el.innerHTML = 'Search in ' + quoteHtml(YAHOO.alpine.current.f);

    el = document.getElementById('searchRefine');
    if(el){
	d = dom.getStyle(el,'display');
	if(d && d != 'none'){
	    el = document.createElement('div');
	    form.appendChild(el);
	    dom.addClass(el,'scope');
	    elScope = newSelect('searchScope',['Perform this search within current search results','Add results of this search to current results','Perform a fresh search, abandoning the current results']);
	    el.appendChild(elScope);
	}
    }

    elWhichAddr = newSelect('whichAddr',['From:','To:','Cc:','From:, Cc:','From:, To:, Cc:']);
    elWhichAddrText = newInput('textAddr');
    form.appendChild(newSearch('Address',elWhichAddr,elWhichAddrText));
    elWhichSubj = newSelect('whichSubj',['Contains','Does Not Contain']);
    elWhichSubjText = newInput('textSubj');
    form.appendChild(newSearch('Subject',elWhichSubj,elWhichSubjText));
    elWhichBody = newSelect('whichBody',['Contains','Does Not Contain']);
    elWhichBodyText = newInput('textBody');
    form.appendChild(newSearch('Message Text',elWhichBody,elWhichBodyText));

    // prep date fields
    span = document.createElement('span');
    span.setAttribute('id','searchDates');
    dom.setStyle(span,'display','none');
    elMonth = newSelect('searchMonth',['------','January','February','March','April','May','June','July','August','September','October','November','December']);
    span.appendChild(elMonth);
    dates = ['--'];
    for(i = 1; i < 32; i++) dates[dates.length] = i;
    elDay = newSelect('searchDay', dates);
    span.appendChild(elDay);
    el= document.createTextNode(', ');
    span.appendChild(el);
    dates = ['----'];
    d = new Date();
    for(i = d.getFullYear(); i > 1970; i--) dates[dates.length] = i;
    elYear = newSelect('searchYear',dates);
    span.appendChild(elYear);
    elWhichDate = newSelect('whichDate',['Anytime','Today','Past week','Past month','On','Since','Before'],advanceSearchDate);
    form.appendChild(newSearch('Date',elWhichDate,span));

    // empty searchTerm
    elWhichStatus = newSelect('whichStatus',['All Message','Unread Message','Read Messages','Starred Messages','Unstarred Messages']);
    form.appendChild(newSearch('Status',elWhichStatus));

    panelDialog('Advanced Search',
		form,
		{
		    label:' Search ',
		    fn: function() {
			var scope = 'new', criteria = '';

			if(elScope){
			    scope = ['narrow','broad'][elScope.selectedIndex];
			}

			if(elWhichAddrText.value.length){
			    criteria += '{text ton ' + ['from','to','cc','recip','partic'][elWhichAddr.selectedIndex] + ' {' + elWhichAddrText.value + '}} ';
			}

			if(elWhichSubjText.value.length){
			    criteria += '{text ';
			    criteria += (elWhichSubj.selectedIndex > 0) ? 'not' : 'ton';
			    criteria += ' subj {' + elWhichSubjText.value + '}} ';
			}

			if(elWhichBodyText.value.length){
			    criteria += '{text ';
			    criteria += (elWhichBody.selectedIndex > 0) ? 'not' : 'ton';
			    criteria += ' body {' + elWhichBodyText.value + '}} ';
			}

			if(elWhichDate.selectedIndex > 0 && elMonth.selectedIndex > 0 && elDay.selectedIndex > 0 && elYear.selectedIndex > 0){
			    criteria += '{date ' + ['','on','since','since','on','since','before'][elWhichDate.selectedIndex];
			    criteria += ' ' + elYear.options[elYear.selectedIndex].text;
			    criteria += ' ' + elMonth.options[elMonth.selectedIndex].text.substring(0,3).toLowerCase();
			    criteria += (elDay.selectedIndex < 10) ? ' 0' : ' ';
			    criteria += elDay.selectedIndex;
			    criteria += '} ';
			}

			if(elWhichStatus.selectedIndex > 0){
			    criteria += '{status ' + ['ton new','not new','ton imp','not imp'][elWhichStatus.selectedIndex] + '} ';
			}

			newMessageList({parms:{op:'search',type:'compound',scope:scope,criteria:criteria,page:'new'}});
		    }
		});

    return false;
}

function advanceSearchDate(e){
    var y, d = new Date();

    function setSelect(id,i){
	var el = document.getElementById(id);
	if(el) el.selectedIndex = i;
    }

    function setSearchDate(m,d,y){
	setSelect('searchMonth',m);
	setSelect('searchDay',d);
	setSelect('searchYear',y);
    }

    switch(YAHOO.util.Event.getTarget(e).selectedIndex){
    case 0 :
	hideDisplayDivOrSpan('searchDates');
	setSearchDate(0,0,0);
	break;
    case 1:
	hideDisplayDivOrSpan('searchDates');
	setSearchDate(d.getMonth() + 1, d.getDate(), 1);
	break;
    case 2 :
	hideDisplayDivOrSpan('searchDates');
	y = d.getFullYear();
	d.setDate(d.getDate() - 7);
	setSearchDate(d.getMonth() + 1,d.getDate(),(y - d.getFullYear()) + 1);
	break;
    case 3 :
	hideDisplayDivOrSpan('searchDates');
	y = d.getFullYear();
	if(d.getMonth()) d.setMonth(d.getMonth() - 1); else d.setMonth(11);
	setSearchDate(d.getMonth() + 1, d.getDate(),(y - d.getFullYear()) + 1);
	break;
    default :
	showDisplaySpan('searchDates');
	setSearchDate(d.getMonth() + 1, d.getDate(), 1);
	break;
    }
}

function searchOnEnter(e,btn){
    if(YAHOO.util.Event.getCharCode(e) == 13){
	var el = document.getElementById(btn);
	if(el){
	    // dig thru any decoration to find button
	    while(el && !el.click) el = el.firstChild;
	    if(el && el.click){
		el.click();
		return false;
	    }
	}
    }

    return true;
}

function rotateNews(el){
    if(el.parentNode){
	var dom = YAHOO.util.Dom;
	var node = el.parentNode;

	dom.setStyle(node, 'display', 'none');
	if(node.nextSibling && node.nextSibling.tagName == 'SPAN'){
	    node = node.nextSibling;
	}
	else{
	    for(var node = el.parentNode; node.previousSibling && node.previousSibling.tagName == 'SPAN'; node = node.previousSibling)
		;
	}

	dom.setStyle(node,'display','inline');
    }

    return false;
}

// Hacks to work around IE
function createInputElement(type,name){
    if(YAHOO.env.ua.ie > 0){
	return(document.createElement('<input type="' + type + '" name="' + name + '">'));
    }

    var el = document.createElement('input');
    el.setAttribute('type',type);
    el.setAttribute('name',name);
    return(el);
}

function createNameValueElement(tag, name, nameAttr){
    if(YAHOO.env.ua.ie > 0){
	return(document.createElement('<' + tag + ' ' + name + '="' + nameAttr + '">'));
    }

    var el = document.createElement(tag);
    el.setAttribute(name,nameAttr);
    return(el);
}

// HACKS TO MAINTAIN FIX DIV IN VIEWPORT
function sizeVPHeight(){
    // force msg txt container to fit in viewport
    var dom = YAHOO.util.Dom;
    var docHeight = dom.getDocumentHeight();
    var topToolbarY = dom.getY('topToolbar');
    var alpineContentY = dom.getY('alpineContent');
    var bottomToolbarY = dom.getY('bottomToolbar');
    var bottomHeight =  docHeight - bottomToolbarY;
    var contentHeight = dom.getViewportHeight() - (alpineContentY + bottomHeight);
    var leftColumnHeight = contentHeight + (alpineContentY - topToolbarY);
    if(contentHeight > 0){
	dom.setStyle('alpineContent','height', Math.round(contentHeight));
	dom.setStyle('leftColumn','height', Math.round(leftColumnHeight));
    }
}

function resizeVPHeight(){
    var dom = YAHOO.util.Dom;
    dom.setStyle('alpineContent','height', 'auto');
    dom.setStyle('leftColumn','height', 'auto');
    sizeVPHeight();
}

// DEBUGGING
function dumpProp(o){
    var s = '';
    for(p in o)	s += p + ' = ' + o[p] + '\n';
    return s;
}

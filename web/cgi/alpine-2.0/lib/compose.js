/* $Id: compose.js 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
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

// globals in our namespace
YAHOO.alpine.autodraft = { timeout:0, timer:null, reported:0, reporttimer:null };
YAHOO.alpine.pickcontact = { tabs:null, markup:null };
YAHOO.alpine.uploading = 0;


function initSimpleEditor(focus){
    var dom = YAHOO.util.Dom;
    var edRegion = dom.getRegion('composeText');
    var textareaHeight = edRegion.bottom - edRegion.top;
    var textareaWidth = edRegion.right - edRegion.left - 10;
    if(edRegion){
	var editAttrib = {
		height:textareaHeight,
		width: textareaWidth,
		toolbar: {
		    draggable: false,
		    buttons: [
	{ group: 'fontstyle', label: 'Font Name and Size',
	  buttons: [
	{ type: 'select', label: 'Arial', value: 'fontname', disabled: true,
	  menu: [
	{ text: 'Arial', checked: true },
	{ text: 'Arial Black' },
	{ text: 'Comic Sans MS' },
	{ text: 'Courier New' },
	{ text: 'Lucida Console' },
	{ text: 'Tahoma' },
	{ text: 'Times New Roman' },
	{ text: 'Trebuchet MS' },
	{ text: 'Verdana' }
		 ]
	},
	{ type: 'spin', label: '13', value: 'fontsize', range: [ 9, 75 ], disabled: true }
		    ]
	},
	{ type: 'separator' },
	{ group: 'textstyle', label: 'Font Style',
	  buttons: [
	{ type: 'push', label: 'Bold CTRL + SHIFT + B', value: 'bold' },
	{ type: 'push', label: 'Italic CTRL + SHIFT + I', value: 'italic' },
	{ type: 'push', label: 'Underline CTRL + SHIFT + U', value: 'underline' },
	{ type: 'separator' },
	{ type: 'color', label: 'Font Color', value: 'forecolor', disabled: true },
	{ type: 'color', label: 'Background Color', value: 'backcolor', disabled: true }
		    ]
	},
	{ type: 'separator' },
	{ group: 'indentlist', label: 'Lists',
	  buttons: [
	{ type: 'push', label: 'Create an Unordered List', value: 'insertunorderedlist' },
	{ type: 'push', label: 'Create an Ordered List', value: 'insertorderedlist' }
		    ]
	},
	{ type: 'separator' },
	{ group: 'alignment', label: 'Alignment',
	  buttons: [
	{ type: 'push', label: 'Align Left CTRL + SHIFT + [', value: 'justifyleft' },
	{ type: 'push', label: 'Align Center CTRL + SHIFT + |', value: 'justifycenter' },
	{ type: 'push', label: 'Align Right CTRL + SHIFT + ]', value: 'justifyright' },
	{ type: 'push', label: 'Justify', value: 'justifyfull' }
		    ]
	},
	{ type: 'separator' },
	{ group: 'insertitem', label: 'Insert Item',
	  buttons: [
	{ type: 'push', label: 'HTML Link CTRL + SHIFT + L', value: 'createlink', disabled: true },
	{ type: 'push', label: 'Insert Image', value: 'insertimage' }
		    ]
	}
			      ]
		}
	};
	if(focus){
	    editAttrib.focusAtStart = true;
	}
	gRichtextEditor = new YAHOO.widget.SimpleEditor('composeText', editAttrib);
	gRichtextEditor.textareaHeight = textareaHeight;
	gRichtextEditor.textareaWidth = textareaWidth;
    }
}


function flipRichText(){
    var dom = YAHOO.util.Dom;
    if(gRichState){
	gRichState = 0;
	gRichtextEditor.saveHTML();
	var stripHTML = /<\S[^><]*>/g;
	gRichtextEditor.get('textarea').value = gRichtextEditor.get('textarea').value.replace(/<br>/gi, '\n').replace(stripHTML, '').replace(/&lt;/g,'<').replace(/&gt;/g,'>');
	dom.setStyle(gRichtextEditor.get('element_cont').get('firstChild'), 'position', 'absolute');
	dom.setStyle(gRichtextEditor.get('element_cont').get('firstChild'), 'top', '-9999px');
	dom.setStyle(gRichtextEditor.get('element_cont').get('firstChild'), 'left', '-9999px');
	gRichtextEditor.get('element_cont').removeClass('yui-editor-container');
	dom.setStyle(gRichtextEditor.get('element'), 'visibility', 'visible');
	dom.setStyle(gRichtextEditor.get('element'), 'top', '');
	dom.setStyle(gRichtextEditor.get('element'), 'left', '');
	dom.setStyle(gRichtextEditor.get('element'), 'height', gRichtextEditor.textareaHeight);
	dom.setStyle(gRichtextEditor.get('element'), 'width', gRichtextEditor.textareaWidth);
	dom.setStyle(gRichtextEditor.get('element'), 'position', 'static');
	document.getElementById('flipRich').innerHTML = 'Rich Text';
    }
    else{
	if(!gRichtextRendered){
	    // fixup initial text
	    var t = document.getElementById('composeText').value.replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/\n/g, '<br>');
	    document.getElementById('composeText').value = t;
	    initSimpleEditor(1);
	    gRichtextEditor.render();
	    gRichtextRendered = true;
	}
	gRichState = 1;
	dom.setStyle(gRichtextEditor.get('element_cont').get('firstChild'), 'position', 'static');
	dom.setStyle(gRichtextEditor.get('element_cont').get('firstChild'), 'top', '0');
	dom.setStyle(gRichtextEditor.get('element_cont').get('firstChild'), 'left', '0');
	dom.setStyle(gRichtextEditor.get('element'), 'visibility', 'hidden');
	dom.setStyle(gRichtextEditor.get('element'), 'top', '-9999px');
	dom.setStyle(gRichtextEditor.get('element'), 'left', '-9999px');
	dom.setStyle(gRichtextEditor.get('element'), 'position', 'absolute');
	gRichtextEditor.get('element_cont').addClass('yui-editor-container');
	gRichtextEditor._setDesignMode('on');
	if(gRichtextEditor.get('textarea')) gRichtextEditor.setEditorHTML(gRichtextEditor.get('textarea').value.replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/\n/g, '<br>'));
	document.getElementById('flipRich').innerHTML = 'Plain Text';
    }

    return false;
}

function autoSizeThis(e){
    if(e.keyCode == 13 || e.keyCode == 10) YAHOO.util.Event.preventDefault(e);
    else autoSize(this);
}

function autoSize(el){
    var dom = YAHOO.util.Dom;
    var r = dom.getRegion(el)
	var th = r.bottom - r.top;
    var maxh = 3 * gFieldHeight;
    if(!el.value.length){
	dom.setStyle(el,'height',(gFieldHeight + 4)+'px');
    }
    else if(th < maxh){
	if(el.scrollHeight > gFieldHeight){
	    var h = (2 * (gFieldHeight));
	    if(el.scrollHeight > h) h += gFieldHeight;
	    dom.setStyle(el,'height',h + 'px');
	}
    }
}

function autoSizeAddressField(id){
    var el = document.getElementById(id);
    if(el){
	if(!gFieldHeight){
	    var r = YAHOO.util.Dom.getRegion(el);
	    gFieldHeight = r.bottom - r.top - 4;
	}

	YAHOO.util.Event.addListener(el,'keypress',autoSizeThis);
	autoSize(el);
    }
}

function autoCompleteDefaults(o){
    if(o){
	o.delimChar = ',';
	o.maxResultsDisplayed = 12;
	o.minQueryLength = 2;
	o.animSpeed = 0.1;
	o.formatResult = function(aResults, sQuery) {
	    var r, where;
	    // highlight matching substrings
	    var oREHilite = new RegExp(sQuery,'ig');
	    var sEmail = aResults[0].replace(/[<]/,'&lt;')
	    var sNick = aResults[1].replace(/[<]/,'&lt;')
	    r = sEmail.replace(oREHilite,'<b>$&</b>');
	    where = sNick.replace(oREHilite,'<b>$&</b>');
	    if(where.length) r += ' (' + where + ')';
	    return r;
	};
    }
}

function actuallySend(o){
    YAHOO.alpine.resubmitRequest = function(){ actuallySend(o); };

    if(YAHOO.alpine.disablePost) return;

    stopAutoDrafting();

    document.getElementById('sendOp').value = o.op;

    if(gRichtextEditor && gRichState){
	gRichtextEditor.saveHTML();
	document.getElementById('contentSubtype').value = 'HTML';
    }

    showLoading();

    while(YAHOO.alpine.uploading > 0)
	panelAlert('Finishing attachment upload before sending...');

    document.getElementById('composeForm').submit();
}

function sendSuccess(m){
    var alp = YAHOO.alpine;
    alp.disablePost = true;
    window.location.href = alp.cgi_base + 'browse/' + alp.current.c + '/' + alp.current.f;
}

function sendFailure(m){
    addStatusMessage('Send Failure: ' + m);
    displayStatusMessages();
}

function processPostAuthException(o){
    YAHOO.alpine.cancelRequest = null;
    processAuthException(o);
}

function cancelComposition(dest){
    stopAutoDrafting();
    var changes = 0;
    var el, changeFields = ['fieldTo','fieldCc','fieldBcc','fieldSubject'];
    for (f in changeFields){
	el = document.getElementById(changeFields[f]);
	changes += el.value.length;
    }

    if(!changes){
	if(gRichState){
	    changes += gRichtextEditor.getEditorHTML().length;
	}
	else{
	    el = document.getElementById('composeText');
	    if(el) changes += el.value.length;
	}
    }

    if(changes) panelConfirm('You have  unsaved changes.<br><br>Click <b>Discard</b> to abandon your text and exit Compose.<br>Click <b>Cancel</b> to continue editing.',{text:'Discard',fn:abandonComposition,args:{url:dest}},{fn:startAutoDrafting});
    else abandonComposition({url:dest});

    return false;
}

function sendComposition(){
    stopAutoDrafting();

    if(!(document.getElementById('fieldTo').value.length
	 || document.getElementById('fieldCc').value.length
	 || document.getElementById('fieldBcc').value.length
	 || document.getElementById('fieldFcc').value.length)){
	panelAlert('Message must contain at least one To, Cc, Bcc, or Fcc recipient!', startAutoDrafting);
    }
    else{
	//panelConfirm('Are you sure you want to send this message?',{text:'Send',fn:actuallySend,args:{op:'send'}},{fn:startAutoDrafting});
	actuallySend({op:'send'});
    }

    return false;
}

function saveDraft(){
    actuallySend({op:'postpone'});
    return false;
}

function setAutoDraftInterval(iTimeOut){
    YAHOO.alpine.autodraft.timeout = iTimeOut;
    startAutoDrafting();
}

function startAutoDrafting(){
    var aa = YAHOO.alpine.autodraft;
    if(aa.timeout) aa.timer = window.setTimeout('autoDraft();', aa.timeout * 1000);
}

function stopAutoDrafting(){
    var aa = YAHOO.alpine.autodraft;
    if(aa.timer) clearTimeout(aa.timer);
    aa.timer = null;
}

function autoDraft(){
    var form = document.getElementById('composeForm');
    var formTarget = form.getAttribute('target');
    form.setAttribute('target','formResponse');
    actuallySend({op:'autodraft'});
    form.setAttribute('target', formTarget);
    startAutoDrafting();
}

function reportAutoDraftUpdate(){
    var aa = YAHOO.alpine.autodraft;
    var t = 'Copy saved to Drafts ' + aa.reported + ' minute';
    if(aa.reported > 1) t += 's';
    document.getElementById('lastAutoDraft').innerHTML = t + ' ago';
    aa.reported += 1;
}

function reportAutoDraft(result){
    if(result.match(/^[0-9]+$/)){
	var aa = YAHOO.alpine.autodraft;
	document.getElementById('autoDraftUid').value = result;
	document.getElementById('lastAutoDraft').innerHTML = 'Copy saved to Drafts less than one minute ago';
	if(aa.reporttimer) clearInterval(aa.reporttimer);
	aa.reported = 1;
	aa.reporttimer = setInterval('reportAutoDraftUpdate();', + 60000);
    }
    else{
	document.getElementById('lastAutoDraft').innerHTML = 'Copy save to Drafts FAILED: ' + result;
	addStatusMessage('Copy autosave to Drafts failed: ' + result);
	displayStatusMessages();
    }
}

function abandonComposition(o){
    var u = document.getElementById('autoDraftUid').value;
    if(u.match(/^\d+$/) && u > 0){
	emptyFolder(gDefCol, gDraftFolder, u, {fn:'window.location.href="' + o.url + '"'});
    }
    else{
	window.location.href = o.url;
    }
}

function drawAttachmentList(o){
    var dom = YAHOO.util.Dom;
    var el, attachList = document.getElementById('attachList');
    var idList = '';
    var comma = '';
    hideLoading();
    while(attachList.childNodes.length) attachList.removeChild(attachList.childNodes[0]);
    if(o){
	if(o.error){
	    addStatusMessage('Attach Error: ' + o.error);
	    displayStatusMessages();
	}

	if(o.attachments && o.attachments.length){
	    var frag = document.createDocumentFragment();
	    var delim = '', len, size, bytes;
	    for(var i = 0; i < o.attachments.length; i++){
		idList += comma + o.attachments[i].id;

		if(delim.length){
		    el = document.createTextNode(delim);
		    frag.appendChild(el);
		}

		el = document.createElement('img');
		el.setAttribute('src','img/cbn/attach_sm.gif');
		frag.appendChild(el);

		l = o.attachments[i].size.length;
		if(l > 6){
		    size = o.attachments[i].size.substr(0, l - 6);
		    bytes = 'MB';
		}
		else if(o.attachments[i].size.length > 3){
		    size = o.attachments[i].size.substr(0, l - 3);
		    bytes = 'KB';
		}
		else{
		    size = o.attachments[i].size;
		    bytes = 'B';
		}

		el = document.createElement('span');
		dom.addClass(el,'attachmentName');
		el.innerHTML = o.attachments[i].fn + ' ' + '(' + size + ' ' + bytes + ')';
		frag.appendChild(el);

		el = document.createElement('a');
		el.setAttribute('href','#');
		YAHOO.util.Event.addListener(el,'click',removeAttachment,{id:o.attachments[i].id});
		var img = document.createElement('img');
		img.setAttribute('src','img/cbn/remove.gif');
		dom.addClass(img,'wap');
		el.appendChild(img);
		frag.appendChild(el);

		delim = ': ';
		comma = ',';
	    }

	    attachList.appendChild(frag);
	    dom.setStyle('composeAttachments','display','block');
	}
	else{
	    dom.setStyle('composeAttachments','display','none');
	    if(el) el.innerHTML = '';
	}
    }

    el = document.getElementById('attachments');
    if(el) el.setAttribute('value',idList);
}

function attachFile(e){
    var target = YAHOO.util.Event.getTarget(e);
    if(target.value.length){
	var elRemove = target.parentNode.parentNode;
	var elParent = elRemove.parentNode;
	YAHOO.util.Connect.setForm(target.parentNode,true);
	YAHOO.alpine.uploading++;
	YAHOO.util.Connect.asyncRequest('POST','conduit/attach.tcl', {
		upload: function(response){
		    YAHOO.alpine.uploading--;
		    elParent.removeChild(elRemove);
		}
	    });
    }
    else
	panelAlert('No File Attached');

    return false;
}

function addAttachField(){
    var event = YAHOO.util.Event;
    var dom = YAHOO.util.Dom;

    dom.setStyle('composeAttachments','display','block');

    var parentDiv = document.getElementById('fileUpload');
    if(parentDiv.childNodes.length < 20){
	var div = document.createElement('div');
	parentDiv.appendChild(div);
	dom.addClass(div,'attachInput');

	var form = document.createElement('form');
	div.appendChild(form);

	form.setAttribute('action','conduit/attach.tcl');
	form.setAttribute('method','POST');
	form.setAttribute('enctype','multipart/form-data');
	form.setAttribute('target','formResponse');
	dom.generateId(form);

	var input = createInputElement('file','file');
	form.appendChild(input);
	input.setAttribute('accept','*/*');
	event.addListener(input,'change',attachFile);

	/*
	var el = createInputElement('button','Attach')
	div.appendChild(el);
	el.setAttribute('value','Attach');
	event.addListener(el,'click',attachFile);
	*/	
    }

    return false;
}

function removeAttachment(e,o){
    var form = document.createElement('form');
    this.parentNode.appendChild(form);
    YAHOO.util.Dom.setStyle(form,'display','none');
    form.setAttribute('action','conduit/attach.tcl');
    form.setAttribute('method','GET');
    form.setAttribute('target','formResponse');

    var input = createInputElement('hidden','op');
    input.setAttribute('value','delete');
    form.appendChild(input);

    input = createInputElement('hidden','id');
    input.setAttribute('value',o.id);
    form.appendChild(input);

    form.submit();
    
    YAHOO.util.Event.preventDefault(e);
}

function setCursorPosition(inputId, cursorOffset) {
    var inputEl = document.getElementById(inputId);
    if(inputEl){
	if (inputEl.setSelectionRange) {
	    inputEl.focus();
	    inputEl.setSelectionRange(cursorOffset, cursorOffset);
	} else if (inputEl.createTextRange) {
	    var range = inputEl.createTextRange();
	    range.collapse(true);
	    range.moveEnd('character', cursorOffset);
	    range.moveStart('character', cursorOffset);
	    range.select();
	}
    }
}

function pickFccDone(o){
    updateElementValue('fieldFcc', o.f);
    return false;
}

function expandAddress(expObj){
    var eUrl = 'conduit/expand.tcl?';
    if(expObj.book){
	eUrl += 'book=' + expObj.book;
	eUrl += '&index=' + expObj.ai;
    }
    else if(expObj.addrs){
	eUrl += 'addrs=' + encodeURIComponent(expObj.addrs);
    }
    else
      return;

    var expandDS = new YAHOO.util.DataSource(eUrl);
    expandDS.responseType = YAHOO.util.DataSource.TYPE_XML;
    expandDS.responseSchema = {
	resultNode: 'Result',
	fields: ['Error','Address','Fcc']
    };
    expandDS.sendRequest('',
			 {
			     success: function(oReq,oResp,oPayload){
				 var errs = false;
				 if(expObj.addrs && oResp.results[0] && !oResp.results[0].Error) updateElementValue(expObj.fieldId, '');
				 for(var i = 0; i < oResp.results.length; i++){
				     if(oResp.results[i].Error){
					 addStatusMessage('Address Error: '+oResp.results[i].Error, 10);
					 errs++;
				     }
				     else if(oResp.results[i].Address) appendAddress(expObj.fieldId, oResp.results[i].Address);
				 }
				 if(errs) displayStatusMessages();
			     },
			     failure: function(oReq,oResp,oPayload){
				 showStatusMessage('Error expanding Field: ' + oResp.responseText, 10);
			     },
			     scope: this,
			     argument:[expObj]
			 });
    
}

function donePickContact(fieldId){
    switch (YAHOO.alpine.pickcontact.tabs.get('activeIndex')){
    case 0 :
	var l = document.getElementsByName('nickList');
	if(l){
	    var i, ai = contactsChecked('Add');
	    for(i in ai) expandAddress({book:ai[i].book,ai:ai[i].index,fieldId:fieldId});
	}
	break;
    case 1 :
	var l = document.getElementsByName('qrList');
	if(l){
	    var i, el, addrs = '';
	    for(i = 0; i < l.length; i++){
		if(l[i].checked){
		    if(addrs.length) addrs += ', ';
		    el = document.getElementById('qrPers'+l[i].value);
		    if(el){
			var specials = el.innerHTML.match(/[()<>@,;:\\\".\[\]]/);
			if(specials) addrs += '"' + el.innerHTML + '"';
			else addrs += el.innerHTML;
		    }
		    el = document.getElementById('qrAddr'+l[i].value);
		    if(el) addrs += ' <' + el.innerHTML + '>';
		}
	    }

	    appendAddress(fieldId,addrs);
	}
	break;
    }
}

function appendAddress(fieldId, addrs){
    if(addrs.length){
	var el = document.getElementById(fieldId);
	if(el){
	    if(el.value.length) el.value += ', ';
	    el.value += addrs;
	}
    }
}

function validField(o){
    expandAddress({addrs:o.value,fieldId:o.id});
}

function pickContact(fieldName, fieldId, objParm, markUp){
    var dom = YAHOO.util.Dom;
    var alp = YAHOO.alpine.pickcontact;
    if (alp.markup){
	var div = document.createElement('div');
	div.innerHTML = alp.markup;
	document.body.appendChild(div);

	var contactDialog = document.getElementById('contactDialog');
	if(contactDialog){
	    alp.tabs = new YAHOO.widget.TabView('contactDialog');
	    alp.tabs.subscribe('activeTabChange',
			      function(e){
				   if(alp.tabs.getTabIndex(e.newValue) == 1){
				       boxChecked(null); // set action button
				       var el = document.getElementById('dirQuery');
				       el.focus();
				       el.select();
				   }
			      });
	}
	else contactDialog = document.getElementById('contactList');

	var objDone = {
	    label:'Add ' + fieldName + ' Address',
	    disabled: true,
	    fn: function(){ donePickContact(fieldId); }
	};

	panelDialog('Add ' + fieldName + ' Address', contactDialog, objDone);
	setPanelBodyWidth('clistContacts');
	drawContactList('contactList',0,objParm);
    }

    return false;
}


function drawLDAPResult(objResult){
    var dom = YAHOO.util.Dom;
    var el = document.getElementById('dirResult');
    if(el){
	var elResult;
	if(objResult.error){
	    elResult = document.createElement('span');
	    elResult.innerHTML = objResult.error;
	}
	else if(objResult.results){
	    var o, n, t, qrPers, elTR, elTD, elCB, resId, elLabel;
	    elResult = document.createElement('table');
	    elResult.setAttribute('width','100%');
	    elResult.setAttribute('cellSpacing','0');
	    elResult.setAttribute('cellPadding','0');
	    for(var i = 0; i < objResult.results.length; i++){
		o = objResult.results[i];
		n = (o.email) ? o.email.length : 0;
		if(n){
		    elTR = elResult.insertRow(elResult.rows.length);
		    elTD = elTR.insertCell(elTR.cells.length);

		    elCB = createInputElement('checkbox','qrList');
		    elCB.setAttribute('value',elResult.rows.length);
		    resId = 'dirEnt' + elResult.rows.length;
		    elCB.setAttribute('id',resId);

		    YAHOO.util.Event.addListener(elCB,'click',boxClicked);
		    elTD.appendChild(elCB);

		    elTD = elTR.insertCell(elTR.cells.length);
		    dom.addClass(elTD,'wap');
		    if(o.personal){
			qrPers = o.personal;
		    }
		    else
			qrPers = '';

		    elLabel = createNameValueElement('label','for',resId);
		    elTD.appendChild(elLabel);
		    elLabel.setAttribute('id','qrPers'+elResult.rows.length);
		    elLabel.innerHTML = o.personal;


		    for(var j = 0; j < n; j++){
			if(j > 0){
			    elTR = elResult.insertRow(elResult.rows.length);
			    elTD = elTR.insertCell(elTR.cells.length);
			    elCB = createInputElement('checkbox','qrList');
			    elCB.setAttribute('value',elResult.rows.length);
			    resId = 'dirEnt' + elResult.rows.length;
			    elCB.setAttribute('id',resId);

			    YAHOO.util.Event.addListener(elCB,'click',boxClicked);
			    elTD.appendChild(elCB);
			    elTD = elTR.insertCell(elTR.cells.length);
			    dom.addClass(elTD,'wap');


			    elLabel = createNameValueElement('label','for',resId);
			    elTD.appendChild(elLabel);
			    elLabel.setAttribute('id','qrPers'+elResult.rows.length);
			    elLabel.innerHTML = qrPers;
			}
			elTD = elTR.insertCell(elTR.cells.length);
			dom.addClass(elTD,'wap');

			elLabel = createNameValueElement('label','for',resId);
			elTD.appendChild(elLabel);
			elLabel.setAttribute('id','qrAddr'+elResult.rows.length);
			elLabel.innerHTML = o.email[j];
		    }
		}
	    }
	}

	el.replaceChild(elResult,el.firstChild);
	el = document.getElementById('dirQuery');
	if(el) el.focus();
    }
}

function boxClicked(e){
    boxChecked(YAHOO.util.Event.getTarget(e));
}

function boxClear(){
    var l = (YAHOO.alpine.pickcontact.tabs.get('activeIndex') == 0) ? 'nickList' : 'qrList';
    var el = document.getElementsByName(l);
    if(el){
	for(var i = 0; i < el.length; i++) el[i].checked = false;
    }

    panelDialogEnableButton(false);
}

function boxChecked(o){
    if(o) markOne(o);
    var l = (YAHOO.alpine.pickcontact.tabs.get('activeIndex') == 0) ? 'nickList' : 'qrList';
    var el = document.getElementsByName(l);
    if(el){
	for(var i = 0; i < el.length; i++){
	    if(el[i].checked){
		panelDialogEnableButton(true);
		return;
	    }
	}
    }

    panelDialogEnableButton(false);
}

function setPriority(elClick,priority){
    var el, pos, dom = YAHOO.util.Dom;
    for(var i = 0; i <= 5; i++){
	el = document.getElementById('pri'+i);
	if(dom.hasClass(el.firstChild,'spfcl3')){
	    dom.removeClass(el.firstChild,'spfcl3');
	    dom.addClass(el.firstChild,'blank');
	    break;
	}
    }

    dom.removeClass(elClick.firstChild,'blank');
    dom.addClass(elClick.firstChild,'spfcl3');
    el = document.getElementById('priority');
    el.value = priority;
    elClick.blur();
    return false;
}

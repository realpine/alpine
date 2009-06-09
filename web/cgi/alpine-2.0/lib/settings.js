/* $Id: settings.js 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
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


function settingsPage(el){
    var dom = YAHOO.util.Dom;
    for(var i = 1; i <= 20; i++){
	dom.setStyle('settingsPage' + i,'display','none');
	var elPage = document.getElementById('Page' + i);
	if (elPage) dom.removeClass(elPage.parentNode,'sel');
    }

    dom.setStyle('settings' + el.id,'display','block');
    dom.addClass(el.parentNode,'sel');
    el.blur();
    return false;
}

function toggleAdvance(el){
    var dom = YAHOO.util.Dom;
    var s = dom.getStyle('advancedSettings','display');
    if(s && s == 'block'){
	dom.setStyle('advancedSettings','display','none');
	el.firstChild.src = 'img/cbn/f_plus.gif'
    }
    else{
	dom.setStyle('advancedSettings','display','block');
	el.firstChild.src = 'img/cbn/f_minus.gif'
    }

    el.blur();
    return false;
}

function validFieldName(e){
    var ev = YAHOO.util.Event;
    var cc = ev.getCharCode(e);
    if((cc >= 32 & cc < 127) && !(cc == 45 || (cc >= 65 && cc <= 90) || (cc >= 97 && cc <= 122))) ev.stopEvent(e);
}

function customHdrAdd(idTable){
    var frag = document.createDocumentFragment();
    var div = document.createElement('div');
    frag.appendChild(div);
    div.setAttribute('align','center');
    div.innerHTML = '<p>Add a new custom header by entering the new header name on the left and an optional value on the left<p>';

    div = document.createElement('div');
    frag.appendChild(div);
    div.setAttribute('align','center');

    var elName = createInputElement('text','field');
    div.appendChild(elName);
    elName.setAttribute('size','12');
    elName.setAttribute('maxlength','36');
    YAHOO.util.Event.addListener(elName,'keypress',validFieldName);
    YAHOO.util.Event.addListener(elName,'keyup',panelDialogInputChange);
    el = document.createTextNode(' : ');
    div.appendChild(el);
    var elValue = createInputElement('text','value');
    div.appendChild(elValue);
    elValue.setAttribute('size','40');

    panelDialog('New Custom Field',
		frag,
		{
		    label:' Add ',
		    disabled: true,
		    fn: function() {
			var elTD, elTR, elTable = document.getElementById(idTable);
			var el = document.getElementById('customHdrFields');
			var n = (el.value - 0) + 1;
			el.value = n;
			if(elTable && elName.value.length){
			    elTR = elTable.insertRow(elTable.rows.length);

			    elTD = elTR.insertCell(elTR.cells.length);
			    el = document.createTextNode(elName.value +':');
			    elTD.appendChild(el);
			    el = createInputElement('hidden','customHdrField'+n);
			    elTD.appendChild(el);
			    el.setAttribute('value',elName.value);

			    elTD = elTR.insertCell(elTR.cells.length);
			    el = createInputElement('text','customHdrData' + n);
			    elTD.appendChild(el);
			    el.setAttribute('value',elValue.value);
			    el.setAttribute('size','45');

			    el = document.createElement('a');
			    elTD.appendChild(el);
			    el.setAttribute('href','#');
			    YAHOO.util.Event.addListener(el,'click',removeTableRowEvent);
			    var img = document.createElement('img');
			    img.setAttribute('src','img/cbn/remove.gif');
			    YAHOO.util.Dom.addClass(img,'wap');
			    el.appendChild(img);
			}
		    }
		});

    elName.focus();
    return false;
}

function removeTableRow(el){
    if(el){
	var iRow = el.parentNode.parentNode.rowIndex;
	el.parentNode.parentNode.parentNode.parentNode.deleteRow(iRow);
    }
    return false;
}

function removeTableRowEvent(e){
    var ev = YAHOO.util.Event;
    removeTableRow(ev.getTarget(e).parentNode);
    ev.stopEvent(e);
}

function listAdd(idTable,listName){
    var elTD, elTR, elTable = document.getElementById(idTable);
    var el = document.getElementById(listName+'s');
    var n = el.value - 0;
    el.value = ++n;
    if(elTable.rows.length < 12){
	if(elTable){
	    elTR = elTable.insertRow(elTable.rows.length);
	    elTD = elTR.insertCell(elTR.cells.length);

	    el = createInputElement('text',listName + n);
	    elTD.appendChild(el);
	    el.setAttribute('size','45');

	    el = document.createElement('a');
	    elTD.appendChild(el);
	    el.setAttribute('href','#');
	    YAHOO.util.Event.addListener(el,'click',removeTableRowEvent);
	    var img = document.createElement('img');
	    img.setAttribute('src','img/cbn/remove.gif');
	    YAHOO.util.Dom.addClass(img,'wap');
	    el.appendChild(img);
	}
    }

    return false;
}

function saveSettings(){
    showLoading();
    document.getElementById('settingsForm').submit(); 
    return false;
}

function restoreDefaultSettings(){
    var el = document.getElementById('restore');
    if(el){
	el.value = 'restore';
	showLoading();
	document.getElementById('settingsForm').submit();
    }
    return false;
}

function settingsSuccess(){
    var alp = YAHOO.alpine;
    window.location.href = alp.cgi_base + 'browse/' + alp.current.c + '/' + alp.current.f;
}

function settingsFailure(m){
    hideLoading();
    addStatusMessage('Settings Failure: ' + m);
    displayStatusMessages();
}

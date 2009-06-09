/* $Id: mailbox.js 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
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

/*
 * Scripts common to mailbox viewing: browse and view
 */

// Globals
YAHOO.alpine.morcButton = [];
YAHOO.alpine.select = { all:false, bannerId:'bannerSelection' };


// BROWSE FUNCTIONS

/* priority column display is overloaded.  Uses el.title to store state. */
/* changes to link titles in that column MUST be coordinated with this stuff. */
/* Tricky */
var gLabelStarred = 'Starred';
var gLabelPriHi = 'High Priority';
var gLabelPriHier = 'Highest Priority';


function applyFlag(ctrlObj,flagName,flagState) {
    switch (flagName) {	  // validate
      case 'new' : break; // read (New) flag
      case 'imp' : break; // important flag
      case 'del' : break; // delete flag
      default	 : return false;
    }
    YAHOO.util.Connect.asyncRequest('GET',
				    'conduit/apply.tcl' + encodeURI('?u=all&f=' + flagName + '&s=' + flagState) + urlSalt('&'),
				    {
					customevents: {
					    onStart:function(eventType){ showLoading(); },
					    onComplete:function(eventType){ hideLoading(); }
					},
					success: function(o) {
					    // valid response is "<numApplied> <numSelected> <numTotalMsgs>"
					    var matchResult;
					    if((matchResult = o.responseText.match(/^(\d+) (\d+) (\d+)$/m)) == null){
						showStatusMessage('Error: ' + o.responseText, 10);
					    }
					    else{
						var plural = (matchResult[1] > 1) ? 's' : '';
						if(flagName == 'del'){
						    showStatusMessage(matchResult[1] + ' message' + plural + ' deleted', 3);
						    YAHOO.alpine.current.selected = matchResult[2];
						    YAHOO.alpine.current.count = matchResult[3];
						    hideSelectAllInfo();
						}
						else{
						    var chk = document.getElementsByName('uidList');
						    var sm;
						    if(flagName == 'imp'){
							var act = (flagState == 'not') ? 'Cleared' : 'Set';
							sm = act + ' Star on ' + matchResult[1] + ' message' + plural;
						    }
						    else{
							sm = 'Marked ' + matchResult[1] + ' message' + plural + ' as ';
							if(flagState == 'not'){
							    decUnreadCount(matchResult[1]);
							    sm += 'Read';
							}
							else{
							    incUnreadCount(matchResult[1]);
							    sm += 'Unread';
							}
						    }
						    showStatusMessage(sm, 3);
						    for(var i = 0; i < chk.length; i++){
							if(chk[i].checked){
							    var dom = YAHOO.util.Dom;
							    if(flagName == 'imp'){
								var offset = 0;
								var m = document.getElementById('star'+chk[i].value);
								if(m){
								    var classAdd = '', classDel = '', newtitle = '';
								    if(flagState == 'not'){
									classAdd = 'nostar';
									classDel = 'star';
									if(m.title.substr(0,gLabelStarred.length) == gLabelStarred){
									    if(m.title.length > gLabelStarred.length){
										newtitle = m.title.substr(gLabelStarred.length + 5);
										if(newtitle == gLabelPriHi){
										    classAdd = 'prihi';
										}
										else if(newtitle == gLabelPriHier){
										    classAdd = 'prihier';
										}
									    }
									    else{
										newtitle = 'Set ' + gLabelStarred;
									    }
									}
								    }
								    else{
									classAdd = 'star';
									switch (m.title) {
									case 'Set ' + gLabelStarred :
									    newtitle = gLabelStarred;
									    classDel = 'nostar';
									    break;
									case gLabelPriHi :
									    classDel = 'prihi';
									    newtitle = gLabelStarred + ' and ' + gLabelPriHi;
									    break;
									case gLabelPriHier :
									    classDel = 'prihier';
									    newtitle = gLabelStarred + ' and ' + gLabelPriHier;
									    break;
									default :
									    break;
									}
								    }

								    if(!dom.hasClass(m.firstChild,classAdd)) dom.addClass(m.firstChild,classAdd);
								    if(classDel.length && dom.hasClass(m.firstChild,classDel)) dom.removeClass(m.firstChild,classDel);
								    if(newtitle.length) m.title = newtitle;
								}
							    }
							    else if(flagName == 'new'){
								var hid = 'h1'+chk[i].value;
								var mnode = chk[i].parentNode.parentNode;
								if(flagState == 'not'){
								    if(dom.hasClass(hid,'unread')) dom.removeClass(hid,'unread');
								    if(dom.hasClass(mnode,'unread')) dom.removeClass(mnode,'unread');
								}
								else{
								    if(!dom.hasClass(hid,'unread')) dom.addClass(hid,'unread');
								    if(!dom.hasClass(mnode,'unread')) dom.addClass(mnode,'unread');
								}
							    }
							}
						    }
						    showSelectAllInfo();
						}
					    }
					},
					failure: function(o) { showStatusMessage('Request Failure: ' + o.statusText + '. Please report this.', 10)}
				    });

    if(ctrlObj) ctrlObj.blur();
    return false;
}


//
// SELECTION (e.g. SELECT ALL)
//

function initSelection() {
    var ac = YAHOO.alpine.current;
    if (ac.selected > 0) {
	if (ac.selected == ac.count) {
	    showSelectAllInfo(4);
	} else if (ac.selected) {
	    showSelectAllInfo(2);
	}
    }
    else hideSelectAllInfo();
}

function hideSelectAllInfo() {
    var banner = document.getElementById(YAHOO.alpine.select.bannerId);
    if(banner) YAHOO.util.Dom.setStyle(banner,'display','none');
}

function showSelectAllInfo(num) {
    var banner = document.getElementById(YAHOO.alpine.select.bannerId);
    if (banner){
	setSelectAllInfoText(num);
	YAHOO.util.Dom.setStyle(banner,'display','block');
    }
}

function setSelectAllInfoText(state) {
    // NOTE: "\u00a0" is the unicode equivalent of no-break space character
    var ac = YAHOO.alpine.current;
    var banner = document.getElementById(YAHOO.alpine.select.bannerId);
    if (!banner) return;
    var msg;
    var pluralSelected = (ac.selected > 1) ? 's' : '';
    var predSelected = (ac.selected > 1) ? 'are' : 'is';
    var pluralTotal = (ac.count > 1) ? 's' : '';
    var groupName = (ac.focused) ? 'Search Results' : ac.f;
    switch (state) {
		case 1:
			banner.innerHTML = "All <b>" + ac.selected + "<\/b> message" + pluralSelected + " in " + groupName + " " + predSelected + " selected.";
			banner.style.backgroundColor = "#ffffa6";
		break;
		case 2:
			banner.innerHTML = "<b>" + ac.selected + "<\/b> message" + pluralSelected + " in " + groupName + " " + predSelected + " selected. \u00a0\u00a0\u00a0\u00a0\u00a0 [<a href='browse/" + ac.c + "/" + ac.f + "/" + ac.u + "?select=all' onClick='return selectAllInFolder();'>Select All <b>" + ac.count + " <\/b> Message" + pluralTotal + " in " + groupName + "<\/a>] \u00a0\u00a0 [<a href='browse/" + ac.c + "/" + ac.f + "/" + ac.u + "?select=none' onClick='return unSelectAllInFolder();'>Unselect All<\/a>]";
			banner.style.backgroundColor = "#ffffa6";
		break;
		case 3:
			banner.innerHTML = "All <b>" + ac.selected + "<\/b> message" + pluralSelected + " on this page " + predSelected + " selected. \u00a0\u00a0\u00a0\u00a0\u00a0 [<a href='browse/" + ac.c + "/" + ac.f + "/" + ac.u + "?select=all' onClick='return selectAllInFolder();'>Select All <b>" + ac.count + " <\/b> Message" + pluralTotal + " in " + groupName + "<\/a>]";
			banner.style.backgroundColor = "#ffffa6";
		break;
		case 4:
			banner.innerHTML = "<b>All <u>" + ac.count + "<\/u> message" + pluralTotal + " in " + groupName + " " + predSelected + " selected.<\/b> \u00a0\u00a0\u00a0\u00a0\u00a0 [<a href='browse/" + ac.c + "/" + ac.f + "/" + ac.u + "?select=none' onClick='return unSelectAllInFolder();'>Unselect All<\/a>]";
			banner.style.backgroundColor = "#ffdebf";
		break;
		case 5:
			banner.innerHTML = "<b>Selection Cleared - No messages selected in " + groupName + ".<\/b>";
			banner.style.backgroundColor = "#ffdebf";
		break;
	}
}


function markMessage(msg) {
    var ac = YAHOO.alpine.current;
    var chk = document.getElementsByName('uidList');
    var selectAll = document.getElementById('selectall');
    var numCheckbox = isNaN(chk.length) ? 1 : chk.length;
    msg.parentNode.parentNode.id = msg.checked ? "sd" : "";
    if (YAHOO.alpine.select.all) {
	YAHOO.alpine.select.all = false;
    }
    if (!msg.checked) {
	--ac.selected;
	if (ac.selected) {
	    setSelectAllInfoText(2);
	} else {
	    hideSelectAllInfo();
	}
	selectAll.checked = false;
    } else {
	++ac.selected;
	if (ac.count <= numCheckbox) {
	    setSelectAllInfoText(1);
	} else {
	    setSelectAllInfoText(2);
	}
    }
    updateMessageState(msg.value, msg.checked);
}

function markAllMessages() {
    var ac = YAHOO.alpine.current;
    var chk = document.getElementsByName('uidList');
    var isChecked = document.getElementById('selectall').checked;
    var numChanged = 0;
    var numCheckbox;
    var markedUidList = '';
    if (chk) {
	hideDisplayDivOrSpan("bannerConfirm");	// remove any confirmation banner from page
	numCheckbox = isNaN(chk.length) ? 1 : chk.length;
	if (numCheckbox == 1) {	// if only one checkbox
	    chk.parentNode.parentNode.id = isChecked ? "sd" : "";
	    if (chk.checked != isChecked) {
		numChanged = 1;
	    }
	    chk.checked = isChecked;
	    markedUidList = chk.value;
	} else {
	    var selectedClass = isChecked ? "sd" : "";
	    for (var i = 0; i < chk.length; i++) {
		if (chk[i].checked != isChecked) {
		    ++numChanged;
		}
		chk[i].checked = isChecked;
		chk[i].parentNode.parentNode.id = selectedClass;
		markedUidList += ',' + chk[i].value;
	    }
	}
	if (isChecked) {	// if Select all
	    ac.selected += numChanged;
	    if (ac.selected > numCheckbox) {
		showSelectAllInfo(2);
	    } else if (ac.count > numCheckbox) {
		showSelectAllInfo(3);
	    }
	} else {		// if UNselect all
	    ac.selected -= numChanged;
	    if (ac.selected) {
		showSelectAllInfo(2);
	    } else {
		hideSelectAllInfo();
	    }
	}
	updateMessageState(markedUidList, isChecked);
    }
}

function selectAllInFolder() {
    var alp = YAHOO.alpine;
    var chk = document.getElementsByName('uidList');
    document.getElementById('selectall').checked = true;
    var numCheckbox;
    if (chk) {
	hideDisplayDivOrSpan("bannerConfirm");	// remove any confirmation banner from page
	numCheckbox = isNaN(chk.length) ? 1 : chk.length;
	if (numCheckbox == 1) {
	    chk.checked = true;
	    chk.parentNode.parentNode.id = "sd";
	} else {
	    for (var i = 0; i < chk.length; i++) {
		chk[i].checked = true;
		chk[i].parentNode.parentNode.id = "sd";
	    }
	}

	var all = (alp.current.focused) ? 'searched' : 'all';
	alp.current.selected = (alp.current.focused) ? alp.current.focused : alp.current.count;
	updateMessageState(all,'true');
    }
    showSelectAllInfo(4);
    YAHOO.alpine.select.all = (alp.current.focused == 0);
    return false;
}

function unSelectAllInFolder() {
    var alp = YAHOO.alpine;
    var chk = document.getElementsByName('uidList');
    var numCheckbox;
    if (chk) {
	numCheckbox = isNaN(chk.length) ? 1 : chk.length;
	YAHOO.alpine.current.selected = 0;
	// setSelectAllInfoText(5);
	document.getElementById('selectall').checked = false;
	if (numCheckbox == 1) {
	    chk.checked = false;
	    chk.parentNode.parentNode.id = "";
	} else {
	    for (var i = 0; i < chk.length; i++) {
		chk[i].checked = false;
		chk[i].parentNode.parentNode.id = "";
	    }
	}

	var all = (alp.current.focused) ? 'searched' : 'all';
	alp.select.all = false;
	updateMessageState(all,'false');
    }
    hideSelectAllInfo();
    return false;
}

function numCheckedOnPage() {
    var chk = document.getElementsByName('uidList');
    var numCheckbox;
    var numChecked=0;
    if (chk) {
	numCheckbox = isNaN(chk.length) ? 1 : chk.length;
	if ((numCheckbox==1) && (chk.checked)) {
	    numChecked = 1;
	} else {
	    for (var i = 0; i < chk.length; i++) {
		if (chk[i].checked) {
		    ++numChecked;
		}
	    }
	}
    }

    return numChecked;
}


// Communicate state changes to server
function updateMessageState(u,m){
    YAHOO.util.Connect.asyncRequest('GET',
				    'conduit/mark.tcl' + encodeURI('?u=' + u + '&mark=' + m) + urlSalt('&'),
				    {
					success: function(o) {},
					failure: function(o) {
					    newStatusMessage('Request Failure: ' + o.statusText);
					}
				    });
}

// Mailbox Search
function mailboxSearch(){
    var elField = document.getElementById('searchField');
    if(elField && elField.value && elField.value.length){
	var elScope = document.getElementById('searchScope');
	var scope = (elScope) ? elScope.options[elScope.selectedIndex].value : 'new';
	newMessageList({parms:{op:'search',type:'any',scope:scope,criteria:elField.value}});
    }
}

// Confirm Toolbar Actions based on Selection
function confirmDelete() {
    var ac = YAHOO.alpine.current;
    var plural = (ac.selected > 1) ? 's' : '';
    if (ac.selected != numCheckedOnPage())
	panelConfirm("Move " + ac.selected + " message" + plural + " from " + ac.f + " to the Trash?<p>Some selected messages are on other pages.",{fn:actuallyDelete},'Delete');
    else
	actuallyDelete();

    return(false);
}

function confirmSpam() {
    var ac = YAHOO.alpine.current;
    var plural = (ac.selected > 1) ? 's' : '';
    if (ac.selected != numCheckedOnPage())
	panelConfirm("Report " + ac.selected + " message" + plural + " as Spam?<p>Some selected messages are on other pages.",{fn:actuallySpam},'Report Spam');
    else
	actuallySpam();

    return(false);
}

// Confirm Toolbar Actions based on Selection
function deleteForeverString() {
    if (YAHOO.alpine.current.selected != numCheckedOnPage()) {
	return confirm( + " forever?\nSome selected messages are on other pages.");
    }

    return(YAHOO.alpine.current.selected > 0);
}

// Complain if  no messages are selected  for Toolbar Action
function anySelected(act) {
   if (YAHOO.alpine.current.selected <= 0){
       panelAlert('No messages selected to ' + act + '.<p>Select one or more messages by checking the box on the line of each desired message.');
       return false;
   }

   return true;
}

function flipStar(el) {
    var dom = YAHOO.util.Dom;
    var u = el.id.substring(4);
    var flagState = '', iClass = '', iTitle = '', iUnClass = '';
    if(el.title.substr(0,7) == gLabelStarred){
	flagState = 'not';
	iUnClass = 'star';
	if(el.title.length > 7){
	    switch (el.title.substr(12)){
	    case gLabelPriHi :
		iClass = 'prihi';
		iTitle = gLabelPriHi;
		break;
	    case gLabelPriHier :
		iClass = 'prihier';
		iTitle = gLabelPriHier;
		break;
	    default :
		return false;
	    }
	}
	else{
	    iClass = 'nostar';
	    iTitle = 'Set '+ gLabelStarred;
	}
    }
    else{
	flagState = 'ton';
	iClass = 'star';
	switch(el.title){
	case 'Set ' + gLabelStarred :
	    iUnClass = 'nostar';
	    iTitle = gLabelStarred;
	    break;
	case gLabelPriHi :
	    iUnClass = 'prihi';
	    iTitle = gLabelStarred + ' and ' + gLabelPriHi;
	    break;
	case gLabelPriHier :
	    iUnClass = 'prihier';
	    iTitle = gLabelStarred + ' and ' + gLabelPriHier;
	    break;
	default : return false;
	}
    }

    setStar(u,flagState,function(){ if(iTitle.length) el.title = iTitle; if(iUnClass.length) dom.removeClass(el.firstChild,iUnClass); dom.addClass(el.firstChild,iClass); });
    return false;
}

function setStar(u,flagState,onDone){
    if(u < 0) u = YAHOO.alpine.current.u;
    YAHOO.util.Connect.asyncRequest('GET',
				    'conduit/flag.tcl?u=' + u + '&f=imp&s=' + flagState + urlSalt('&'),
				    {
					success: function(o) { if(onDone) onDone(); },
					failure: function(o) { showStatusMessage('Request Failure: ' + o.statusText + '. Please report this.', 10)}
				    });
    return false;
}

// Load next message list for viewing relative to current list
function newMessageList(o){
    var div = document.getElementById('alpineContent');
    if(div){
	var nUrl = 'newlist.tcl/' + YAHOO.alpine.current.c + '/' + encodeURIFolderPath(YAHOO.alpine.current.f);
	var conj = '?';
	if(o && o.parms){
	    for(var prop in o.parms){
		nUrl += conj + prop + '=' + encodeURIComponent(o.parms[prop]);
		conj = '&';
	    }
	}
	else{
	    nUrl += '?op=noop';
	    conj = '&';
	}

	YAHOO.util.Connect.asyncRequest('GET',
					nUrl + urlSalt(conj),
					{
					    customevents:{
						onStart:function(eventType){ showLoading(); },
						onComplete:function(eventType){ hideLoading(); }
					    },
					    success:function(aro){
						div.innerHTML = aro.responseText;
						evalScripts(aro.responseText);
					    },
					    failure: function(aro) { showStatusMessage('Request Failure: ' + aro.statusText, 10)},
					    argument: [div]
					});

	if(o && o.control && o.control.blur) o.control.blur();
    }
    return false;
}

function actuallyDelete()
{
    if(numCheckedOnPage()) newMessageList({parms:{'op':'delete'}});
    else applyFlag(null,'del','ton');
}

function actuallySpam()
{
    newMessageList({parms:{'op':'spam'}});
}

function doEmpty(ctrlObj,listOption){
    if(listOption == 'all'){
	newMessageList({control:ctrlObj,parms:{'op':'trashall'}});
    }
    else{
	var ac = YAHOO.alpine.current;
	if(listOption == 'selected' && anySelected('Delete Forever')){
	    var plural = (ac.selected > 1) ? 's' : '';
	    var t = "Delete " + ac.selected + " message" + plural + " from " + ac.f + ' forever?';
	    if(ac.selected != numCheckedOnPage()) t += '<p>Some selected messages are on other pages.';
	    panelConfirm(t,
			 {
			     text:'Delete Forever',
			     fn:function(){ newMessageList({control:ctrlObj,parms:{'op':'trash'}}); }
			 });
	}
    }

    if(ctrlObj) ctrlObj.blur();
    return false;
}

function doSpam(ctrlObj){
    if(anySelected('Report as Spam')) confirmSpam();
    if(ctrlObj) ctrlObj.blur();
    return false;
}

// Drag Drop Support
function hiliteDrop(id,on){
    var dom = YAHOO.util.Dom;
    if(on){
	if(!dom.hasClass(id,'drop'))
	    dom.addClass(id, 'drop');

	if(dom.hasClass(id + 'Icon','splc5')){
	    dom.removeClass(id + 'Icon','splc5');
	    dom.addClass(id + 'Icon','splc10');
	}
	else if(dom.hasClass(id + 'Icon','splc7')){
	    dom.removeClass(id + 'Icon','splc7');
	    dom.addClass(id + 'Icon','splc11');
	}
    }
    else{
	if(dom.hasClass(id,'drop'))
	    dom.removeClass(id, 'drop');

	if(dom.hasClass(id + 'Icon','splc10')){
	    dom.removeClass(id + 'Icon','splc10');
	    dom.addClass(id + 'Icon','splc5');
	}
	else if(dom.hasClass(id + 'Icon','splc11')){
	    dom.removeClass(id + 'Icon','splc11');
	    dom.addClass(id + 'Icon','splc7');
	}
    }
}

function dragOntoFolder(uid,o){
    newMessageList({parms:{'op':'movemsg','df':o.c+'/'+o.f,'uid':uid}});
    return false;
}

function canDragit(id,uid,tt){
    var dom = YAHOO.util.Dom;
    var dd = new YAHOO.util.DDProxy(id,
				    'message',
				    {
					dragElId:'msgDragProxy',
					resizeFrame: false
				    });
    dd.isTarget = false;
    dd.b4MouseDown = function(e){
				   var s = '<b>' + this.getEl().innerHTML + '</b>';
				   var r = dom.getRegion(this.getEl());
				   var w = r.right - r.left;
				   var el = document.getElementById('ml'+uid);
				   r = dom.getRegion(el);
				   w = Math.max(w, (r.right - r.left));
				   if(el && el.innerHTML.length) s += '<br>' + el.innerHTML;
				   el = document.getElementById('msgDragProxyText');
				   el.innerHTML = s;
				   dom.setStyle('msgDragProxy','width',w + 12);
				   return true;
				};
    dd.endDrag = function(){
			      dom.setStyle('msgDragProxy','visibility','hidden')
			   };
    dd.onDragEnter = function(e,id){ hiliteDrop(id,true); };
    dd.onDragOut = function(e,id){ hiliteDrop(id,false); };
    dd.onDragDrop = function(e,id){
				     hiliteDrop(id,false);
				     var o = dom.get(id);
				     if(o) o.f(uid,o.args);
				  }
    if(tt){
	dd.onMouseDown = function(e){
				       tt.hide();
				       tt.cfg.setProperty('disabled',true);
				    };
	dd.onMouseUp = function(e){
				     tt.cfg.setProperty('disabled',false);
				  };
    }
}

function setDragTarget(id,fHandler,oArgs){
    if(id && id.length){
	var o = document.getElementById(id);
	if(o){
	    o.ddObj = new YAHOO.util.DDTarget(id,'message');
	    o.f = fHandler;
	    o.args = oArgs;
	}
    }
}

function cursor(style){
    document.body.style.cursor = style;
}


// VIEW FUNCTIONS

function newMessageText(o){
    var div = document.getElementById('alpineContent');
    var ac = YAHOO.alpine.current;
    if(div){
	var uid = (o.uid) ? o.uid : ac.u;
	var nUrl = 'newview.tcl/' + ac.c + '/' + encodeURIFolderPath(ac.f) + '/' + uid;
	var conj = '?';
	if(o && o.parms){
	    for(var prop in o.parms){
		nUrl += conj + prop + '=' + encodeURIComponent(o.parms[prop]);
		conj = '&';
	    }
	}

	var aJax = YAHOO.util.Connect.asyncRequest('GET',
						   nUrl + urlSalt(conj),
						   {
						       customevents:{
							   onStart:function(eventType){ showLoading(); },
							   onComplete:function(eventType){ hideLoading(); }
						       },
						       success: function(aro){
							   div.innerHTML = aro.responseText;
							   evalScripts(aro.responseText);
							   window.scrollTo(0,0);
						       },
						       failure: function(aro) { showStatusMessage('newMessageText Failure: ' + aro.statusText,10); },
						       argument: [div]
						   });

	if(o && o.control && o.control.blur) o.control.blur();
    }
    return false;
}

// Move or Copy support

function initMorcButton(morcButton){
    if(YAHOO.alpine.morcButton[morcButton] == null){
	YAHOO.alpine.morcButton[morcButton] = new YAHOO.widget.Button({
		type:'menu',
		label:'Move',
		id:morcButton+'Choice',
		menu:[{ text:'Copy', value:'morc', onclick:{ fn:morcClick, obj:{morcButton:morcButton}} }],
		container:morcButton
	    });
    }
}

function morcWhich(morcButton){
    return YAHOO.alpine.morcButton[morcButton].get('label');
}

function morcClick(p_sType,p_aArgs,p_oItem){
    var morc, morcOpt;
    var button = YAHOO.alpine.morcButton[p_oItem.morcButton];
    if('copy' == YAHOO.util.Event.getTarget(p_aArgs[0]).innerHTML.toLowerCase()){
	morc = 'Copy';
	morcOpt = 'Move';
    }
    else{
	morc = 'Move';
	morcOpt = 'Copy';
    }

    button.set('label',morc);
    button.getMenu().getItem(0).cfg.setProperty('text',morcOpt);
}

function saveCacheClick(e,o){
    var event = YAHOO.util.Event;
    event.getTarget(e).blur();
    o.onDone(o);
    event.preventDefault(e);
}

function updateSaveCache(p,c,f,dc,od,sca){
    var event = YAHOO.util.Event;
    var li = document.getElementById(p+'SaveCache');
    if(li){
	var cn = li.childNodes;
	var i, j, node, s = '';
	for(i = 0; i < cn.length && i < sca.length; i++){
	    node = cn[i].childNodes[0];
	    if(node.tagName.toLowerCase() == 'hr') break;
	    node.href = p + '/' + c + '/' + f + '?save=' + dc + '/' + sca[i].fv;
	    node.innerHTML = sca[i].fn.replace(/</g,'&lt;');
	    event.removeListener(node,'click');
	    event.addListener(node,'click',saveCacheClick,{c:dc,f:sca[i].fv,onDone:od});
	}
	for(j = i; j < sca.length; j++){
	    var nli = document.createElement('li');
	    var a = document.createElement('a');
	    a.href = p + '/' + c + '/' + f + '?save=' + dc + '/' + sca[j].fv;
	    a.innerHTML = sca[j].fn.replace(/</g,'&lt;');
	    event.addListener(a,'click',saveCacheClick,{c:dc,f:sca[j].fv,onDone:od});
	    nli.appendChild(a);
	    li.insertBefore(nli,cn[i++]);
	}
    }
}


// Move or Copy in mailbox list browse
function morcInBrowseDone(o){
    if(anySelected('Move or Copy')){
	var mvorcp = morcWhich('listMorcButton').toLowerCase();
	newMessageList({parms:{'op':mvorcp,'df':o.c+'/'+o.f}});
    }

    return false;
}

// Move or Copy in mailbox message view
function morcInViewDone(o){
    var mvorcp = morcWhich('viewMorcButton').toLowerCase();
    newMessageText({parms:{'op':mvorcp,'df':o.c+'/'+o.f}});
    return false;
}


function takeAddress(){
    var dom = YAHOO.util.Dom;
    var dbody = document.createDocumentFragment();
    var div = document.createElement('div');
    dbody.appendChild(div);
    dom.addClass(div,'takeInstructions');
    div.innerHTML = 'Choose extracted addresses and save to a new Contact'

    div = document.createElement('div');
    dbody.appendChild(div);
    div.setAttribute('id','takeList');
    dom.addClass(div,'takeList');
    div.innerHTML = 'Loading...';

    panelDialog('Extract Addresses',
		dbody,
		{
		    buttonId:'butChoose',
		    label:'Add to Contacts',
		    disabled: true,
		    fn: function(){
			var o = { which:'add', nickname:'', personal:'', email:'', note:'', fcc:'' };
			var l = document.getElementsByName('taList');
			if(l){
			    var i, el;
			    for(i = 0; i < l.length; i++){
				if(l[i].checked){
				    if(o.email.length){
					o.email += ',\n';
					o.group = true;
				    }
				    el = document.getElementById('taPers'+l[i].value);
				    if(el){
					var specials = el.innerHTML.match(/[()<>@,;:\\\".\[\]]/);
					if(specials) o.email += '"' + el.innerHTML + '"';
					else o.email += el.innerHTML;
				    }
				    el = document.getElementById('taEmail'+l[i].value);
				    if(el) o.email += ' <' + el.innerHTML + '>';
				}
			    }

			    contactEditor(o);
			}
		    }
		});

    setPanelBodyWidth('takeList');
    drawExtractedAddresses(div);
    return false;
}

function drawExtractedAddresses(elContainer){
    var takeDS = new YAHOO.util.DataSource('conduit/take.tcl?op=all&u=');
    takeDS.responseType = YAHOO.util.DataSource.TYPE_XML;
    takeDS.responseSchema = {
	resultNode: 'Result',
	fields: ['Type', 'Nickname','Personal','Email','Fcc','Note','Error']
    };
    takeDS.sendRequest(YAHOO.alpine.current.u,
		       {
			   success: function(oReq,oResp,oPayload){
			       var dom = YAHOO.util.Dom;
			       if(oResp.results.length == 1 && oResp.Error){
				   showStatusMessage('Error Taking Address: ' + oResp.Error, 10);
			       }
			       else {
				   var id, el, elTD, elTR, elTable = document.createElement('table');
				   elTable.setAttribute('width','100%');
				   elTable.setAttribute('cellSpacing','0');
				   elTable.setAttribute('cellPadding','0');
				   for(var i = 0; i < oResp.results.length; i++){
				       var o = oResp.results[i];
				       n = (o.Email) ? o.Email.length : 0;
				       if(n){
					   elTR = elTable.insertRow(elTable.rows.length);
					   elTD = elTR.insertCell(elTR.cells.length);

					   el = createInputElement('checkbox','taList');
					   el.setAttribute('value',elTable.rows.length);
					   id = 'takeAddr' + elTable.rows.length;
					   el.setAttribute('id',id);

					   YAHOO.util.Event.addListener(el,'click',boxClicked);
					   elTD.appendChild(el);

					   elTD = elTR.insertCell(elTR.cells.length);
					   dom.addClass(elTD,'wap');
					   el = createNameValueElement('label','for',id);
					   elTD.appendChild(el);
					   el.setAttribute('id','taPers'+elTable.rows.length);
					   el.innerHTML = (o.Personal) ? o.Personal : '';

					   elTD = elTR.insertCell(elTR.cells.length);
					   dom.addClass(elTD,'wap');
					   el = createNameValueElement('label','for',id);
					   elTD.appendChild(el);
					   el.setAttribute('id','taEmail'+elTable.rows.length);
					   el.innerHTML = o.Email;
				       }

				       //if(oResp.results[i].Nickname) ;
				       //if(oResp.results[i].Note) ;
				       //if(oResp.results[i].Fcc) ;
				   }

				   elContainer.replaceChild(elTable, elContainer.firstChild);
			       }
			   },
			   failure: function(oReq,oResp,oPayload){
			       showStatusMessage('Error Taking Address: ' + oResp.responseText, 10);
			   },
			   scope: this,
			   argument:[elContainer]
		       });
    return(false);
}

function boxClicked(e){
    var o = YAHOO.util.Event.getTarget(e);

    if(o) markOne(o);
    var el = document.getElementsByName('taList');
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

function boxClear(){
    var el = document.getElementsByName('taList');
    if(el){
	for(var i = 0; i < el.length; i++) el[i].checked = false;
    }

    panelDialogEnableButton(false);
}

// focus message list on search results
function listSearchResults(){
    newMessageList({parms:{op:'focus',page:'new'}});
    return false;
}

function fixupUnreadCount(id,n) {
    var idObj;
    if(n) updateElementHtml('unread' + id + 'Count', n);
    else updateElementHtml('unread' + id, '');
}


// Pagewise Context updaters
function showBrowseMenus(){
    var dom = YAHOO.util.Dom;
    dom.setStyle('viewTopMenubar','display','none');
    dom.setStyle('listTopMenubar','display','block');
    dom.setStyle('viewBottomMenubar','display','none');
    dom.setStyle('listBottomMenubar','display','block');
}

function updateBrowseLinksAndSuch(o){
    var dom = YAHOO.util.Dom;
    var alp = YAHOO.alpine;
    if(o.u) alp.current.u = o.u;
    alp.current.count = o.count;
    alp.current.selected = o.selected;
    alp.current.searched = o.searched;
    alp.current.focused = o.focused;
    if(o.selected != numCheckedOnPage()) showSelectAllInfo(2);
    if(!o.page || o.page < 1) o.page = 1;
    if(!o.pages || o.pages < 1) o.pages = 1;
    document.title = alp.current.f + ', ' + o.page + ' of ' + o.pages + ' (' + o.unread + ') - Web Alpine 2.0';
    fixupUnreadCount('Current',o.unread);
    updateElementHtml('listContext', 'Page ' + o.page + ' of ' + o.pages + ' &nbsp;(' + o.count + ' Total Messages)&nbsp;');
    var bUrl = 'browse/' + alp.current.c + '/' + alp.current.f;
    updateElementHref('gFolder', bUrl);
    updateElementHref('composeLink', 'compose?pop=' + bUrl);
    if(o.sort){
	var ea = document.getElementsByTagName('span');
	for(var i = 0; i < ea.length; i++){
	    if(dom.hasClass(ea[i],'spfcl3')){
		dom.removeClass(ea[i],'spfcl3');
		break;
	    }
	}

	var el = document.getElementById('sort'+o.sort);
	if(el) dom.addClass(el.firstChild,'spfcl3')
    }
    if(o.searched > 0){
	dom.setStyle('searchResult','display','block');
	dom.setStyle('searchRefine','display','block');
	dom.setStyle('searchClear','display','block');
	if(o.focused > 0){
	    updateElementHtml('pageTitle', 'Search Results in ' + quoteHtml(alp.current.f));
	    updateElementHtml('searchResultText', 'Search Results (' + o.focused + ')');
	    if(!dom.hasClass('searchResult','sel')) dom.addClass('searchResult','sel');
	    var el = document.getElementById('gFolder');
	    if(el && dom.hasClass(el.parentNode,'sel')) dom.removeClass(el.parentNode,'sel');
	}
	else{
	    updateElementHtml('pageTitle', quoteHtml(alp.current.f));
	    if(dom.hasClass('searchResult','sel')) dom.removeClass('searchResult','sel');
	    var el = document.getElementById('gFolder');
	    if(el && !dom.hasClass(el.parentNode,'sel')) dom.addClass(el.parentNode,'sel');
	}
    }
    else{
	updateElementHtml('pageTitle', quoteHtml(alp.current.f));
	dom.setStyle('searchResult','display','none');
	if(dom.hasClass('searchResult','sel')) dom.removeClass('searchResult','sel');
	var el = document.getElementById('gFolder');
	if(el && !dom.hasClass(el.parentNode,'sel')) dom.addClass(el.parentNode,'sel');
	dom.setStyle('searchRefine','display','none');
	dom.setStyle('searchClear','display','none');
	document.getElementById('searchScope').selectedIndex = 2;
    }
    if(YAHOO.env.ua.gecko > 0){
	sizeVPHeight();
	window.onresize = resizeVPHeight;
    }
}

function showViewMenus(){
    var dom = YAHOO.util.Dom;
    dom.setStyle('listTopMenubar','display','none');
    dom.setStyle('viewTopMenubar','display','block');
    dom.setStyle('listBottomMenubar','display','none');
    dom.setStyle('viewBottomMenubar','display','block');
}

function updateViewLinksAndSuch(o){
    var alp = YAHOO.alpine;
    var ac = alp.current;
    if(o.u == 0){
	window.location = alp.app_root + '/browse/' + ac.c + '/' + ac.f;
	return;
    }

    ac.u = o.u;
    ac.count = o.count;
    ac.selected = o.selected;
    fixupUnreadCount('Current',o.unread);
    var thisFldr = '/' + ac.c  + '/' + ac.f + '/';
    var thisFldrUrl = '/' + ac.c + '/' + encodeURIComponent(ac.f).replace(/%2F/g,'/');
    var viewUrl = 'view' + thisFldr;
    var browseUrl = 'browse' + thisFldrUrl;
    var context = 'Message ' + o.n + ' of ' + o.count;
    var fullcontext = ac.f + ', ' + context;
    document.title = context + ' (' + o.unread + ') - Web Alpine 2.0';
    updateElementHtml('viewContext', context);
    updateElementHtml('pageTitle', '<a href="' + viewUrl + '" onClick="return newMessageList({parms:{op:\'noop\',page:\'new\'}});">' + quoteHtml(fullcontext) + '');
    updateElementHref('gFolder', browseUrl);
    updateElementHref('gCheck', viewUrl + ac.u);
    updateElementHref('gExtract', 'extract/' + ac.c + '/' + ac.f + '/' + ac.u);
    updateElementHref('gDelete', viewUrl + o.unext + '?delete=' + ac.u);
    updateElementHref('gSpam', viewUrl + o.unext + '?spam=' + ac.u);
    updateElementHref('gUnread', viewUrl + o.unext + '?unread=' + ac.u);
    updateElementHref('gDecrypt', viewUrl + ac.u);
    var compUrl = thisFldrUrl + '/' + ac.u + '?pop=view' + thisFldrUrl + '/' + ac.u;
    updateElementHref('gReply', 'reply' + compUrl);
    updateElementHref('gReplyAll', 'replyall' + compUrl);
    updateElementHref('gForward', 'forward' + compUrl);
    updateElementHref('composeLink', 'compose?pop=view' + compUrl);
    var oForm = document.getElementById('searchForm');
    oForm.action = oForm.action.replace(/\/[\d]+$/,'/' + ac.u);
    if(YAHOO.env.ua.gecko > 0){
	sizeVPHeight();
	window.onresize = resizeVPHeight;
    }
}

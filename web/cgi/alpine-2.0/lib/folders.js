/* $Id: folders.js 1150 2008-08-20 00:27:11Z mikes@u.washington.edu $
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


function doOpen(){
    var fce = document.getElementById('pickFolderCollection');
    var fc = (fce) ? fce.value : null;
    var fne = document.getElementById('pickFolderName');
    var fn = (fne && fne.value && fne.value.length) ? fne.value : null;
    if(fc && fn) location.href = 'browse/'+ fc + '/' + fn;
    else panelAlert('Which folder would you like to View?<p>Click a folder\'s name to select it, then click <b>View&nbsp;Messages</b>.');
    return false;
}

function doDelete(o){
    var el = document.getElementById('alpineContent');
    newFolderList(el,null,o.c,o.path,{op:'delete',f:o.folder});
    var fne = document.getElementById('pickFolderName');
    if(fne) fne.value = '';
    var base, link;
    if((base = document.getElementsByTagName('base')) && base.length) link = base[0].getAttribute('href');
    else link = '';
    link += 'browse/'+o.c+'/'+o.path+encodeURIComponent(o.folder);
    for(var i = 0; i < 20 ; i++){// 20 > fldr_cache_max
	var target = 'target' + i;
	el = document.getElementById(target);
	if(!el) break;
	if(el.firstChild.href == link){
	    YAHOO.util.Dom.setStyle(target,'display','none');
	    break;
	}
    }
}

function queryDelete(){
    if(YAHOO.alpine.current.incoming){
	showStatusMessage('Cannot delete Incoming Folders yet',5);
	return false;
    }

    var fc = document.getElementById('pickFolderCollection').value;
    var fp = document.getElementById('pickFolderPath').value;
    var fn = document.getElementById('pickFolderName').value;
    if(fn.length) panelConfirm('Are you sure you want to permanently delete the folder <b>' + fn + '</b> and its contents?<p>Deleted folders are gone forever.',{text:'Delete Forever',fn:doDelete,args:{c:fc,path:fp,folder:fn}});
    else panelAlert('Which folder would you like to Delete?<p>Click a folder\'s name to select it, then click <b>Delete</b>.');

    return false;
}

function addFolder(){
    if(YAHOO.alpine.current.incoming){
	showStatusMessage('Cannot add new Incoming Folders yet',5);
	return false;
    }

    var dom = YAHOO.util.Dom;
    var fp = document.getElementById('pickFolderPath').value;
    var fc = document.getElementById('pickFolderCollection').value;
    if(fc >= 0){
	var dbody = document.createDocumentFragment();
	var div = document.createElement('div');
	dom.addClass(div,'panelExplanation');
	var lc = 'in', l = 'Name of Folder to create.  To create the folder with in a subdirectory, simply prepend the subdirectory\'s name.';
	var el = document.createTextNode(l);
	div.appendChild(el);
	dbody.appendChild(div);

	// Folder Name
	div = document.createElement('div');
	dom.addClass(div,'panelInput');
	div.innerHTML = 'Folder Name:';
	var input = createInputElement('text','');
	div.appendChild(input);
	YAHOO.util.Event.addListener(input,'keyup',panelDialogInputChange);
	dbody.appendChild(div);
	panelDialog('New Folder',
		    dbody,
		    {
			label:"New Folder",
			disabled: true,
			fn: function(){
			    if(input.value.length) newFolderList(document.getElementById('alpineContent'),null,fc,fp,{op:'add',f:input.value});
			    else showStatusMessage('No folder name provided.  No folder added.',5);
			    var fne = document.getElementById('pickFolderName');
			    if(fne) fne.value = '';
			}
		    });
	input.focus();
    }

    return false;
}

function renameFolder(){
    if(YAHOO.alpine.current.incoming){
	showStatusMessage('Cannot rename Incoming Folders yet',5);
	return false;
    }

    var dom = YAHOO.util.Dom;
    var fc = document.getElementById('pickFolderCollection').value;
    var fp = document.getElementById('pickFolderPath').value;
    var fn = document.getElementById('pickFolderName').value;
    if(fc >= 0 && fn.length){
	var dbody = document.createDocumentFragment();
	var div = document.createElement('div');
	dom.addClass(div,'panelExplanation');
	var lc = 'in', l = 'Rename folder ';
	if(fp.length){
	    l += 'in directory ' + fp + ' ';
	    lc = 'of';
	}
	if(YAHOO.alpine.current.collections[fc]) l +=  lc+' collection ' + YAHOO.alpine.current.collections[fc];
	div.innerHTML = l;
	dbody.appendChild(div);

	// Folder Name Input
	div = document.createElement('div');
	dom.addClass(div,'panelInput');
	div.innerHTML = 'New Folder Name:';

	var input = createInputElement('text','');
	var fnt = fn.match(/[\/]?([^\/]*)$/);
	input.setAttribute('value',fnt[1]);
	div.appendChild(input);

	//YAHOO.util.Event.addListener(el,'keyup',panelDialogInputChange);
	dbody.appendChild(div);

	panelDialog('Rename Folder',
		    dbody,
		    {
			label:"Rename Folder",
			fn: function(){
				if(fc >= 0 && fn.length && input.value.length){
				    if(fn != input.value){
					var el = document.getElementById('alpineContent');
					newFolderList(el,null,fc,fp,{op:'rename',sf:fn,df:input.value});
				    }
				    else showStatusMessage('Folder name unchanged.',5);
				}
				else showStatusMessage('Folder name unchanged.',5);
				var fne = document.getElementById('pickFolderName');
				if(fne) fne.value = '';
			    }
		    });

	input.select();
    }
    else panelAlert('Which folder would you like to rename?<p>Click a folder\'s name to select it, then click <b>Rename</b>.');
    
    return false;
}

function exportFolder(){
    var dom = YAHOO.util.Dom;
    var fc = document.getElementById('pickFolderCollection').value;
    var fp = document.getElementById('pickFolderPath').value;
    var fn = document.getElementById('pickFolderName').value;
    var t;
    if(fc >= 0 && fn.length){
	var url = 'conduit/export/' + fc + '/';
	if(fp.length) url += encodeURIFolderPath(fp) + '/';
	url += encodeURIComponent(fn);
	window.location.href = document.getElementsByTagName('base')[0].href + url;
	t = '<h3>Export Email</h3>Web Alpine is preparing folder <b>'+fn+'</b> for download. &nbsp;';
	t += 'Your browser\'s File Open dialog should appear momentarily.<p>';
	t += 'The exported file will be in a format compatible with many desktop mail programs. &nbsp;';
	t += 'You can also use <i>Import Email</i> to copy the folder back into Web Alpine.<p>';
	t += 'If the download completes without error, you may delete the folder from Web Alpine.<p>';
    }
    else
	t = 'Which folder would you like to export?<p>Click a folder\'s name to select it, then click <b>Export&nbsp;Email...</b>.'

    panelAlert(t);
    return false;
}

function redrawFolderList(){
    var fc = document.getElementById('pickFolderCollection').value;
    var fp = document.getElementById('pickFolderPath').value;
    if(fc >= 0){
	var el = document.getElementById('alpineContent');
	newFolderList(el,null,fc,fp,{op:'noop'});
    }
}

function importFolder(){
    if(YAHOO.alpine.current.incoming){
	showStatusMessage('Cannot import Incoming Folders yet',5);
	return false;
    }

    var dom = YAHOO.util.Dom;
    var event = YAHOO.util.Event;
    var fc = document.getElementById('pickFolderCollection').value;
    var fp = document.getElementById('pickFolderPath').value;
    var form = document.createElement('form');
    form.setAttribute('action','conduit/import/' + fc + '/' + encodeURIFolderPath(fp));
    form.setAttribute('method','POST');
    form.setAttribute('enctype','multipart/form-data');
    form.setAttribute('target','formResponse');

    var div = document.createElement('div');
    dom.addClass(div,'panelExplanation');
    div.innerHTML = 'Folder Import copies a mail folder, typically created by the <i>Export</i> command, from the computer your browser is running on into a new Web Alpine folder.  Successful Import consists of three steps.<p>First, enter the path and filename of the folder below. Use the Browse button to help choose the file.';
    form.appendChild(div);

    div = document.createElement('div');
    dom.addClass(div,'panelInput');
    var input0 = createInputElement('file','file');
    event.addListener(input0,'keypress',panelDialogInputChange);
    event.addListener(input0,'change',panelDialogInputChange);
    div.appendChild(input0);
    form.appendChild(div);

    div = document.createElement('div');
    div.innerHTML = 'Second, provide a <b>unique</b> name to give the uploaded folder';
    if(YAHOO.alpine.current.collections[fc]) div.innerHTML += ' within the '+ YAHOO.alpine.current.collections[fc]+' collection.';
    dom.addClass(div,'panelExplanation');
    form.appendChild(div);

    div = document.createElement('div');
    dom.addClass(div,'panelInput');
    div.innerHTML = 'Name the folder:';
    var input = createInputElement('text','newFolder');
    div.appendChild(input);
    form.appendChild(div);

    div = document.createElement('div');
    div.innerHTML = 'Finally, click <b>Import&nbsp;Folder</b> to copy the folder into Web Alpine.';
    dom.addClass(div,'panelExplanation');
    form.appendChild(div);


    panelDialog('Import Folder', form,
		{
		    label:'Import Folder',
		    disabled: true,
		    fn: function(){ showLoading(); form.submit(); }
		});
    input0.focus();
    return false;
}

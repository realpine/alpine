/* $Id: folders.js 391 2007-01-25 03:53:59Z mikes@u.washington.edu $
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
    for(var i = 1; i <= 10; i++){
	dom.setStyle('settingsPage' + i,'display','none');
	dom.removeClass(document.getElementById('Page' + i).parentNode,'sel');
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

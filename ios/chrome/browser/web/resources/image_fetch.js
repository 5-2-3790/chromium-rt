// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Add functionality related to getting image data.
 */
goog.provide('__crWeb.imageFetch');

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected.
 */
__gCrWeb.imageFetch = {};

/**
 * Store common namespace object in a global __gCrWeb object referenced by a
 * string, so it does not get renamed by closure compiler during the
 * minification.
 */
__gCrWeb['imageFetch'] = __gCrWeb.imageFetch;

/* Beginning of anonymous object. */
(function() {

/**
 * Returns image data as base64 string, because WKWebView does not support BLOB
 * on messages to native code. Try getting data directly from <img> first, and
 * if failed try downloading by XMLHttpRequest.
 *
 * @param {number} id The ID for curent call. It should be attached to the
 *     message sent back.
 * @param {string} url The URL of the requested image.
 */
__gCrWeb.imageFetch.getImageData = function(id, url) {
  var onData = function(data) {
    __gCrWeb.message.invokeOnHost(
        {'command': 'imageFetch.getImageData', 'id': id, 'data': data});
  };
  var onError = function() {
    __gCrWeb.message.invokeOnHost(
        {'command': 'imageFetch.getImageData', 'id': id});
  };

  var data = getImageDataByCanvas(url);
  if (data) {
    onData(data);
  } else {
    getImageDataByXMLHttpRequest(url, 100, onData, onError);
  }
};

/**
 * Returns image data directly from <img> by drawing it to <canvas> and export
 * it. If the <img> is cross-origin without "crossorigin=anonymous", this would
 * be prevented by the browser. The exported image is in a resolution of 96 dpi.
 *
 * @param {string} url The URL of the requested image.
 * @return {string|null} Image data in base64 string, or null if no <img> with
 *     "src=|url|" is found or exporting data from <img> failed.
 */
function getImageDataByCanvas(url) {
  for (var key in document.images) {
    var img = document.images[key];
    if (img.src == url) {
      var canvas = document.createElement('canvas');
      canvas.width = img.naturalWidth;
      canvas.height = img.naturalHeight;
      var ctx = canvas.getContext('2d');
      ctx.drawImage(img, 0, 0);
      var data;
      try {
        // If the <img> is cross-domain without "crossorigin=anonymous", an
        // exception will be thrown.
        data = canvas.toDataURL('image/png');
      } catch (error) {
        return null;
      }
      // Remove the "data:type/subtype;base64," header.
      return data.split(',')[1];
    }
  }
  return null;
};

/**
 * Returns image data by downloading it using XMLHttpRequest.
 *
 * @param {string} url The URL of the requested image.
 * @param {number} timeout The timeout in milliseconds for XMLHttpRequest.
 * @param {Function} onData Callback when fetching image data succeeded.
 * @param {Function} onError Callback when fetching image data failed.
 */
function getImageDataByXMLHttpRequest(url, timeout, onData, onError) {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', url);
  xhr.timeout = timeout;
  xhr.responseType = 'blob';

  xhr.onload = function() {
    if (xhr.status != 200) {
      onError();
      return;
    }
    var fr = new FileReader();

    fr.onload = function() {
      onData(btoa(/** @type{string} */ (fr.result)));
    };
    fr.onabort = onError;
    fr.onerror = onError;

    fr.readAsBinaryString(/** @type{!Blob} */ (xhr.response));
  };
  xhr.onabort = onError;
  xhr.onerror = onError;
  xhr.ontimeout = onError;

  xhr.send();
};
}());  // End of anonymous object

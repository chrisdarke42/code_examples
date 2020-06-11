(function ($) {
  'use strict';

  Drupal.behaviors.nygbPdfOnLoad = {

    getIframe: function(url, params) {
      let deferred = $.Deferred();
      $.ajax({
        type: 'POST',
        url: url,
        data: params,
      }).done(function (data) {
        deferred.resolve(data);
      }).fail(function () {
        deferred.reject('HTTP error');
      });
      $.when(deferred).done(function(iframe) {
        // If something should be done after the iframe data is returned.
      });
      return deferred.promise();
    },

    attach: function (context, settings) {
      // Get placeholders for iframes
      let $placeholderIframes = $('div.nygb-load_pdf_callback');
      if ($placeholderIframes.length === 0) {
        // Returns early.
        return;
      }

      // Iframe loader to get iframe html back from Drupal and replace
      // placeholder on page.
      function iframeLoader($iframePlaceholder) {
        let params = {};
        let url = $iframePlaceholder.data('url');
        let fid = $iframePlaceholder.data('fid');
        let access = $iframePlaceholder.data('access');
        let page = null;
        let type = 'multi';
        if (window.location.hash) {
          let pageMatch = /#page=(\d*)/g;
          let result = pageMatch.exec(window.location.hash);
          if (result.length && result[1].length) {
            page = result[1];
          }
        }
        if ($iframePlaceholder.hasClass('nygb-load_singe_page') && page === null) {
          // Hide single view tab.
          $('.horizontal-tab-button-1').focus();
          type = 'single';
          $iframePlaceholder.closest('.horizontal-tabs-pane').hide();
          return;
        }
        params.url = url;
        params.width = $iframePlaceholder.data('width');
        params.height = $iframePlaceholder.data('height');
        params.fid = fid;
        params.page = page;
        params.type = type;
        if (typeof params.fid !== 'undefined') {
          Drupal.behaviors.nygbPdfOnLoad.getIframe('/pdf-iframe/get', params).then(function (data) {
            $iframePlaceholder.replaceWith(data);
            Drupal.attachBehaviors($iframePlaceholder);
          });
        }
      }

      // Mutation observer to check if a tab becomes visible, to render out
      // iframe data.
      let observer = new MutationObserver(function(mutations) {
        mutations.forEach(function (mutation) {
          let $tab = $(mutation.target);
          if (!$tab.hasClass('horizontal-tab-hidden')) {
            let $iframePlaceholder = $tab.find('.nygb-load_on_view');
            if ($iframePlaceholder.length === 0) {
              // Returns early.
              return;
            }
            iframeLoader($iframePlaceholder);
          }
        });
      });


      // For all iframe placeholders, add a mutation observer or load directly
      // if already visible.
      $placeholderIframes.each(function (i, domElement) {
        let $placeholderElement = $(domElement);
        let $horizontalTab = $placeholderElement.closest('.horizontal-tabs-pane');
        if ($horizontalTab.hasClass('horizontal-tab-hidden')) {
          observer.observe($horizontalTab[0], {
            attributes: true
          });
        }
        else {
          let $iframePlaceholder = $horizontalTab.find('.nygb-load_pdf_callback');
          iframeLoader($iframePlaceholder);
        }

      });
    }
  };

  Drupal.behaviors.nygbPDFtweaks = {
    outerElement: {},
    returnPage: {},
    pageCount: 0,
    pdfOrigin: '',
    page: 1,
    originalPage: 1,
    PDFViewerApplication: {},
    PDFRenderingQueue: {},
    override: true,
    init: true,

    initValues: function() {
      let _this = Drupal.behaviors.nygbPDFtweaks;
      if (_this.init) {
        _this.resetValues();
        _this.pageCount = _this.outerElement.data('pagecount');
        _this.pdfOrigin = _this.outerElement.data('docsource');
        _this.page = _this.outerElement.data('page');
        _this.originalPage = _this.outerElement.data('page');
        _this.externalControls = $('<div class="pdf-external-controls"></div>').insertBefore(_this.outerElement);
      }
    },

    resetValues: function() {
      let _this = Drupal.behaviors.nygbPDFtweaks;
      let iframeContentWindow = _this.outerElement[0].contentWindow;
      if (typeof iframeContentWindow.PDFViewerApplication !== 'undefined') {
        _this.PDFViewerApplication = iframeContentWindow.PDFViewerApplication;
        if (_this.override) {
          _this.overridePDFViewerToolbar();
        }
        return true;
      }
      return false;
    },

    bindPageNumberChanged: function(event) {
      let _this = Drupal.behaviors.nygbPDFtweaks;
      _this.setPage(event.value);
    },

    pageReturnLink: function() {
      let _this = Drupal.behaviors.nygbPDFtweaks;
      let $link = _this.externalControls.find('.return-to-page');
      if (_this.page !== _this.originalPage) {
        // Backwards or forwards?.
        var linktext = '';
        if (_this.page > _this.originalPage) {
          linktext = '<< Return to original page';
        }
        else {
          linktext = 'Return to original page >>';
        }

        if (!$link.length) {
          var $return_to_page = $('<a href="#" class="return-to-page pdf-external-controls__control">' + linktext + '</a>').appendTo(_this.externalControls);
          $return_to_page.on('click', _this.pageReturnLinkEvent);
        }
        else {
          $link.text(linktext);
        }
      }
      else {
        if ($link.length) {
          $link.remove();
        }
      }
    },

    pageReturnLinkEvent: function() {
      let _this = Drupal.behaviors.nygbPDFtweaks;
      _this.setPage(_this.originalPage);
    },

    setPage: function(page) {
      let _this = Drupal.behaviors.nygbPDFtweaks;
      let pdfPath = _this.pdfOrigin + '/' + page;
      let open = _this.PDFViewerApplication.open(pdfPath, {'filename': 'filename.pdf', 'pagenum': parseInt(page)});
      open.then(function() {
        _this.page = page;
        _this.pageReturnLink();
      }, function(error) {
        let errorParams = {};
        errorParams.error = error;
        Drupal.behaviors.nygbPdfOnLoad.getIframe('/pdf-iframe/error', errorParams).then(function (data) {
          _this.outerElement.replaceWith(data);
        });
      });
    },

    previousPage: function() {
      let page = parseInt(Drupal.behaviors.nygbPDFtweaks.page);
      if (page <= 1) {
        return;
      }
      Drupal.behaviors.nygbPDFtweaks.setPage(page - 1);
    },

    nextPage: function() {
      let page = parseInt(Drupal.behaviors.nygbPDFtweaks.page);
      if (page >= Drupal.behaviors.nygbPDFtweaks.pageCount) {
        return;
      }
      Drupal.behaviors.nygbPDFtweaks.setPage(page + 1);
    },

    overridePDFViewerToolbar: function () {
      let _this = Drupal.behaviors.nygbPDFtweaks;
      if (!_this.override) {
        return;
      }
      _this.override = false;
      let PDFViewer = _this.PDFViewerApplication.pdfViewer;
      let eventBus = PDFViewer.eventBus;

      _this.PDFViewerApplication.unbindEvents();
      eventBus.on('previouspage', function(event) {
        _this.previousPage();
      });
      eventBus.on('nextpage', function(event) {
        _this.nextPage();
      });

      eventBus.on('pagenumberchanged', _this.bindPageNumberChanged);
      _this.PDFViewerApplication.bindEvents();
    },

    loadingOverlayTrigger: function(iframeElement) {
      let $overlay = $('<div id="pdfLoadingOverlay">Loading...</div>');
      let _this = Drupal.behaviors.nygbPDFtweaks;
      _this.overlay = $overlay;
      let $pdfArea = $('.file-application-pdf');
      let width = $pdfArea.outerWidth();
      if (!$pdfArea.find('#pdfLoadingOverlay').length) {
        $pdfArea.prepend($overlay);
      }
      else {
        $overlay.show();
      }
      let divWidth = $overlay.outerWidth();
      $overlay.css({'top': 150,'left': (width/2) - (divWidth/2)});

      if(typeof iframeElement.contentWindow.PDFViewerApplication !== 'undefined' && iframeElement.contentWindow.PDFViewerApplication.initialized) {
        if (typeof iframeElement.contentWindow.PDFViewerApplication.pdfLoadingTask !== 'undefined') {
          let PDFViewer = iframeElement.contentWindow.PDFViewerApplication.pdfViewer;
          let eventBus = PDFViewer.eventBus;
          $(document).off('loadOverlay');
          eventBus.on('hideLoadingBar', function(event) {
            $('#pdfLoadingOverlay').remove();
          });
        }
      }
    },

    loadingOverlay: function ($iframePdf, context) {

      let iframeElement = $iframePdf[0];

      $(document).on('loadOverlay', function(event) {
        let _this = Drupal.behaviors.nygbPDFtweaks;
        _this.loadingOverlayTrigger(iframeElement);
      });

      window.document.addEventListener('webviewerloaded', handleEvent, false);
      function handleEvent(e) {
        $(document).trigger('loadOverlay', {'position': 'event'});
      }
      $(document).trigger('loadOverlay', {'position': 'init'});
    },

    attach: function (context) {
      if (context !== document) {
        return;
      }
      let _this = Drupal.behaviors.nygbPDFtweaks;
      let $iframePdf = $('iframe.pdf');
      if ($iframePdf.length) {
        _this.loadingOverlay($iframePdf, context);
        let $singlePagePDF = $iframePdf.filter('.nygb-load_singe_page');
        if ($singlePagePDF.length) {
          $singlePagePDF[0].contentWindow.addEventListener('textlayerrendered', function (event) {
            _this.overlay.remove();
            let _pdfApplication = $singlePagePDF[0].contentWindow.PDFViewerApplication;
            _this.outerElement = $singlePagePDF;
            _this.initValues();
            _pdfApplication.pdfLoadingTask.then(function () {
              if (_this.resetValues() && _this.init) {
                _this.init = false;
              }
            });
          }, true);
        }
      }
    }
  };

})(jQuery);

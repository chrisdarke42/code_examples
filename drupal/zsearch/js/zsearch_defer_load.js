(function ($) {
  'use strict';

  Drupal.behaviors.zicassoSearchDeferLoad = {
    attach: function (context) {

      function getData(nid, fieldname) {
        var deferred = $.Deferred();
        $.ajax({
          url: '/search/defer/load/' + nid + '/' + fieldname,
          context: document.body
        }).done(function (data) {
          deferred.resolve(data);
        }).fail(function () {
          deferred.reject('HTTP error');
        });
        return deferred.promise();
      }

      var $deferredLoaders = $('.deferred', context);
      // If there is no defferred content in context...
      if ($deferredLoaders.length === 0) {
        // Returns early.
        return;
      }

      $deferredLoaders.each(function (i, domElement) {
        var $deferredElement = $(domElement);
        var $result = $deferredElement.closest('.search-result');
        var deferredElementId = $result.attr('id');
        var nid = $result.data('result-id');
        var fieldname = $deferredElement.data('defer-field');
        if (typeof nid !== 'undefined' && typeof fieldname !== 'undefined') {
          getData(nid, fieldname).then(function (data) {
            $('#' + deferredElementId + ' .deferred[data-defer-field=' + fieldname + ']').empty().append(data);
          });
        }
      });


    }
  };
})(jQuery);

(function ($) {
  'use strict';

  Drupal.behaviors.zicassoRateResult = {
    attach: function (context) {
      var $wrapper = $('.search-rating_wrapper', context);
      var $search_rating = $('#search_results .search-rating', context);
      var $rating_text = $('.search-rating__text', context);
      var cacheId = $search_rating.data('es-cache-id');
      var $thumbs_up = $search_rating.find('.thumbs.up');
      var $thumbs_down = $search_rating.find('.thumbs.down');
      $thumbs_up.click(function (event) {
        event.preventDefault();
        if (!$thumbs_up.hasClass('processing')) {
          $thumbs_up.addClass('processing');
          $thumbs_down.addClass('processing');
          $thumbs_up.addClass('selected');
          $thumbs_down.find('img').fadeTo(0, 0.5);
          $rating_text.html('Thank you for providing feedback!');
          $.get('/search/rate/up/' + cacheId, function(data) {
            if(data.status === 1) {
              $wrapper.trigger('close');
            }
          });
        }
      });
      $thumbs_down.click(function (event) {
        event.preventDefault();
        if (!$thumbs_down.hasClass('processing')) {
          $thumbs_up.addClass('processing');
          $thumbs_down.addClass('processing');
          $thumbs_down.addClass('selected');
          $thumbs_up.find('img').fadeTo(0, 0.5);
          $rating_text.html('Thank you for providing feedback!');
          $.get('/search/rate/down/' + cacheId, function(data) {
            if(data.status === 1) {
              $wrapper.trigger('close');
            }
          });
        }
      });
    }
  };
})(jQuery);

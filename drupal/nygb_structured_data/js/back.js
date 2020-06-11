/**
 * @file
 * Custom logic to build a replacement for the browser's "back" button.
 */

(function ($) {
  'use strict';

  Drupal.behaviors.backButton = {
    attach: function (context) {
      // Add a link back to where the visitor came from.
      if (document.referrer !== '') {
        // Only add the link on pages referred from a search results page.
        if (document.referrer.search('online-records/search-results') >= 0) {
          // Append a link back to the search results page.
          $('.work-around-broken-ux')
            .append('<a href="' + document.referrer + '">&lt; Back to Search Results</a>');
          $('#block-nygb-structured-data-collection-back').show();
        }
      }
    }
  };

})(jQuery);

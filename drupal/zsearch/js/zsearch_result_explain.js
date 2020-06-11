(function ($) {
  'use strict';

  Drupal.behaviors.zicassoExplainResult = {
    attach: function (context) {

      if (context !== document) {
        return;
      }

      var $form_eql = $('#search_results .eql-area', context);
      $form_eql.each(function (i, domElement) {
        var $eql_area = $(domElement);
        var $eql_button = $eql_area.find('a.es-devel-button');
        $eql_button.click(function(event) {
          event.preventDefault();
          var $pre_area = $eql_area.find('pre');
          if ($pre_area.hasClass('hidden')) {
            $pre_area.removeClass('hidden');
            var text = $eql_button.text();
            $eql_button.data('original-text', text);
            $eql_button.text('Hide ' + text);
          }
          else {
            $pre_area.addClass('hidden');
            $eql_button.text($eql_button.data('original-text'));
          }
        });
      });




      var $results = $('#search_results .search-result', context);
      $results.each(function (i, domElement) {
        var $result = $(domElement);
        var nid = $result.data('result-id');
        var $explainArea = $('.explain-area', $result);
        var $clickable = $explainArea.find('a');
        $clickable.click(function(event) {
          event.preventDefault();
          if ($explainArea.hasClass('not-processed')) {
            $.get('/search/explain/' + parseInt(nid, 10) + '/' + $explainArea.data('es-cache-id'), function(data) {
              if(data.status === 1) {
                $explainArea.append($('<pre>').html(data.data));
                $explainArea.find('a').text('Hide Explain').clone(true).addClass('up-click').append('<i class="arrow up"></i>').appendTo($explainArea);
                $explainArea.removeClass('not-processed').addClass('expanded processed');
              }
            });
          }
          else {
            if ($explainArea.hasClass('expanded')) {
              $explainArea.find('pre').slideUp();
              $explainArea.removeClass('expanded');
              $explainArea.find('a').remove('.up-click').text('Explain');
            }
            else {
              $explainArea.find('pre').show();
              $explainArea.addClass('expanded');
              $explainArea.find('a').text('Hide Explain').clone(true).addClass('up-click').append('<i class="arrow up"></i>').appendTo($explainArea);
            }
          }

        });

      });
    }
  };
})(jQuery);


(function ($) {
  'use strict';

  let Facets = {};

  Drupal.behaviors.nygbSearchFacets = {

    resetFacetGroup: function ($facetsGroup) {
      $facetsGroup.find('.checkbox').show();
      $facetsGroup.find('a.select-more').remove();
      $facetsGroup.data('link-added', false);
    },

    sortCheckboxes: function ($facetsGroup) {

      let $facet_elements = $facetsGroup.find('.checkbox');

      // Sort checkboxes by number of results
      $facet_elements.sort(function(a,b){
        if (parseInt($(b).data('result-count')) < parseInt($(a).data('result-count'))) {
          return -1;
        } else if (parseInt($(b).data('result-count')) > parseInt($(a).data('result-count'))) {
          return 1;
        }
        else {
          return 0;
        }
      });

      // Pull out the checked elements.
      let $checked_elements = $facet_elements.filter(function(index, domElement) {
        let $element = $(domElement);
        if ($element.find('input.search-facets').is(':checked')) {
          return true;
        }
        return false;
      });

      $($checked_elements.get().reverse()).each(function (i, domElement) {
        let $checkbox = $(domElement);
        $checkbox.remove().prependTo($facetsGroup);
      });
    },

    collapseCheckboxes: function ($facetsGroup, force_reload) {

      if (force_reload) {
        Drupal.behaviors.nygbSearchFacets.resetFacetGroup($facetsGroup);
        Drupal.behaviors.nygbSearchFacets.sortCheckboxes($facetsGroup);
      }

      let group_id = $facetsGroup.attr('id');
      let $facet_elements = $facetsGroup.find('.checkbox');
      let facetCount = $facet_elements.length;

      // Return early if 5 items or less.
      if (facetCount <= 5 && !force_reload) {
        return false;
      }
      // Collapse all elements that arent selected after the first 5.
      // 1. count how many are selected.
      let selectedCount = $facet_elements.find('input.search-facets:checked').length;
      // Return early if all the items are selected.
      if (facetCount === selectedCount) {
        return false;
      }
      // 2. do the math
      let index = 5;
      if (selectedCount > 4) {
        index = selectedCount - 1;
      }
      // 3. collapse the rest
      $facet_elements.filter(':gt(' + index + ')').hide();
      // 4. add link
      let link_text = '+' + (facetCount - index - 1) + ' more';

      let linkAdded = $facetsGroup.data('link-added');
      if (typeof linkAdded == 'undefined' || !linkAdded) {
        // Create Link to load modal, use the Bootstrap modal.
        let $link = $('<a>',{
          text: link_text,
          title: 'Show More Links',
          href: '#',
          type: 'button',
          class: 'select-more btn btn-primary',
          'data-group-id': group_id,
          'data-selected': false
        });

        $facetsGroup.append($link);
        $facetsGroup.data('link-added', true);

        $link.on('click', function (event) {
          // Expand
          event.preventDefault();
          Drupal.behaviors.nygbSearchFacets.collapseAllOtherCheckboxes(group_id, true);
          let $linkElements = $(event.currentTarget);
          let linkElement = $linkElements[0];
          let selectedState = $link.data('selected');
          if (!selectedState) {
            $facet_elements.show();
            $link.data('original-title', linkElement.text);
            linkElement.text = 'Collapse';
            $link.data('selected', true);
          }
          else {
            // Collapse
            Drupal.behaviors.nygbSearchFacets.collapseCheckboxes($facetsGroup, true);
          }
        });
      }
      else {
        let $link = $facetsGroup.find('a.select-more');
        $link[0].text = $link.data('original-title');
        $link.data('selected', false);
      }
    },

    collapseAllOtherCheckboxes: function (group_id, force_reload) {
      let $facetSets = $('.form-checkboxes.search-facets:not(#' + group_id + ')');
      $facetSets.each(function (i, domElement) {
        if (!$facetSets.hasClass('modal-facets')) {
          let $facetsGroup = $(domElement);
          Drupal.behaviors.nygbSearchFacets.addAncillaryData($facetsGroup);
          Drupal.behaviors.nygbSearchFacets.collapseCheckboxes($facetsGroup, force_reload);
        }
      });
    },

    addAncillaryData: function($facetsGroup) {
      // Regex for extracting result number
      const regex = /\((\d+)\)$/gm;
      let $facet_elements = $facetsGroup.find('.checkbox');
      $facet_elements.each(function (i, domElement) {
        let $facet_element = $(domElement);
        let match;
        while ((match = regex.exec($facet_element.find('label.control-label').text())) !== null) {
          // This is necessary to avoid infinite loops with zero-width matches
          if (match.index === regex.lastIndex) {
            regex.lastIndex++;
          }
          if (match.length > 1) {
            $facet_element.attr('data-result-count', parseInt(match[1], 10));
          }
        }
      });
    },

    attach: function (context, settings) {
      // Collapse down all Facet groups.
      let $facetSets = $('.form-checkboxes.search-facets');
      $facetSets.each(function (i, domElement) {
        if (!$facetSets.hasClass('modal-facets')) {
          let $facetsGroup = $(domElement);
          Drupal.behaviors.nygbSearchFacets.addAncillaryData($facetsGroup);
          Drupal.behaviors.nygbSearchFacets.collapseCheckboxes($facetsGroup, false);
        }
      });
    }
  };

})(jQuery);


(function ($) {
  'use strict';

  let Facets = {};

  Drupal.behaviors.nygbSearchFacets = {

    buildModalParent: function() {
      let modalSelectorId = 'facet-modal';
      if (!$('#' + modalSelectorId).length) {
        let content = '<div class="modal-header">' +
            '<h5 class="modal-title" id="modalLongTitle"></h5>' +
            '<button type="button" class="close" data-dismiss="modal" aria-label="Close">' +
            '<span aria-hidden="true">&times;</span></button>' +
            '</div>' +
            '<div class="modal-body"></div>' +
            '<div class="modal-footer"><button type="button" class="btn btn-secondary" data-dismiss="modal">Close</button></div>';
        $('body').append('<div id="' + modalSelectorId + '" class="modal fade" tabindex="-1" role="dialog" aria-hidden="true"><div class="modal-dialog modal-dialog-centered" role="document"><div class="modal-content">' + content + '</div></div></div>');

        Facets.modal = $('#' + modalSelectorId);
        Facets.modal.header = $('#' + modalSelectorId + ' .modal-header');
        Facets.modal.body = $('#' + modalSelectorId + ' .modal-body');
        Facets.modal.header.append($('<input>',{
          type: 'text',
          id: 'filter-facets-search',
          placeholder: 'search',
        }).keyup(Drupal.behaviors.nygbSearchFacets.debounce(Drupal.behaviors.nygbSearchFacets.filterCheckboxes, 250)).hide());
        Facets.modal.title = $('#' + modalSelectorId + ' #modalLongTitle');
        Facets.modal.search = Facets.modal.header.find('#filter-facets-search');
        Facets.modal.id = modalSelectorId;
      }
    },

    debounce: function(func, wait, immediate) {
      let timeout;
      return function() {
        let context = this, args = arguments;
        let later = function() {
          timeout = null;
          if (!immediate) func.apply(context, args);
        };
        let callNow = immediate && !timeout;
        clearTimeout(timeout);
        timeout = setTimeout(later, wait);
        if (callNow) func.apply(context, args);
      };
    },

    filterCheckboxes: function(event) {
      let filter = event.currentTarget.value.toUpperCase();
      let $checkboxes = Facets.modal.find('.modal-facets .checkbox');
      $checkboxes.each(function(i, domElement) {
        let $checkbox = $(domElement);
        if ($checkbox.text().toUpperCase().indexOf(filter) > -1) {
          $checkbox.show();
        }
        else {
          $checkbox.hide();
        }
      });
    },

    cloneCheckboxes: function(groupId) {
      let $parentFacet = $('#' + groupId);
      Facets.modal.title.html($parentFacet.siblings('label').text());
      let $facets = $parentFacet.clone(true).attr('id', groupId + '-modal').addClass('modal-facets').wrap("<div class='facetWrapper'></div>");
      let $facetsWrapper = $facets.closest('.facetWrapper');
      $facetsWrapper.appendTo(Facets.modal.body);
      Drupal.behaviors.nygbSearchFacets.expandCheckboxes($facetsWrapper);
      Facets.modal.facetWrapper = $facetsWrapper;
      Facets.modal.facetId = groupId;
      if ($facets.children().length > 50) {
        Facets.modal.search.show('fast');
        Facets.modal.facetWrapper.addClass('searchFieldDisplay');
      }
    },

    cleanUpCheckboxes: function() {
      Facets.modal.search.hide();
      Facets.modal.facetWrapper.remove();
      let $parentFacet = $('#' + Facets.modal.facetId);
      Drupal.behaviors.nygbSearchFacets.collapseCheckboxes($parentFacet, true);
    },

    resetFacetGroup: function ($facetsGroup) {
      $facetsGroup.find('.checkbox').show();
      $facetsGroup.find('a.select-more').remove();
    },

    expandCheckboxes: function ($facetsWrapper) {
      let $facetsGroup = $facetsWrapper.find('.form-checkboxes').first();
      Drupal.behaviors.nygbSearchFacets.resetFacetGroup($facetsGroup);
      // let group_id = $facetsGroup.attr('id');
      let $facet_elements = $facetsGroup.find('.checkbox input');
      // When checkbox in modal is clicked, make the original checkbox check.
      $facet_elements.change(function(event) {
        let $checkbox = $(event.currentTarget);
        let $target = $('#' + $checkbox.attr('id'));
        let state = $checkbox.prop('checked');
        $target.prop('checked', state);
      });

      // DISABLING LABEL CLICK TO MAKE USE OF THE DIV CLICK.
      // let $facet_labels = $facetsGroup.find('.checkbox label');
      // When the label in modal is clicked, make the modal checkbox check.
      // $facet_labels.click(function(event) {
      //   if (event.target.className === 'control-label') {
      //     let $label = $(event.currentTarget);
      //     let facet_checkbox_class = $label.attr('for');
      //     let $facet_checkbox = $facetsGroup.find('#' + facet_checkbox_class);
      //     let state = !$facet_checkbox.prop('checked');
      //     $facet_checkbox.prop('checked', state);
      //   }
      // });

      let $facet_div = $facetsGroup.find('.checkbox');
      $facet_div.click(function(event) {
        let $checkbox = $(event.target).find('input');
        $checkbox.prop( "checked", !$checkbox.is(':checked'));
        $checkbox.trigger("change");
      });
    },

    sortCheckboxes: function ($facetsGroup) {
      let $facet_elements = $facetsGroup.find('.checkbox');
      // let $checked = $facet_elements.find('input.search-facets:checked');
      // Sort All checkboxes alphabetically.
      $facet_elements.sort(function(a,b){
        return $(a).find('label.control-label').text().toUpperCase().localeCompare($(b).find('label.control-label').text().toUpperCase());
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

      // Create Link to load modal, use the Bootstrap modal.
      let $link = $('<a>',{
        text: link_text,
        title: 'Show More Links',
        href: '#',
        type: 'button',
        class: 'select-more btn btn-primary',
        'data-group-id': group_id,
        'data-toggle':  'modal',
        'data-target': '#' + Facets.modal.id
      });

      $facetsGroup.append($link);
    },

    attach: function (context, settings) {
      // Set up Modal Wrapper.
      Drupal.behaviors.nygbSearchFacets.buildModalParent();

      // Collapse down all Facet groups.
      let $facetSets = $('.form-checkboxes.search-facets');
      $facetSets.each(function (i, domElement) {
        if (!$facetSets.hasClass('modal-facets')) {
          let $facetsGroup = $(domElement);
          Drupal.behaviors.nygbSearchFacets.collapseCheckboxes($facetsGroup, false);
        }
      });

      Facets.modal.on('show.bs.modal', function (event) {
        let button = $(event.relatedTarget); // Button that triggered the modal
        let groupId = button.data('group-id'); // Group ID that needs to be loaded
        Drupal.behaviors.nygbSearchFacets.cloneCheckboxes(groupId);
      });

      Facets.modal.on('hide.bs.modal', function (event) {
        Drupal.behaviors.nygbSearchFacets.cleanUpCheckboxes();
      });


      // Add click action to close Modal.
      $('body').click(function(event) {
        if (Facets.modal.visible) {
          let hide = false;
          if (typeof $(event.target).attr('id') !== 'undefined' && $(event.target).attr('id') === Facets.modal.id) {
            hide = false;
          }
          else if ($(event.target).parents('#' + Facets.modal.id).length === 0 && event.originalEvent.detail === 1) {
            hide = true;
          }
          if (hide) {
            Drupal.behaviors.nygbSearchFacets.hideModal(event);
          }
        }
      });

    }
  };

})(jQuery);

<?php

/**
 * @file
 * Form footer theming for search.
 *
 * Available variables:
 *
 * $form
 *  - keywords
 *  - submit_button
 *
 * $classes
 */

$classes = isset($form['#attributes']['class']) ? $form['#attributes']['class'] : '';
$tip_content = theme('zsearch_elasticsearch_search_tips');

?>
<div class="search-footer <?php print $classes; ?>">
  <div class="search-footer__contents content-container">
    <h2 class="search-footer__title-mobile">Search Sample Trips</h2>
    <h2 class="search-footer__title-desktop">Search Sample Itineraries</h2>
    <div class="search-form elastic-search">
      <?php print render($form['keywords']); ?>
      <?php print theme('zsearch_elasticsearch_tooltip', array('classes' => 'search-form__tooltip search-tooltip--center')); ?>
      <a href="" class="search-form__mobile-clear input-clear" data-input-name="keywords">
        <img class="search-form__mobile-clear__icon" src="/sites/all/themes/custom/zictheme/images/search/search_clear_x.svg" alt="clear search icon">
      </a>
      <?php print render($form['submit_button']); ?>
      <?php print drupal_render_children($form); ?>
    </div>
    <div class="search-footer__tooltip-mobile">
      <span>Search Tip: </span>
      <?php print theme('zsearch_elasticsearch_tooltip', array('classes' => 'search-tooltip--left')); ?>
    </div>
    <div class="search-footer__no-results-content">
      <?php print $tip_content; ?>
    </div>
  </div>
</div>
<div class="search-end-cta">
  <h2>Can't find exactly what you're looking for?</h2>
  <a class="large-button" href="/node/add/triprequest">Get matched to a travel specialist</a>
</div>

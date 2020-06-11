<?php

/**
 * @file
 * Summary theme for search.
 *
 * Fields from theme:
 * $num_results - number of results.
 * $page - page number.
 */

  $classes = '';
  $results_text = '';
  $page_text = '';
  if(isset($num_results) && $num_results > 0) {
    $results_text = format_plural($num_results, '1 Sample Trip Result', '@count Sample Trip Results');
    $page_text = ($page > 0) ? t('Page @page of', ['@page' => ($page + 1)]) . ' ' : '';
  }
  else {
    $classes = 'search-summary_no-results';
  }

?>

<div class="search-summary <?php print $classes; ?>">
  <?php if (isset($num_results) && $num_results > 0): ?>
    <div class="search-summary__contents">
      <h2><?php print $page_text . $results_text; ?></h2>
      <p class="search-summary__text">Use these as inspiration for your trip and remember: <i>Everything is customizable.</i></p>
    </div>
    <div class="search-summary__cta hide-tablet">
      <p>Can't find exactly what you're looking for?</p>
      <a class="large-button" href="/node/add/triprequest">Get a Custom-made Trip</a>
    </div>
  <?php else: ?>
    <h2>We're Sorry. No results matched your search.</h2>
    <p class="search-summary__text-no-results">Remember, <a href="">everything is customizable.</a> Please try again describing your trip differently.</p>
  <?php endif; ?>
</div>

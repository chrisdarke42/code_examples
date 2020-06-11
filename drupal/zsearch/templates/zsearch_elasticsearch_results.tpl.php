<?php

/**
 * @file
 * Search Results wrapper.
 *
 * Available variables:
 *
 * $summary
 * $debug
 * $results
 * $pager
 * $cache_id
 */
?>
<?php print $summary; ?>
<div id="search_results" class="search-results content-container-narrow">
  <?php if ($debug): ?>
    <div class="search-summary__debug">
      <?php print $debug; ?>
    </div>
  <?php endif; ?>
  <?php print $results; ?>
  <?php print $pager; ?>
  <div class="search-rating_wrapper search-rating_positioned">
    <div class="search-rating" data-es-cache-id="<?php print $cache_id; ?>">
      <div class="search-rating__text">Were these search results helpful?</div>
      <div class="search-rating__thumbs">
        <a href="" class="search-rating__thumbs-up thumbs up" data-search-rating="up">
          <img src="/sites/all/themes/custom/zictheme/images/search/thumbs_up.svg" alt="Thumbs Up">
        </a>
        <a href="" class="search-rating__thumbs-down thumbs down" data-search-rating="down">
          <img src="/sites/all/themes/custom/zictheme/images/search/thumbs_down.svg" alt="Thumbs Down">
        </a>
      </div>
      <div class="search-rating__close">
        <a href=""><img src="/sites/all/themes/custom/zictheme/images/search/close_popup.svg" alt="Close Popup"></a>
      </div>
    </div>
  </div>
</div>

<?php

$options = [20 => 20, 50 => 50, 100 => 100];
if (!array_key_exists($results_number, $options)) {
  $options[$results_number] = $results_number;
}
$params = drupal_get_query_parameters();

?>

<div class="search-results-header search-results-header__<?php echo $index_name; ?>">
  <h2 class="search-results-header__results-title"><?php echo $title; ?></h2>
  <a href="#" class="btn-tooltip-modal search-results-header__help" data-toggle="modal" data-target="#search-help_<?php echo $index_name; ?>"><?php echo t('What\'s this?'); ?></a>
  <div class="search-results-header__settings">
      <div class="search-results-header__settings-dropdown dropdown">
          <button id="search-results-header__settings-dropdown-label" type="button" data-toggle="dropdown" aria-haspopup="true" aria-expanded="false">
              <?php echo $results_number; ?>
              <span class="caret"></span>
          </button>
          <ul class="dropdown-menu search-results-header__settings-dropdown-menu" aria-labelledby="search-results-header__settings-dropdown-label">
            <?php foreach($options as $option): ?>
            <?php
              $url_param = $params;
              $url_param['rescount'] = $option;
              ?>
            <li class="<?php echo ($results_number === $option) ? 'selected' : '' ?>"><a href="?<?php echo drupal_http_build_query($url_param); ?>"><?php echo $option; ?></a></li>
            <?php endforeach; ?>
          </ul>
          <span> Results Per Page</span>
      </div>
  </div>
</div>

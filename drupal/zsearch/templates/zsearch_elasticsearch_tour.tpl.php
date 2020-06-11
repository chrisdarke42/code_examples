<?php

/**
 * @file
 * Result theme for search.
 *
 * Available variables:
 *
 * $result
 *  - id
 *  - title
 *  - summary
 *  - duration
 *  - missing
 */

$image = theme_image(array(
  'path' => drupal_get_path('module', 'zsearch') . '/images/placeholder.png',
  'width' => 670,
  'height' => 584,
  'attributes' => array('class' => 'image-placeholder'),
));

$cta = l(t('See Trip Details'), drupal_get_path_alias('node/' . $result['id']), array('attributes' => array('class' => 'large-button', 'target' => '_blank')));

?>
<div class="search-result" data-result-id="<?php print $result['id']; ?>" id="result-<?php print $result['id']; ?>">
  <div class="search-result__image deferred thumbnail" data-defer-field="field_photos">
    <?php print $image; ?>
  </div>
  <div class="search-result__text">
    <h3><?php print $result['title']; ?></h3>
    <div class="search-result__rating"><span>Tour Company Rating: </span>
      <div class="search-result__rating__fivestar deferred" data-defer-field="field_travel_company">
        <?php print zictheme_average_fivestar(0, NULL, 5); ?>
      </div></div>
    <?php if (isset($result['locations']) && count($result['locations']) > 0): ?>
    <div class="search-result__text__destinations search-result__text-spaced">
      <span class="search-result__text_left">Destination(s):</span>
      <span class="search-result__text_right read-more" data-read-more-characters="70"><?php print implode(', ', $result['locations']); ?></span>
    </div>
    <?php endif; ?>
    <?php if (isset($result['duration']) && $result['duration'] > 0): ?>
    <div class="search-result__text__num-days search-result__text-spaced">
      <span class="search-result__text_left">No. of Days:</span>
      <span class="search-result__text_right"><?php print $result['duration']; ?> <i>(Customizable)</i></span>
    </div>
    <?php endif; ?>
    <div class="search-result__text__description hide-tablet read-more" data-read-more-pretruncated="<?php print truncate_utf8($result['summary'], 260, TRUE); ?>">
      <?php print $result['summary']; ?>
    </div>
    <?php if (isset($result['missing']) && count($result['missing'])): ?>
    <div class="search-result__text__missing search-result__text-spaced">
      <span class="search-result__text_left">Missing:</span>
      <span class="search-result__text_right">
        <?php foreach ($result['missing'] as $term): ?>
        <span class="search-result__text__missing-strikethrough"><?php print $term; ?></span>&nbsp;
        <?php endforeach; ?>
      </span>
    </div>
    <?php endif; ?>
    <div class="search-result__text__cta">
      <?php print $cta; ?>
      <span class="search-result__text_subtext">This trip is completely customizable</span>
    </div>
    <?php if ($result['explain']): ?>
    <div class="search-result__explain">
      <?php print $result['explain']; ?>
    </div>
    <?php endif; ?>
  </div>
  <?php if ($trigger): ?>
    <div class="search-rating-show"></div>
  <?php endif; ?>
</div>

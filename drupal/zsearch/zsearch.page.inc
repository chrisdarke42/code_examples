<?php

/**
 * Test Search form.
 *
 * @return mixed
 *   Form array.
 *
 * @throws \Exception
 */
function zsearch_elasticsearch() {
  $params = drupal_get_query_parameters();
  $search_term = '';
  $search = TRUE;
  if (isset($params['s'])) {
    $search_term = filter_xss(trim($params['s']));
  }
  if (isset($form_state['input']) && isset($form_state['input']['keywords'])) {
    $search = FALSE;
  }

  $results = TRUE;
  $devel_access = FALSE;

  if (strlen($search_term) && $search) {
    $devel_access = _zsearch_elasticsearch_devel($form);
    $page = 0;
    if (isset($params['page']) && is_numeric($params['page'])) {
      $page = $params['page'];
    }
    $output = _zsearch_elasticsearch_get_search_results($search_term, $page, $devel_access, $results);
  }
  else {
    $summary = theme('zsearch_elasticsearch_summary');
    $output = theme('zsearch_elasticsearch_results', array(
      'summary' => $summary,
    ));
    $results = FALSE;
  }

  $header_form = drupal_get_form('zsearch_elasticsearch_header_form', $search_term, $results, $devel_access);
  $footer_form = drupal_get_form('zsearch_elasticsearch_footer_form', $search_term, $results);

  $content = array(
    '#markup' => $output,
  );

  $build['search'] = array(
    '#prefix' => '<div id="search">',
    '#suffix' => '</div>',
    'header' => $header_form,
    'results' => $content,
    'footer' => $footer_form,
  );

  return $build;
}

/**
 * Get Search Results.
 *
 * @param string $search_term
 *   Search term.
 * @param int $page
 *   Page number.
 * @param bool $devel_access
 *   Devel Access flag.
 * @param array|bool $results
 *   Results array.
 *
 * @return string
 *   Rendered Search Results.
 *
 * @throws \Exception
 */
function _zsearch_elasticsearch_get_search_results($search_term, $page, $devel_access, &$results) {
  $results = FALSE;
  try {
    $search = new ToursElasticsearchConnector();
    $response = $search->search($search_term, $page);
    $summary = theme('zsearch_elasticsearch_summary', array(
      'num_results' => $response,
      'page' => $page,
    )
    );

    if ($response > 0) {
      ToursElasticsearchConnector::saveToCache($search);

      // Generate search result items.
      $results = $search->getResults();
      $trigger_position = (count($results) < 5) ? count($results) : 5;
      $results_html = '';
      foreach ($results as $index => $result) {
        $trigger = (($index + 1) === $trigger_position) ? TRUE : FALSE;
        $results_html .= _zsearch_elasticsearch_get_tour($result, $search, $devel_access, $trigger);
      }

      // No point in storing in session if its just one page.
      if ($search->getNumPages() > 1) {
        $_SESSION['search_params'] = $search_term;
      }
      pager_default_initialize($search->getNumberOfHitsForPager(), $search->getResultsPerPage());
      $pager = theme('pager', array('quantity' => $search->getNumberOfHitsForPager()));

      $output = theme('zsearch_elasticsearch_results', array(
        'results' => $results_html,
        'pager' => $pager,
        'summary' => $summary,
        'cache_id' => $search->getCacheId(),
        // Add devel data to search header.
        'debug' => _zsearch_elasticsearch_devel_header($search, $devel_access),
      ));
      $output .= theme('zsearch_elasticsearch_rating');
    }
    else {
      $output = theme('zsearch_elasticsearch_results', array(
        'summary' => $summary,
      ));
    }

    return $output;
  }
  catch (Exception $e) {
    return theme('zsearch_elasticsearch_error');
  }
}

/**
 * Get Devel Header information.
 *
 * @param object $search
 *   ElasticSearch object.
 * @param bool $devel_access
 *   Devel Access boolean.
 *
 * @return string
 *   Rendered header data.
 */
function _zsearch_elasticsearch_devel_header($search, $devel_access) {
  $output = '';
  if ($devel_access) {
    $cacheID = $search->getCacheId();
    $output = '<div class="eql-area es-devel-area" data-es-cache-id="' . $cacheID . '"><a class="es-devel-button" href="">Location Query</a><pre class="wrapped hidden">' . $search->getQueryJson('locations') . '</pre></div>';
    $locations = $search->getLocations();
    if (count($locations)) {
      $output .= '<div class="eql-area es-devel-area" data-es-cache-id="' . $cacheID . '"><a class="es-devel-button" href="">Locations</a><pre class="wrapped hidden">' . implode('<br>', $locations) . '</pre></div>';
    }
    $activities = $search->getActivities();
    if (count($activities)) {
      $output .= '<div class="eql-area es-devel-area" data-es-cache-id="' . $cacheID . '"><a class="es-devel-button" href="">Activities</a><pre class="wrapped hidden">' . implode('<br>', $activities) . '</pre></div>';
    }
    $output .= '<div class="eql-area es-devel-area" data-es-cache-id="' . $cacheID . '"><a class="es-devel-button" href="">Main Query</a><pre class="wrapped hidden">' . $search->getQueryJson('tours') . '</pre></div>';
    $output .= '<div class="eql-area es-devel-area" data-es-cache-id="' . $cacheID . '"><a class="es-devel-button" href="">Refine Query</a><pre class="wrapped hidden">' . $search->getQueryJson('refine') . '</pre></div>';
  }
  return $output;
}

/**
 * Get Tour Summary for search results.
 *
 * @param array $result
 *   Result data.
 * @param object $search
 *   ElasticSearch object.
 * @param bool $devel_access
 *   Devel access flag.
 * @param bool $trigger
 *   Trigger flag to identify if Rating Trigger displays at this render point.
 *
 * @return string
 *   Rendered tour item.
 *
 * @throws \Exception
 */
function _zsearch_elasticsearch_get_tour(array $result, $search, $devel_access, $trigger = FALSE) {
  $score = array();
  $explain = '';
  if ($devel_access) {
    $cacheID = $search->getCacheId();
    $scoreAdjusted = $search->getScoreAdjusted($result);
    $score = array('data-score' => $result['_score'] . ' - ' . $scoreAdjusted);
    $explain = '<div class="explain-area es-devel-area not-processed" data-es-cache-id="' . $cacheID . '"><a class="es-devel-button" href="">Explain</a></div>';
  }

  $data['id'] = $result['id'];
  $data['title'] = l($result['title'], 'node/' . $result['id'], array('attributes' => array_merge(array('target' => '_blank'), $score)));
  $data['summary'] = $result['summary'];
  $data['duration'] = $result['duration'];
  $data['missing'] = $result['_missing'];
  $data['locations'] = $result['locations'];
  $data['explain'] = $explain;

  return theme('zsearch_elasticsearch_tour', array('result' => $data, 'trigger' => $trigger));
}

/**
 * Elasticsearch Base form.
 *
 * @param string $search_term
 *   Search Term.
 *
 * @return array
 *   Form API array.
 */
function _zsearch_elasticearch_generic_form($search_term) {
  $form = array();

  $form['keywords'] = array(
    '#type' => 'textfield',
    '#title' => '',
    '#size' => 60,
    '#maxlength' => 60,
    '#default_value' => $search_term,
    '#required' => FALSE,
    '#attributes' => array('class' => array('search-form__text'), 'placeholder' => array(t('e.g. Italy, culture, food, wine, romance, Rome'))),
  );

  $form['submit_button'] = array(
    '#type' => 'submit',
    '#value' => t('Search'),
    '#attributes' => array('class' => array('search-form__button__submit')),
    '#prefix' => '<button class="search-form__button" name="search-button" value="search"><img class="search-form__button__icon" src="/sites/all/themes/custom/zictheme/images/search/search_icon.svg" alt="search icon">',
    '#suffix' => '</button>',
  );

  $form['#submit'][] = 'zsearch_elasticsearch_form_submit';

  return $form;
}

/**
 * Elasticsearch form for LTL pages.
 *
 * @param array $form
 *   Form API array.
 * @param array $form_state
 *   Form State array.
 *
 * @return array
 *   Form API array.
 */
function zsearch_elasticsearch_ltl_form(array $form, array &$form_state) {

  $form = _zsearch_elasticearch_generic_form('');

  $form['#attached']['js'][] = drupal_get_path('module', 'zsearch') . '/js/zsearch_defer_load.js';
  $form['#attached']['js'][] = drupal_get_path('module', 'zsearch') . '/js/zsearch_rate_results.js';
  $form['#attributes'] = array('class' => 'search-header_ltl');
  $form['#theme'] = 'zsearch_elasticsearch_form';
  return $form;
}

/**
 * Generate Header form for Search Page.
 *
 * @param array $form
 *   Form API array.
 * @param array $form_state
 *   Form State array.
 * @param string $search_term
 *   Search Term.
 * @param bool $results
 *   Flag to indicate if there are results.
 * @param bool $devel_access
 *   Devel Access boolean.
 *
 * @return array
 *   Form API array.
 */
function zsearch_elasticsearch_header_form(array $form, array &$form_state, $search_term = '', $results = FALSE, $devel_access = FALSE) {

  $form = _zsearch_elasticearch_generic_form($search_term);

  $form['#attached']['js'][] = drupal_get_path('module', 'zsearch') . '/js/zsearch_defer_load.js';
  $form['#attached']['js'][] = drupal_get_path('module', 'zsearch') . '/js/zsearch_rate_results.js';
  // JS and CSS only needed for Devel data.
  if ($devel_access) {
    $form['#attached']['js'][] = drupal_get_path('module', 'zsearch') . '/js/zsearch_result_explain.js';
    $form['#attached']['css'][] = drupal_get_path('module', 'zsearch') . '/css/zsearch_tours.css';
  }

  if ($results === FALSE) {
    $form['#attributes'] = array('class' => 'search-header_no-results');
  }

  $form['#theme'] = 'zsearch_elasticsearch_form';
  return $form;
}

/**
 * Generate Footer form for Search Page.
 *
 * @param array $form
 *   Form API array.
 * @param array $form_state
 *   Form State array.
 * @param string $search_term
 *   Search Term.
 * @param bool $results
 *   Flag to indicate if there are results.
 *
 * @return array
 *   Form API array.
 */
function zsearch_elasticsearch_footer_form(array $form, array &$form_state, $search_term, $results) {

  $form = _zsearch_elasticearch_generic_form($search_term);

  if ($results === FALSE) {
    $form['#attributes'] = array('class' => 'search-footer_no-results');
  }

  $form['#theme'] = 'zsearch_elasticsearch_form_footer_theme';

  return $form;
}

/**
 * Search Tours submit function.
 *
 * @param array $form
 *   Form array.
 * @param array $form_state
 *   Form state array.
 */
function zsearch_elasticsearch_form_submit(array $form, array &$form_state) {
  if (isset($form_state['values']['keywords'])) {
    $params = array();
    $params['s'] = $form_state['values']['keywords'];
    drupal_goto('luxury-tours/search', array('query' => $params));
  }
}

/**
 * Get Devel Access.
 *
 * @param array|null $form
 *   Form API array.
 *
 * @return bool
 *   Devel Access flag.
 */
function _zsearch_elasticsearch_devel(&$form) {
  $devel_access = FALSE;
  // Devel tools for listing.
  if ((module_exists('devel') && user_access('access devel information')) || (variable_get('zsearch_es_debug') === 1)) {
    $devel_access = TRUE;
    $form['#attached']['js'][] = drupal_get_path('module', 'zsearch') . '/js/zsearch_result_explain.js';
  }
  return $devel_access;
}

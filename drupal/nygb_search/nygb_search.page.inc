<?php

/**
 * Test Search form.
 *
 * @param array $form
 *   Form array.
 * @param array $form_state
 *   Form state array.
 *
 * @return mixed
 *   Form array.
 */
function nygb_search_global_form(array $form, array &$form_state, $collection_id = NULL, $structured = FALSE) {
  // Set up global search manager.
  $searchIndex = ElasticManager::load($collection_id, $structured);

  // Clear out any empty filters from unselected dropdowns.
  $params = array_filter(drupal_get_query_parameters());
  $searchIndex->getResultFormingParams($params);
  $filters = $searchIndex->getSearchFilters();

  // Get title and render if it exists
  $title = $searchIndex->getSearchTitle();
  if (strlen($title)) {
    $form['title']['#markup'] = '<h2>' . $title . '</h2>';
  }

  // Set Form storage for filters.
  $form_state['storage']['fieldsets'] = $filters;
  $form['#parents'] = [];

  $modifiers = $searchIndex->getSearchFieldModifiers();

  // Build free text fields for search.
  foreach($filters['text'] as $machine_name => $name) {
    $search_value = isset($params[$machine_name]) ? filter_xss(trim($params[$machine_name])) : '';
    $form[$machine_name] = array(
      '#type' => 'textfield',
      '#title' => $name,
      '#title_display' => 'invisible',
      '#attributes' => array(
        'placeholder' => $name,
      ),
      '#size' => 60,
      '#maxlength' => 60,
      '#default_value' => $search_value,
      '#required' => FALSE,
    );
    if (isset($modifiers[$machine_name])) {
      $modifier_key = $machine_name . '_modifier';
      $form[$modifier_key] = $modifiers[$machine_name];
      $search_modifier_value = isset($params[$modifier_key]) ? filter_xss(trim($params[$modifier_key])) : '';
      $form[$modifier_key]['#default_value'] = $search_modifier_value;
    }
  }

  // Check if this is the search results version of the form
  $resultsSet = TRUE;
  $path = current_path();
  if (strpos($path, 'online-records/search-results') === FALSE) {
    $resultsSet = FALSE;
  }

  // Filters exist?
  $filtersSet = FALSE;
  $filtersAvailable = FALSE;
  if ($resultsSet) {
    $filterCopy = $params;
    $default_drupal_form_keys = _nygb_search_drupal_form_keys();
    foreach($default_drupal_form_keys as $key) {
      unset($filterCopy[$key]);
    }
    $default_nygb_form_keys = $searchIndex->getSearchFields();
    foreach($default_nygb_form_keys as $key => $value) {
      unset($filterCopy[$key]);
    }
    if (count($filterCopy)) {
      $filtersSet = TRUE;
    }
  }
  else {
    if ((count($filters['select']) + count($filters['auto'])) > 0) {
      $filtersAvailable = TRUE;
    }
  }

  // Results have not yet been presented.
  if (!$resultsSet && $filtersAvailable) {

    $form['search_filters'] = array(
      '#type' => 'container',
      '#attributes' => array(
        'class' => array(
          'search-filters',
          'collapse',
        ),
      ),
      '#id' => 'search-filters-collapsible'
    );

    $form['search_filters']['heading'] = array(
      '#markup' => '<div class="advanced-search-helptext"><h4>' . t('Narrow your search by:') . '</h4><div class="help-block">Selecting an option below will filter your search results.</div></div>',
    );

    // Collect advanced filter fields for collapsible filter check.
    $advanced_filter_fields = [];

    // Build up filter select fields from Taxonomy Terms.
    foreach($filters['select'] as $index_key => $field) {
      $search_value = isset($params[$index_key]) ? filter_xss(trim($params[$index_key])) : '';
      $searchIndex->getSearchTermDropdown($index_key, $form, $field, $search_value);
      $advanced_filter_fields[] = $index_key;
    }

    // Build up filter autocomplete fields from Taxonomy Terms.
    foreach($filters['auto'] as $index_key => $field) {
      $search_value = _nygb_search_autocomplete_parse_get_params($index_key, $params,$form_state);
      $searchIndex->getSearchTermAutocomplete($index_key, $form, $field, $search_value);
      $advanced_filter_fields[] = $index_key;
    }

    // Check if the filters were set in previous validation step
    $advanced_filters_class = 'collapsed';
    $advanced_filters_aria = 'false';
    if (_nygb_search_advanced_filters_active($advanced_filter_fields, $form_state, $params)) {
      $advanced_filters_class = '';
      $advanced_filters_aria = 'true';
      $form['search_filters']['#attributes']['class'][] = 'in';
    }
    $form['search_filters']['#prefix'] = '<a class="advanced-search-button ' . $advanced_filters_class .'" data-toggle="collapse" href="#search-filters-collapsible" role="button" aria-expanded="' . $advanced_filters_aria . '" aria-controls="search-filters-collapsible">' . t('Advanced Search') . '</a>';

  }


  // Build up any hidden vars that need to be set.
  $searchIndex->getSearchHiddenVars($form);

  // Submit button
  $form['submit_button'] = array(
    '#type' => 'submit',
    '#value' => t('Search'),
    '#attributes' => array('class' => array('gray-button', 'search-form-button')),
  );

  // Show reset filters if the search was run
  if ($resultsSet && $filtersSet) {
    $form['reset_filters'] = array(
      '#type'   => 'submit',
      '#value'    => 'Clear Filters',
      '#attributes' => array('class' => array('gray-button', 'search-form-button', 'clear-filters')),
    );

    $form['reset_filters_duplicate'] = array(
      '#type'   => 'submit',
      '#value'    => 'Clear Filters',
      '#weight' => 999,
      '#attributes' => array('class' => array('gray-button', 'search-form-button', 'clear-filters', 'clear-filters__bottom')),
    );
  }


  // Collections link for search initial page.
  if (!$resultsSet && !isset($collection_id)) {
    $form['collection_link'] = array(
      '#markup' => '<a href="#block-block-141" class="btn anchor-link">' . t('Click here to see all available collections') . '</a>',
    );
  }

  // Load facet javascript.
  if ($searchIndex->getSearchFacets($form, $params)) {
    $jsFile = 'facetsCollapse.js';
    $form['#attached']['js'] = array(
      drupal_get_path('module', 'nygb_search') . '/js/' . $jsFile,
    );
  }

  return $form;
}

function nygb_search_results_form(array $form, array &$form_state, $collection_id = NULL, $structured = FALSE) {
  // Set up global search manager.
  $searchIndex = ElasticManager::load($collection_id, $structured);

  $search = TRUE;
  $params = drupal_get_query_parameters();
  $searchIndex->getResultFormingParams($params);
  if (isset($form_state['input']) && count($form_state['input'])) {
    $params = $form_state['input'];
    if (!isset($form_state['submitted']) || !$form_state['submitted']) {
      $search = FALSE;
    }
  }

  if (!count($params)) {
    $search = FALSE;
  }

  if ($search && $searchIndex->searchMatchesRequirements($form, $params)) {

    try {
      $searchIndex->searchWrapper($form_state);
      $hits = $searchIndex->getTotalResponse();
      if ($hits) {
        $results = $searchIndex->getResults();
        $output = implode('<br>', $results);
      }
      else {
        $output = t('No results');
      }
      $form['#attached']['css'][] = drupal_get_path('module', 'nygb_search') . '/css/base.css';
      $form['results']['#markup'] = $output;
    }
    catch (Exception $e) {
      $form['results']['#markup'] = $e->getMessage();
    }
  }

  $full_width_warning = theme('html_tag', array(
      'element' => array(
        '#tag' => 'div',
        '#attributes' => array(
          'class' => 'search-results-header-messaging__full-screen-experience',
        ),
        '#value' => t('The NYG&B\'s Online Records Platform is best experienced using 
     a laptop or desktop. </br>If you experience issues using a tablet or other mobile
     device contact <a href="@webmaster-link">webmaster@nygbs.org</a>.', array('@webmaster-link' => 'mailto:webmaster@nygbs.org'))
      ),
    )
  );
  $form['#prefix'] = theme('html_tag', array(
      'element' => array(
        '#tag' => 'div',
        '#attributes' => array(
          'class' => 'search-results-header-messaging',
        ),
        '#value' => $full_width_warning
      ),
    )
  );

  $form['#suffix'] = theme('nygb_search_help', array('content' => variable_get('nygb_search_help_text_indexed', ''), 'index_name' => 'collection_structured_data')) .
    theme('nygb_search_help', array('content' => variable_get('nygb_search_help_text_fulltext', ''), 'index_name' => 'collection_files_pages'));
  return $form;
}

function _nygb_search_meets_requirements($form, $params, $textfields) {
  foreach($textfields as $machine_name => $name) {
    $search_val = isset($params[$machine_name]) ? filter_xss(trim($params[$machine_name])) : '';
    if (strlen($search_val)) {
      return TRUE;
    }
  }
  form_error($form, t('Please enter search text.'));
  return FALSE;
}

function _nygb_search_advanced_filters_active($filters, &$form_state, $params) {
  if (isset($form_state['input'])) {
    foreach($filters as $filter_field) {
      if (isset($form_state['input'][$filter_field]) && strlen($form_state['input'][$filter_field])) {
        return TRUE;
      }
    }
  }
  if (count($params)) {
    foreach($filters as $filter_field) {
      if (isset($params[$filter_field]) && strlen($params[$filter_field])) {
        return TRUE;
      }
    }
  }
  return FALSE;
}

/**
 * Global Search validation function.
 *
 * @param array $form
 *   Form array.
 * @param array $form_state
 *   Form state array.
 */
function nygb_search_global_form_validate(array $form, array &$form_state) {
  $params = [];
  if (isset($form_state['input'])) {
    $params = $form_state['input'];
  }
  $textfields = array();
  if (isset($form_state['storage']['fieldsets']['text'])) {
    $textfields = $form_state['storage']['fieldsets']['text'];
  }
  _nygb_search_meets_requirements($form, $params, $textfields);
}

function _nygb_search_drupal_form_keys() {
  return ['op', 'form_build_id', 'form_token', 'form_id'];
}

/**
 * Global Search submit function.
 *
 * @param array $form
 *   Form array.
 * @param array $form_state
 *   Form state array.
 */
function nygb_search_global_form_submit(array $form, array &$form_state) {
  $inputs = $form_state['input'];

  $current_params = drupal_get_query_parameters();

  $searchIndex = ElasticManager::load();
  $search_fields = $searchIndex->getSearchFields();
  $search_fields_modifiers = $searchIndex->getSearchFieldModifiers();
  $modifier_keys = [];
  foreach($search_fields_modifiers as $modifier_key => $value) {
    $modifier_keys[] = $modifier_key . '_modifier';
  }
  $keys = array_merge(array_keys($search_fields), $modifier_keys, _nygb_search_drupal_form_keys());
  $keysToRemove = array_diff(array_keys($inputs), $keys);
  if ($form_state['input']['op'] === 'Clear Filters') {
    // Reset filter values.
    $inputs = array_diff_key($inputs, array_flip($keysToRemove));
  }
  else {
    $filter_button = array_filter($inputs, function ($e) {
      return (substr($e, -14) === '_submit_button');
    }, ARRAY_FILTER_USE_KEY);
    if (count($filter_button) > 0) {
      // We have a filter activation, assume only one
      $button_key = key($filter_button);
      $filter = substr($button_key, 0, -14);
      $keysToRemove = array_flip($keysToRemove);
      unset($keysToRemove[$button_key]);
      unset($keysToRemove[$filter]);
      $keysToRemove = array_flip($keysToRemove);
      foreach ($keysToRemove as $filters) {
        if (isset($current_params[$filters])) {
          $inputs[$filters] = $current_params[$filters];
        }
        else {
          unset($inputs[$filters]);
        }
      }
    }
  }

  $form_state['redirect']= array(
    variable_get('nygb_search_results_uri', 'online-records/search-results'),
    array(
      'query' => $inputs,
      'fragment' => 'optional',
    ),
  );
}

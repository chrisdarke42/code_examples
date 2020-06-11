<?php

/**
 * @file
 * ElasticManager class file.
 *
 * This file contains the class that all the Elasticsearch functionality is
 * based off.
 */

/**
 * Class ElasticManager
 */
class ElasticManager {

  protected $collection;

  protected $global = TRUE;

  protected $structured;

  protected $dataIndices;
  protected $fileIndex;

  protected $params;
  protected $response;

  protected $facets;

  /**
   * ElasticManager constructor.
   *
   * @param null $collection_id
   * @param bool $structured
   *
   * @throws \Exception
   */
  public function __construct($collection_id = NULL, $structured = FALSE) {
    $this->indexName = 'collection';
    if (isset($collection_id)) {
      $collection = node_load($collection_id);
      if ($collection && $collection->type === 'collection') {
        $this->collection = $collection;
        $this->global = FALSE;
      }
      else {
        return FALSE;
      }
    }
    $this->setDataIndex();
  }

  /**
   * Cache Manager for Search.
   *
   * @param null $collection_id
   * @param bool $structured
   *
   * @return \ElasticManager
   * @throws \Exception
   */
  public static function load($collection_id = NULL, $structured = FALSE) {
    $cache = &drupal_static(__METHOD__);
    $cache_key = (string) $collection_id . ':' . $structured;
    if (!empty($cache) && !empty($cache[$cache_key])) {
      return $cache[$cache_key];
    }
    $object = new ElasticManager($collection_id, $structured);
    $cache[$cache_key] = $object;
    return $object;
  }

  /**
   * Set DataIndex.
   */
  protected function setDataIndex() {
    try {
      $this->dataIndices[] = new StructuredElasticsearch();
    }
    catch(Exception $e) {
      watchdog('NYGB Search', 'Error loading Structured Search: %error', array('%error' => $e->getMessage()));
    }
    try {
      $this->dataIndices[] = new PageElasticsearch();
    }
    catch(Exception $e) {
      watchdog('NYGB Search', 'Error loading Text Search: %error', array('%error' => $e->getMessage()));
    }
    if (!$this->global) {
      foreach($this->dataIndices as $index) {
        $index->initPrecursor($this->collection->nid);
      }
      $this->initPrecursor();
    }
  }

  /**
   * Get Data Indices.
   *
   * @return mixed
   */
  protected function getDataIndices() {
    return $this->dataIndices;
  }

  /**
   * Initialize precursor object.
   *
   * @return \FileElasticsearch|null
   */
  public function initPrecursor() {
    if (isset($this->fileIndex)) {
      return $this->fileIndex;
    }
    $precursor = NULL;
    try {
      $precursor = new FileElasticsearch($this->collection->nid);
      $this->fileIndex = $precursor;
    }
    catch(Exception $e) {
      watchdog('NYGB Search', 'Error loading File Precursor Search: %error', array('%error' => $e->getMessage()));
    }
    return $precursor;
  }

  /**
   * Build a list of filters.
   *
   * @return mixed
   */
  public function getSearchFilters($type = NULL) {
    if ($this->global) {
      $search_parameters = [];
      $search_parameters['collection'] = variable_get('nygb_search_collection_fields_searchable', []);
      $filters = $this->getSearchParameterFields($search_parameters);
      $filters['text'] = array_merge($this->getSearchFields(), $filters['text']);
    }
    else {
      $collection_wrapper = entity_metadata_wrapper('node', $this->collection);
      $search_parameters = json_decode($collection_wrapper->field_search_parameters->value(), TRUE);
      $filters = $this->getSearchParameterFields($search_parameters);
      $filters['text'] = array_merge($this->getSearchFields(), $filters['text']);
    }

    if (isset($type) && isset($filters[$type])) {
      return $filters[$type];
    }
    return $filters;
  }

  /**
   * Search Parameter to Search Fields.
   *
   * Turn the array from search_parameters field in Collections into a pair of
   * arrays of search fields for the form.
   *
   * @param array $search_parameters
   *   Parameter array.
   *
   * @return array
   *   Field array.
   */
  function getSearchParameterFields($search_parameters) {
    $textfields = [];
    $autocompletefields = [];
    $selectfields = [];
    $facetfields = [];
    if (!$this->global) {
      if ($this->structured) {
        $types = ['collection', 'structured_data'];
      }
      else {
        $types = ['collection_file', 'collection'];
      }
    }
    else {
      $types = ['collection_file', 'collection', 'structured_data'];
    }
    foreach($types as $bundle_name) {
      if (!isset($search_parameters[$bundle_name])) {
        continue;
      }
      $filtered_set = array_filter($search_parameters[$bundle_name]);
      foreach ($filtered_set as $parameter) {
        if ($bundle_name === 'structured_data') {
          list($field_name, $display_name, $type) = explode('|', $parameter);
          $textfields[$field_name] = $display_name;
        }
        else {
          list($field_name, $index_field_name, $type) = explode('|', $parameter);
          if ($type === 'collection_file') {
            $entity_type = 'file';
          }
          else {
            $entity_type = 'node';
          }
          $field_instance_info = field_info_instance($entity_type, $field_name, $bundle_name);
          $field_info = field_info_field($field_name);
          if ($field_info['type'] === 'taxonomy_term_reference') {
            $vocab_machinename = $field_info['settings']['allowed_values'][0]['vocabulary'];
            $vocabulary = taxonomy_vocabulary_machine_name_load($vocab_machinename);
            $vocab_name = $vocabulary->name;
            $vocab_name = $this->transformFieldNames($vocab_name);
            if ($type === 'select') {
              $selectfields[$index_field_name] = array(
                'vocab' => $vocab_machinename,
                'name' => $vocab_name,
              );
            }
            if ($type === 'autocomplete') {
              $autocompletefields[$index_field_name] = array(
                'vocab' => $vocab_machinename,
                'name' => $vocab_name,
              );
            }
            if ($type === 'facets') {
              $facetfields[$index_field_name] = ucwords($field_instance_info['label']);
            }
          }
          else {
            if ($type === 'text') {
              $textfields[$index_field_name] = ucwords($field_instance_info['label']);
            }
            if ($type === 'facets') {
              $facetfields[$index_field_name] = ucwords($field_instance_info['label']);
            }
            if ($type === 'select') {
              $selectfields[$index_field_name] = ucwords($field_instance_info['label']);
            }
          }
        }


      }
    }
    return [
      'select' => $selectfields,
      'auto' => $autocompletefields,
      'text' => $textfields,
      'facets' => $facetfields,
    ];
  }

  protected function transformFieldNames($vocab_name) {
    switch ($vocab_name) {
      case 'Collection Category':
        $vocab_name = 'Category';
        break;
    }
    return $vocab_name;
  }

  /**
   * Get names of fields.
   *
   * @return array|false
   */
  protected function getFieldNames() {
    $field_names = array_filter(array_merge($this->getSearchFilters('select'), $this->getSearchFilters('auto')));
    $field_display = array_combine(array_keys($field_names), array_column($field_names, 'name'));
    return $field_display;
  }

  /**
   * Build list of text search fields.
   *
   * @param array $text_fields
   *
   * @return array
   */
  public function getSearchFields() {
    $default_fields = [
      'first_name' => 'First Name',
      'last_name' => 'Last Name',
      'keywords' => 'Keywords / Location',
    ];
    return $default_fields;
  }

  /**
   * Get search field modifiers, that can change how a field/filter behave.
   *
   * @return array
   *   Array of modifiers.
   */
  public function getSearchFieldModifiers() {
    $indicies = $this->getDataIndices();
    $modifiers = [];
    foreach($indicies as $index) {
      if (method_exists($index, 'getSearchFieldModifiers')) {
        $index->getSearchFieldModifiers($modifiers);
      }
    }
    return $modifiers;
  }

  /**
   * Get the title for a search.
   *
   * @return string|null
   *   Title of search.
   */
  public function getSearchTitle() {
    if ($this->global) {
      if (isset($this->response)) {
        $title = t('Your Search');
      }
      else {
        $title = t('Search');
      }
    }
    else {
      $title = t('Search this Collection');
//      $collection_wrapper = entity_metadata_wrapper('node', $this->collection);
//      $title = t(
//        '%collection_name - Search',
//        array('%collection_name' => $collection_wrapper->title->value()));
    }
    return $title;
  }

  /**
   * Get query facets for a specific index.
   *
   * @param $index_key
   *
   * @return array|bool
   *   Facet data.
   */
  protected function getQueryFacets($index_key) {
    if (!$this->global && isset($this->fileIndex)) {
      return $this->fileIndex->getQueryFacets($index_key);
    }
    return FALSE;
  }

  /**
   * Build a dropdown for a search.
   *
   * @param string $index_key
   *   The index key.
   * @param array $form
   *   Form array.
   * @param array $field
   *   Field name.
   * @param null|string $default
   *   Default value.
   */
  public function getSearchTermDropdown($index_key, &$form, $field, $default = NULL) {
    $terms = $this->getQueryFacets($index_key);

    // If its a field with no properties it shouldn't be a search term dropdown.
    if (!isset($field['name'])) {
      return;
    }

    $vocabulary = taxonomy_vocabulary_machine_name_load($field['vocab']);
    $tree = taxonomy_get_tree($vocabulary->vid);

    $options = [];
    $parents = [];

    foreach($tree as $term) {
      if (count($term->parents) && $term->parents[0] > 0) {
        $parents[$term->parents[0]][] = $term->tid;
      }
      $options[$term->tid] = $term->name;
    }

    if (count($parents)) {
      foreach($parents as $ptid => $children) {
        $parent_prefix = $options[$ptid] . ' > ';
        unset($options[$ptid]);
        foreach ($children as $child) {
          $options[$child] = $parent_prefix . $options[$child];
        }
      }
    }
    if ($terms !== FALSE) {
      // We have term matches possible for this doc/key.
      $options = array_intersect_key($options, $terms);
    }

    $class_suffix = drupal_clean_css_identifier($index_key);
    $options = [NULL => t('--Select--')] + $options;
    $form['search_filters'][$index_key] = array(
      '#prefix' => '<div class="search-filter search-filter--' . $class_suffix . '">',
      '#suffix' => '</div>',
      '#type' => 'select',
      '#title' => t($field['name']),
      '#options' => $options,
      '#default_value' => $default,
    );
  }

  /**
   * Build an autocomplete text field for a search.
   *
   * @param string $index_key
   *   The index key.
   * @param array $form
   *   Form array.
   * @param string $field
   *   Field name.
   * @param null|string $default
   *   Default value.
   */
  public function getSearchTermAutocomplete($index_key, &$form, $field, $default = NULL) {
    $autocomplete_path = 'admin/nygb_search/autocomplete/vocab/' . $field['vocab'];
    if (!$this->global) {
      $collection_id = $this->collection->nid;
      $autocomplete_path = 'admin/nygb_search/autocomplete/vocabsubset/' . $field['vocab'] . '/' . $collection_id . '/' . $index_key;
    }

    $term_name = '';
    if (isset($default)) {
      if (is_numeric($default)) {
        $term = taxonomy_term_load($default);
        if ($term !== FALSE) {
          $term_name = $term->name;
        }
      }
      else {
        $term_name = $default;
      }
    }

    $class_suffix = drupal_clean_css_identifier($index_key);
    $form['search_filters'][$index_key] = array(
      '#prefix' => '<div class="search-filter search-filter--' . $class_suffix . '">',
      '#suffix' => '</div>',
      '#type' => 'textfield',
      '#default_value' => $term_name,
      '#title' => t($field['name']),
      '#autocomplete_path' => $autocomplete_path,
      '#maxlength' => 1024,
      '#element_validate' => array('nygb_search_autocomplete_validate'),
    );
  }

  /**
   * Build facet checkboxes for a search.
   *
   * @category Facet Filters.
   */
  public function getSearchFacets(&$form, $params) {
    if ($facetData = $this->getFacets()) {
      foreach($facetData as $facet_family => $values) {
        // Get selected values if available.
        $default_values = [];
        list($name, $type, $filter) = explode('|', $facet_family);
        if (strlen($filter) && isset($params[$filter])) {
          $data = [];
          if (!is_array($params[$filter])) {
            $data[$params[$filter]] = $params[$filter];
          }
          else {
            $data = $params[$filter];
          }
          $default_values = $data;
        }

        // Sort Facet fields by value
        $keys = array_keys($values);
        array_multisort(
          array_column($values, 'count'), SORT_DESC, SORT_NUMERIC, $values, $keys
        );
        $values = array_combine($keys, $values);

        // Getting checkboxes for options.
        $options = $this->getFacetCheckboxes($values);

        // Seperate checked elements from unchecked.
        $checked = [];
        foreach($default_values as $key => $val) {
          if (isset($options[$key])) {
            $checked[$key] = $options[$key];
            unset($options[$key]);
          }
        }

        // Place checked elements at start of checkboxes.
        $sorted_options = $checked + $options;

        $form[$filter] = array(
          '#prefix' => '<div class="search-filters">',
          '#suffix' => '</div>',
          '#type' => 'checkboxes',
          '#options' => $sorted_options,
          '#attributes' => array(
            'class' => array('search-facets'),
          ),
          '#title' => t('Refine by %facetname', array('%facetname' => $this->facetTitles($name))),
          '#default_value' => $default_values,
        );
        $form[$filter . '_submit'] = array(
          '#name' => $filter . '_submit_button',
          '#type' => 'submit',
          '#attributes' => array(
            'class' => array('btn-primary'),
          ),
          '#value' => t('Refine by @facetname', array('@facetname' => $this->facetTitles($name))),
        );

      }
      return TRUE;
    }
    return FALSE;
  }

  /**
   * Return titles for facet display.
   * @todo this could be placed into config.
   *
   * @param string $name
   *   Facet name.
   *
   * @return string
   *   Facet title.
   */
  protected function facetTitles($name) {
    switch($name) {
      case 'collection':
        return 'Collection';
      case 'collection_category':
        return 'Category';
      case 'location':
        return 'County';
    }
    return $name;
  }

  /**
   * Get facet checkboxes array.
   *
   * @param array $facet_values
   *   Facet values from data.
   *
   * @return array
   *   Array of checkbox values for Form API.
   */
  protected function getFacetCheckboxes($facet_values) {
    $options = [];
    foreach ($facet_values as $id => $data) {
      $options[$id] = $data['label'] . ' (' . $data['count'] . ')';
    }
    return $options;
  }

  /**
   * Get Hidden search parameters.
   *
   * @return array
   */
  protected function getSearchHiddenParams() {
    if (!$this->global) {
      return ['field_collection:nid' => $this->collection->nid];
    }
    return [];
  }

  /**
   * Set hidden field values on form.
   *
   * @param array $form
   *   Form array.
   */
  public function getSearchHiddenVars(&$form) {
    $params = $this->getSearchHiddenParams();
    foreach($params as $key => $value) {
      $form[$key] = array(
        '#type' => 'hidden',
        '#value' => $value,
      );
    }
  }

  /**
   * Check if the current search meets minimum requirements to proceed.
   *
   * @param array $form
   *   Form array.
   * @param array $params
   *   Parameters array.
   *
   * @return bool
   *   Return true if the search matches minimum requirements.
   */
  public function searchMatchesRequirements(&$form, $params) {
    $text_fields = $this->getSearchFilters('text');

    foreach($text_fields as $machine_name => $name) {
//      $modifier_machine_name = $machine_name . '_modifier';
//      if (isset($modifiers[$machine_name]) && isset($params[$modifier_machine_name])) {
//        $this->params = $params;
//      }
      $search_val = isset($params[$machine_name]) ? filter_xss(trim($params[$machine_name])) : '';
      if (strlen($search_val)) {
        $this->params = $params;
        return TRUE;
      }
    }
    form_error($form, t('Please enter search text.'));
    return FALSE;
  }

  /**
   * Process page number information to set page number in object.
   */
  protected function parsePageNumbers() {
    $params = $this->params;
    $dataIndices = $this->getDataIndices();
    if (isset($params['page'])) {
      $pages = explode(',', $params['page']);
    }
    foreach($dataIndices as $key => &$index) {
      $page = 0;
      if (isset($pages[$key]) && is_numeric($pages[$key])) {
        $page = $pages[$key];
      }
      $index->setPageNumber($page);
    }
  }

  /**
   * Run the Search on the inidices.
   */
  public function searchWrapper($form_state) {
    $params = $this->params;
    $this->parsePageNumbers();
    $search_terms = [];
    // Get Filter information.
    $filters = $this->getSearchFilters();
    foreach($filters['text'] as $key => $val) {
      if (array_key_exists($key, $params)) {
        $raw = $params[$key];
        if (strlen($raw)) {
          $search_terms[$key] = filter_xss(trim($params[$key]));
        }
      }
    }

    $modifiers = $this->getSearchFieldModifiers();
    $filter_modifiers = [];
    foreach($modifiers as $key => $modifier) {
      $modifier_machine_name = $key . '_modifier';
      if (isset($params[$modifier_machine_name])) {
        $filter_modifiers[$key] = $params[$modifier_machine_name];
      }
    }

    $search_filters = [];
    foreach($filters['auto'] as $key => $val) {
      if (array_key_exists($key, $params)) {
        if (isset($form_state['storage']['autocomplete_values']) && array_key_exists($key, $form_state['storage']['autocomplete_values'])) {
          $search_filters[$key]['tid'] = filter_xss(trim($form_state['storage']['autocomplete_values'][$key]));
        }
        else {
          $raw = $params[$key];
          if (is_array($raw)) {
            foreach($raw as $tid) {
              $search_filters[$key][]['name'] = filter_xss(trim($tid));
            }
          } elseif (strlen($raw) && ($raw > 0)) {
            $search_filters[$key][]['name'] = filter_xss(trim($raw));
          }
        }
      }
    }

    $processed_filters = [];
    foreach($filters['select'] as $key => $field) {
      if (array_key_exists($key, $params)) {
        $raw = $params[$key];
        if (is_array($raw)) {
          foreach($raw as $val) {
            if ($key === 'field_collection:nid') {
              $search_filters[$key][]['nid'] = filter_xss(trim($val));
            }
            else {
              $search_filters[$key][]['tid'] = filter_xss(trim($val));
            }
          }
        } elseif (strlen($raw) && ($raw > 0)) {
          if ($key === 'field_collection:nid') {
            $search_filters[$key][]['nid'] = filter_xss(trim($raw));
          }
          else {
            $search_filters[$key][]['tid'] = filter_xss(trim($raw));
          }
        }
        $processed_filters[$key] = $key;
      }
    }

    foreach($filters['facets'] as $key => $field) {
      if (isset($processed_filters[$key])) {
        continue;
      }
      if (array_key_exists($key, $params)) {
        $raw = $params[$key];
        $keyparts = explode(':', $key);
        $paramtype = 'tid';
        if (count($keyparts) > 1 && end($keyparts) === 'nid') {
          $paramtype = 'nid';
        }
        if (is_array($raw)) {
          foreach($raw as $kid) {
            $search_filters[$key][][$paramtype] = filter_xss(trim($kid));
          }
        }
        elseif (strlen($raw) && ($raw > 0)) {
          $search_filters[$key][][$paramtype] = filter_xss(trim($raw));
        }
      }
    }

//    $hidden_fields = $this->getSearchHiddenParams();
//    foreach($hidden_fields as $key => $field) {
//      if (array_key_exists($key, $params)) {
//        $raw = $form_state['input'][$key];
//        if (strlen($raw) && ($raw > 0)) {
//          $search_filters[$key] = filter_xss(trim($raw));
//        }
//      }
//    }
    $this->runSearchOnDataIndices($search_terms, $search_filters, $filter_modifiers);

  }

  /**
   * Run search.
   *
   * @param array $search_terms
   *   Search terms.
   * @param array $search_filters
   *   Search filters.
   * @param array $filter_modifiers
   *   Search term/filter modifiers.
   */
  protected function runSearchOnDataIndices($search_terms, $search_filters, $filter_modifiers) {
    $dataIndices = $this->getDataIndices();
    $response = [];
    foreach($dataIndices as $key => $index) {
      $search_terms_instance = $search_terms;
      $response[$key] = $index->search($search_terms_instance, $search_filters, $filter_modifiers);
    }
    $this->response = $response;
  }

  /**
   * Get total number of hits.
   *
   * @return int
   *   Number of hits.
   */
  public function getTotalResponse() {
    $hits = 0;
    if (!empty($this->response)) {
      foreach($this->response as $type => $hits_partial) {
        $hits += $hits_partial;
      }
    }
    return $hits;
  }

  /**
   * Get search results.
   *
   * @return array
   *   Search result array.
   */
  public function getResults() {
    $dataIndices = $this->getDataIndices();
    $output = [];
    $params = $this->params;
    foreach($dataIndices as $key => &$index) {
      $output[$key] = $index->getResultSet($this->getFieldNames(), $params, $key + 1);
    }
    return $output;
  }

  /**
   * Get Search Facets for current search.
   *
   * @return bool|array
   *   FALSE if no facets, or array of facets.
   *
   * @category Facet Filters.
   */
  public function getFacets() {
    if ($this->getTotalResponse() === 0) {
      return FALSE;
    }


    if (isset($this->facets)) {
      return $this->facets;
    }
    $dataIndices = $this->getDataIndices();
    $facets = [];
    foreach($dataIndices as $key => &$index) {
      if (method_exists($index, 'getFacets')) {
        $facets = $this->facetSum($facets, $index->getFacets());
      }
    }
    $this->populateFacetMetadata($facets);
    return $this->facets;
  }

  protected function populateFacetMetadata($facets) {
    foreach($facets as $facet_family => &$facet_data) {
      list($name, $type, $filter) = explode('|', $facet_family);
      // Get the loading function for this type of facet content

      // If we are to show all facets we need to load them before adding in the specific values.
      if (variable_get('nygb_search_show_all_facets', FALSE)) {
        $result = [];
        switch($type) {
          case 'taxonomy_term':
            // Return all nids of nodes of type "page".
            $query = db_select('taxonomy_term_data', 'tt');
            $query->join('taxonomy_vocabulary', 'v', 'v.vid = tt.vid');
            $query->condition('v.machine_name', $name)
              ->fields('tt', ['tid', 'name']);
            $exec_query = $query->execute();
            $result = $exec_query->fetchAllKeyed(0, 1);
            break;


          case 'node':
            // Return all nids of nodes of type "page".
            $result = db_select('node', 'n')
              ->fields('n', ['nid', 'title'])
              ->condition('n.type', $name)
              ->execute()
              ->fetchAllKeyed(0, 1);
            break;
        }

        array_walk(
          $result,
          function ($item, $key) use (&$facet_data) {
            if (isset($facet_data[$key])) {
              $facet_data[$key]['label'] = $item;
            }
            else {
              $facet_data[$key]['label'] = $item;
              $facet_data[$key]['count'] = 0;
            }
          }
        );
      }
      else {
        switch($type) {
          case 'taxonomy_term':
            $func = 'taxonomy_term_load';
            break;
          case 'node':
            $func = 'node_load';
            break;
        }

        foreach ($facet_data as $facet_item => &$facet_item_data) {
          $entity = $func($facet_item);
          if (is_object($entity)) {
            $wrapper = entity_metadata_wrapper($type, $entity);
            $facet_item_data['label'] = $wrapper->label();
          }
        }
      }
    }
    $this->facets = $facets;
  }


  protected function facetSum($array1, $array2) {
    $sum = array_fill_keys(array_keys($array1 + $array2), 0);
    foreach($sum as $key => &$data) {
      $a1 = isset($array1[$key]) ? $array1[$key] : [];
      $a2 = isset($array2[$key]) ? $array2[$key] : [];
      $data = array_fill_keys(array_keys($a1 + $a2), []);
      foreach($data as $facet_key => &$facet_data) {
        $value1 = isset($a1[$facet_key]) ? $a1[$facet_key]['count'] : 0;
        $value2 = isset($a2[$facet_key]) ? $a2[$facet_key]['count'] : 0;
        $facet_data['count'] = $value1 + $value2;
      }
    }
    return $sum;
  }

//  /**
//   * Getter for {$resultsPerPage}.
//   *
//   * @return int
//   *   Value of {$resultsPerPage}.
//   */
//  public function getResultsPerPage() {
//    dpm('get results per page');
//    if (isset($this->results_per_page)) {
//      return $this->results_per_page;
//    }
//    if ($this->global) {
//      $results = variable_get('nygb_search_results_per_page_global', 10);
//    }
//    else {
//      $results = variable_get('nygb_search_results_per_page_collection', 40);
//    }
//    return $results;
//  }

  public static function isDebug() {
    return variable_get('nygb_search_debug', FALSE);
  }

  public function getResultFormingParams($params) {
    if (isset($params['rescount']) && is_numeric($params['rescount']) && ($params['rescount'] < 200)) {
      $rescount = filter_xss($params['rescount']);
      $dataIndices = $this->getDataIndices();
      foreach($dataIndices as $key => $index) {
        $index->setResultsPerPage($rescount);
      }
    }
  }
}

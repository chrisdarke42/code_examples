<?php

class StructuredElasticsearch extends BaseElasticsearch {


  protected $collection_index;

  protected $collection_id;

  protected $results;

  protected $facets;

  /**
   * StructuredElasticsearch constructor.
   *
   * @throws \Exception
   */
  public function __construct() {
    if (variable_get('nygb_search_structured_disabled', FALSE)) {
      throw new Exception(t('Structured Search disabled.'));
    }
    $this->indexName = 'collection_structured_data';
    parent::__construct();
  }

  public function initPrecursor($collection_id = NULL) {
    $precursor = new NodeElasticsearch();
    $this->collection_index = $precursor;
    if (isset($collection_id)) {
      $this->collection_id = $collection_id;
    }
    if (isset($this->collection_id)) {
      return $this->collection_id;
    }
    if (isset($this->collection_index)) {
      return $this->collection_index;
    }
  }

  /**
   * Search Parameter to Search Fields.
   *
   * Turn the array from search_parameters field in Collections into a pair of
   * arrays of search fields for the form.
   *
   * @param $search_parameters
   */
  function search_parameters_to_fields($search_parameters) {
    $textfields = [];
    $autocompletefields = [];
    $selectfields = [];
    foreach ($search_parameters['structured_data'] as $parameter) {
      list($index_field_name, $field_label, $type) = explode('|', $parameter);
      $textfields[$index_field_name] = ucwords($field_label);
    }
    return [
      'select' => $selectfields,
      'auto' => $autocompletefields,
      'text' => $textfields
    ];
  }

  /**
   * Build list of text search fields.
   *
   * @param array $text_fields
   *
   * @return array
   */
  function search_fields($text_fields = []) {
    $default_fields = [
      'first_name' => 'First Name',
      'last_name' => 'Last Name',
    ];
    return array_merge($text_fields, $default_fields);
  }

  function getSearchFieldModifiers(&$modifiers) {
    $modifiers['last_name'] = array(
      '#type' => 'checkbox',
      '#title' => t('Use exact spelling'),
    );
  }

  function searchFieldModifierTypes($search_terms, $modifiers) {
    $keys = [];
    foreach($modifiers as $key => $value) {
      if ($value == 1) {
        // Currently the only modifier is 'raw'
        if (isset($search_terms[$key]) && strlen($search_terms[$key])) {
          $keys[$key] = 'raw';
        }
      }
    }
    return $keys;
  }

  /**
   * Run the search.
   *
   * @param string $term
   *   Term to search.
   * @param int $page
   *   Page number.
   *
   * @return bool|int
   *   Number of hits.
   *
   * @throws \Exception
   *   Thrown if there is an issue running search.
   */
  public function search($terms, $filters = array(), $filter_modifiers = array()) {
    $page = $this->page_number;
    $must_array = [];
    $should_array = [];
    $filters_array = [];
    if (!is_array($terms)) {
      $should_array[] = $terms;
    }
    else {
      foreach($terms as $key => $term) {
        $must_array[$key] = $term;
      }
    }

    $precursor = $this->initPrecursor();

    if (is_numeric($precursor)) {
      $collection_ids[] = $precursor;
    }
    else {
      $collection_ids = [];
      // Check if we have any Collection IDs from Filters.
      if (count($filters)) {
        // Run a precursor search to identify Collection IDs.
        $response = $precursor->searchFiltered(array(), $filters);
        if ($response > 0) {
          $collection_ids = $precursor->parsePrecursorQueryNids();
        }
        else {
          // If filters didn't match anything, stop here.
          return 0;
        }
      }
    }

    // If we have fids, add a filter for that.
    if (count($collection_ids)) {
      $filters_array = array('terms' => ['collection_id' => $collection_ids]);
    }

    // Get any modifiers that will affect which data type is used for search.
    $modifier_types = $this->searchFieldModifierTypes($terms, $filter_modifiers);

    // Create main query.
    $bodyQuery = $this->createStructuredDataQuery($must_array, $should_array, $filters_array, $modifier_types, $page);

    // Page Calculations
    $from = $page * $this->getResultsPerPage();
    $size = $this->getResultsPerPage();
    $bodyQuery['from'] = $from;
    $bodyQuery['size'] = $size;

    $this->debugOutput(json_encode($bodyQuery), 'Structured Data Query');

    $params = $this->getParams($bodyQuery);

    try {
      $response = $this->client->search($params);
      $this->response = $response->getRawResponse();
      return $this->getNumberOfHits();
    }
    catch (Exception $e) {
      drupal_set_message($e->getMessage(), 'error');
    }
  }

  protected function createStructuredDataQuery($must, $should = array(), $filters = array(), $modifier_types = array()) {
    $must_queries = [];
    $should_queries = [];
    $must_query = [];

    $keyword_condition = 'must';
    // Search explicitly on name fields - parse field values first
    foreach ($must as $key => $must_data_match) {
      if ($key === 'keywords') {
        if (count($must) > 1) {
          $keyword_condition = 'should';
        }
        $should[$key] = $must_data_match;
      }
      else {
        $type = 'metaphone';
        if (isset($modifier_types[$key])) {
          $type = $modifier_types[$key];
        }
        $must_queries[] = array(
          'match' => array(
            'data.' . $key . '.' . $type => $must_data_match,
          ),
        );
      }
    }

    // Search globally on keywords within data


//    // Do a must on all must fields
//    foreach($must as $must_match) {
//      $must_queries[] = array(
//        'match' => array(
//          'page_contents' => $must_match,
//        ),
//      );
//      $constant_score[] = array('match' => $must_match, 'boost' => 4);
//    }

    // Do a should on the combination of all must fields
//    $must_combined = implode(' ', $must);
//    $should_queries[] = array(
//      'match_phrase' => array(
//        'page_contents' => array(
//          'query' => $must_combined,
//          'slop' => 0,
//          'boost' => 15,
//        ),
//      ),
//    );

    if (count($must_queries)) {
      $must_query = array(
        'nested' => array(
          'path' => 'data',
          'query' => array(
            'bool' => array(
              'must' => $must_queries,
            ),
          ),
        ),
      );
    }

    if (count($should)) {
      // Do a should on all should fields
      foreach($should as $key => $should_match) {
        if ($key === 'keywords') {
          $should_queries[] = array(
            'nested' => array(
              'path' => 'data',
              'query' => array(
                'bool' => array(
                  $keyword_condition => array(
                    'multi_match' => array(
                      'query' => $should_match,
                      'fields' => 'data.*.standard',
                    ),
                  ),
                ),
              ),
            ),
          );
          $should_queries[] = array(
            'nested' => array(
              'path' => 'term_field_location',
              'query' => array(
                'bool' => array(
                  'should' => array(
                    'match' => array(
                      'term_field_location.name' => $should_match,
                    ),
                  ),
                ),
              ),
            ),
          );
        }
      }

//      // Do a should on all fields combined
//      $all_combined = $must_combined . ' ' . implode(' ', $should);
//      $should_queries[] = array(
//        'match_phrase' => array(
//          'page_contents' => array(
//            'query' => $all_combined,
//            'slop' => 100,
//            'boost' => 5,
//          ),
//        ),
//      );
    }

    // Constant Score Queries, these ignore TF/IDF
//    if (count($constant_score)) {
//      $constant_score_queries = [];
//      foreach($constant_score as $data) {
//        $constant_score_queries[] = array(
//          'constant_score' => array(
//            'filter' => array(
//              'match' => array(
//                'page_contents' => $data['match'],
//              ),
//            ),
//            'boost' => $data['boost'],
//          ),
//        );
//      }
//      $should_queries[] = array(
//        'bool' => array(
//          'must' => array(
//            $constant_score_queries,
//          ),
//        ),
//      );
//    }



    $sort_query = FALSE;
    // Sort by last name query addition. @todo enable with a flag on filter.
//    $sort_query = [
//      [
//        'data.last_name.raw' =>
//          ['order' => 'asc',
//           'nested' =>
//             ['path' => 'data'],
//          ]
//      ]
//    ];


    // Disabled _source filtering since it is re-arranging the order of the data.

    $composite_must_query = array(
      'bool' => array(
        'must' => $must_query,
        'should' => $should_queries,
      ),
    );

    // Source is commented out since this affects the order of the nested data
    // in the 'data' field.
    $bodyQuery = array(
//      '_source' => array(
//        'includes' => array(
//          'sid',
//          'collection_id',
//          'fid',
//          'page_number',
//          'data',
//        )
//      ),
      'query' => array(
        'bool' => array(
          'must' => $composite_must_query,
          'filter' => $filters,
        ),
      ),
    );


    if ($sort_query) {
      $bodyQuery['sort'] = $sort_query;
    }

    $aggregation_function = 'getSimpleAggregation';
    if (count($filters)) {
      $aggregation_function = 'getFilteredAggregation';
    }

    $aggregations = $this->getAggregations();

    foreach ($aggregations as $key => $aggregation) {
      $bodyQuery['aggs'][$key] = $this->{$aggregation_function}($aggregation, $key, $filters);
    }

    return $bodyQuery;
  }

  /**
   * Collection aggregation data and parse it for facet filters.
   *
   * @category Facet Filters.
   */
  protected function processAggregations() {
    $facets = [];
    if (isset($this->response['aggregations'])) {
      foreach ($this->response['aggregations'] as $facet_name => $facet) {
        list($facet_key_name) = explode('|', $facet_name);
        $facet_key_name .= '_filtered';
        $bucket_data = [];
        if (isset($facet['buckets'])) {
          $bucket_data = $facet['buckets'];
        }
        elseif (isset($facet[$facet_key_name])) {
          $bucket_data = $facet[$facet_key_name]['buckets'];
        }
        foreach ($bucket_data as $bucket) {
          $facets[$facet_name][$bucket['key']]['count'] = $bucket['doc_count'];
        }
      }
    }
    $this->facets = $facets;
  }

  /**
   * Processes result data into a form useful for the front end.
   *
   * @param array $item
   *   Document item.
   * @param string $key
   *   Array index.
   */
  protected function processResults(array $item, $key) {
    $collection_data_processed = &drupal_static(__CLASS__ . '::' . __FUNCTION__);
    $entry['collection_title'] = '';
    if (isset($item['_source']['collection_id'])) {
      $collection_id = $item['_source']['collection_id'];
      if (!isset($collection_data_processed[$collection_id])) {
        if ($collection_data = $this->collection_index->getCollectionData($collection_id)) {
          $collection_data_processed[$collection_id] = $this->collection_index->extractSearchResultValues($collection_data);
        }
      }

      $collection_entry = $collection_data_processed[$collection_id];
      $entry['collection_title'] = $collection_entry['title'];
      $entry['collection_nid'] = $collection_id;
    }

    $entry['score'] = $item['_score'];
    $entry['id'] = $item['_id'];

    $data_fields_extract = ['last_name', 'first_name', 'year'];

    $data = $item['_source']['data'];
    foreach($data_fields_extract as $field_name) {
      $entry[$field_name] = isset($data[$field_name]) ? $data[$field_name] : '';
      unset($data[$field_name]);
    }

    // If we have an article title pick it up now.
    if (isset($data['article_title'])) {
      $title = $data['article_title'];
      $label = 'Article Title';
    }
    $data = StructuredData::recreateHumanHeaders($data);
    $data = array_filter($data);
    // If we didn't have an article title, pick up whatever is the first item.
    if (!isset($title)) {
      reset($data);
      $label = key($data);
      $title = $data[$label];
    }
    array_walk($data, array('self', 'simpleDataFormat'));
    $entry['structured_data'] = theme('item_list', array('items' => $data, 'attributes' => ['data-title' => $title, 'data-title-label' => $label]));

    $entry['fid'] = NULL;
    $entry['page'] = NULL;
    if (is_numeric($item['_source']['fid']) && $item['_source']['fid'] > 0) {
      $entry['fid'] = $item['_source']['fid'];
      $entry['page'] = $item['_source']['page_number'];
    }

    $this->results[] = $entry;
  }

  protected function simpleDataFormat(&$column, $key) {
    $column = '<strong>' . $key . '</strong>' . ': ' . $column;
  }

  protected function renderResultsTitle() {
    $this->title = 'Indexed Results';
    return parent::renderResultsTitle();
  }

  public function getResultSet($fields, $params, $pagination_key = 0) {
    $fieldsPre = [
      'last_name' => 'Last Name',
      'first_name' => 'First Name',
      'collection_title' => 'Collection',
      'structured_data' => 'Additional Details',
    ];

    if (ElasticManager::isDebug()) {
      $fieldsPre = ['score' => 'Score'] + $fieldsPre;
    }

    $fieldsPost = [
      'link' => 'Image',
    ];

    if (ElasticManager::isDebug()) {
      $fieldsPost = ['page' => 'Image #'] + $fieldsPost;
    }

    $display_fields = array_merge($fieldsPre, $fieldsPost);
    $header = array_values($display_fields);
    $attributes = ['class' => 'structured-data-results'];
    $rows = [];

    foreach ($this->getResults() as $result) {
      $result['link'] = '';
      if (isset($result['fid'])) {
        $path = drupal_get_path_alias('file/' . $result['fid']);
        if (is_numeric($result['page'])) {
          $path .= '/' . $result['page'];
        }
        $result['link'] = l('View', $path, array('attributes' => array('class' => array('search-results__view'))));
      }
      else {
        module_load_include('inc', 'pathauto', 'pathauto');
        $node = node_load($result['collection_nid']);
        // Replace any tokens in the pattern. Uses callback option to clean replacements. No sanitization.
        $structured_data = new stdClass();
        $structured_data->sid = $result['id'];
        $alias = token_replace('online-records/[node:title]/[structured_data:sid]', array('node' => $node, 'structured_data' => $structured_data), array(
          'sanitize' => FALSE,
          'clear' => TRUE,
          'callback' => 'pathauto_clean_token_values',
          'language' => (object) array('language' => LANGUAGE_NONE),
          'pathauto' => TRUE,
        ));
        $path = pathauto_clean_alias($alias);

        $result['link'] = l('View Data', $path, array('attributes' => array('class' => array('search-results__view'))));
      }


      $entry = [];
      foreach(array_keys($display_fields) as $field) {
        if (!isset($result[$field])) {
          $entry[$field] = '';
        }
        if (is_array($result[$field])) {
          $entry[$field] = implode(': ', $result[$field]);
        }
        else {
          $entry[$field] = $result[$field];
        }
      }
      $rows[] = $entry;
    }

    $variables = array('header' => $header, 'rows' => $rows, 'attributes' => $attributes, 'sticky' => FALSE, 'caption' => NULL, 'colgroups' => array(), 'empty' => 'No Results Found.');

    $output = $this->renderResultsTitle();
    $output .= theme('table', $variables);
    pager_default_initialize($this->getNumberOfHitsForPager(), $this->getResultsPerPage(), $pagination_key);
    $output .= theme('pager', array('quantity' => $this->getNumberOfHitsForPager(), 'parameters' => $params, 'element' => $pagination_key));
    return $output;
  }

}

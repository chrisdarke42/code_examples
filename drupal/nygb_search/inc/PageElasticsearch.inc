<?php

class PageElasticsearch extends BaseElasticsearch {


  protected $file_index;

  protected $results;

  protected $facets;

  /**
   * PageElasticsearch constructor.
   *
   * @throws \Exception
   */
  public function __construct() {
    if (variable_get('nygb_search_pdf_page_disabled', FALSE)) {
      throw new Exception(t('Page Search disabled.'));
    }
    $this->indexName = 'collection_files_pages';
    parent::__construct();
  }

  public function initPrecursor($collection_id = NULL) {
    if (isset($this->file_index)) {
      return $this->file_index;
    }
    $precursor = new FileElasticsearch($collection_id);
    $this->file_index = $precursor;
    return $precursor;
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
      'keywords' => 'Keywords / Location',
    ];
    return array_merge($text_fields, $default_fields);
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
  public function search($terms, $filters = array()) {

    $page = $this->page_number;

    $_must_keys = ['first_name' => 'First Name', 'last_name' => 'Last Name'];
    $must_array = [];
    $should_array = [];
    $filters_array = [];

    if (!is_array($terms)) {
      $should_array[] = $terms;
    }
    else {
      foreach($terms as $key => $term) {
        if (array_key_exists($key, $_must_keys)) {
          $must_array[$key] = $term;
        }
        else {
          $should_array[$key] = $term;
        }
      }
    }

    if (count($filters)) {
      $filters_array = $this->filterParams($filters);
    }
    $this->initPrecursor();

//    $fids = [];
//    // Check if we have any fids from Filters.
//    if (count($filters) || $precursor->get_collection_id()) {
//      // Run a precursor search to identify FIDs.
//      $response = $precursor->searchFiltered(array(), $filters);
//      if ($response > 0) {
//        $fids = $precursor->parsePrecursorQueryFids();
//      }
//      else {
//        // If filters didn't match anything, stop here.
//        return 0;
//      }
//    }

    // If we have fids, add a filter for that.
//    if (count($fids)) {
//      $filters_array = array('terms' => ['fid' => $fids]);
//    }

    // Create main query.
    $bodyQuery = $this->createPageDataQuery($must_array, $should_array, $filters_array);

    // Page Calculations
    $from = $page * $this->getResultsPerPage();
    $size = $this->getResultsPerPage();
    $bodyQuery['from'] = $from;
    $bodyQuery['size'] = $size;

    $this->debugOutput(json_encode($bodyQuery), 'Pages Query');

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

  protected function createPageDataQuery($must, $should = array(), $filters = array()) {

    $must_queries = [];
    $should_queries = [];

    $constant_score = [];

    // Do a must on all must fields
    foreach($must as $key => $must_match) {
      $must_queries[] = array(
        'match' => array(
          'page_contents' => $must_match,
        ),
      );
      $constant_score[] = array('match' => $must_match, 'boost' => 4);
    }

    // Do a should on the combination of all must fields
    $must_combined = implode(' ', $must);
    $should_queries[] = array(
      'match_phrase' => array(
        'page_contents' => array(
          'query' => $must_combined,
          'slop' => 0,
          'boost' => 15,
        ),
      ),
    );

    if (count($should)) {
      // Do a should on all should fields
      foreach($should as $key => $should_match) {
        if ($key === 'keywords') {
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
        $constant_score[] = array('match' => $should_match, 'boost' => 2);
        $should_queries[] = array(
          'match' => array(
            'page_contents' => array(
              'query' => $should_match,
              'boost' => 3,
            ),
          ),
        );
      }

      // Do a should on all fields combined
      $all_combined = $must_combined . ' ' . implode(' ', $should);
      $should_queries[] = array(
        'match_phrase' => array(
          'page_contents' => array(
            'query' => $all_combined,
            'slop' => 100,
            'boost' => 5,
          ),
        ),
      );
    }

    // Constant Score Queries, these ignore TF/IDF
    if (count($constant_score)) {
      $constant_score_queries = [];
      foreach($constant_score as $data) {
        $constant_score_queries[] = array(
          'constant_score' => array(
            'filter' => array(
              'match' => array(
                'page_contents' => $data['match'],
              ),
            ),
            'boost' => $data['boost'],
          ),
        );
      }
      $should_queries[] = array(
        'bool' => array(
          'must' => array(
            $constant_score_queries,
          ),
        ),
      );
    }

    $filter_queries = [];
    if (count($filters)) {
      foreach($filters as $filter_name => $filter) {
        $first = reset($filter);
        if (is_array($first)) {
          $filter_queries[] = $this->getNestedFilter($filter_name, $filter);
        }
        else {
          $filter_queries[] = $this->getPlainFilter($filter_name, $filter);
        }
      }
    }

    $composite_must_query = array(
      'bool' => array(
        'must' => $must_queries,
        'should' => $should_queries,
      ),
    );

    $bodyQuery = array(
      '_source' => array(
        'includes' => array(
          'pid',
          'fid',
          'page_number',
        )
      ),
      'query' => array(
        'bool' => array(
          'must' => $composite_must_query,
        ),
      ),
      'highlight' => array(
        'type' => 'fvh',
        'phrase_limit' => 5,
        'order' => 'score',
        'fields' => array(
          'page_contents' => array(
            'pre_tags' => '<em>',
            'post_tags' => '</em>',
            'fragment_size' => 60, //<< Change fragment size based on number of match fields?
          ),
        ),
      ),
    );

    $filter_queries_aggregation = [];
    if (count($filter_queries)) {
      $bodyQuery['query']['bool']['filter'] = $filter_queries;
      $filter_queries_aggregation['bool']['filter'] = $filter_queries;
    }


    $aggregation_function = 'getSimpleAggregation';
    if (count($filter_queries_aggregation)) {
      $aggregation_function = 'getFilteredAggregation';
    }

    $aggregations = $this->getAggregations();

    foreach ($aggregations as $key => $aggregation) {
      $bodyQuery['aggs'][$key] = $this->{$aggregation_function}($aggregation, $key, $filter_queries_aggregation);
    }

    return $bodyQuery;
  }

  protected function  getPlainFilter($filter_name, $filter) {
    $filter_subarray = [];
    foreach($filter as $filter_id) {
      $filter_subarray['term'][$filter_name] = $filter_id;
    }
    return $filter_subarray;
  }

  protected function getNestedFilter($filter_name, $filter) {

    $should_filter_subarray = [];
    foreach($filter as $filter_key => $filter_ids) {
      $filter_full = $filter_name . '.' . $filter_key;
      foreach($filter_ids as $filter_id) {
        $should_filter_subarray[] = [
          ['term' => [
            $filter_full => $filter_id
          ]]
        ];
      }

    }

    return [
      'nested' => [
        'path' => $filter_name,
        'query' => [
          'bool' => [
            'should' => $should_filter_subarray,
          ]
        ]
      ]
    ];

  }

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

    $file_data_processed = &drupal_static(__CLASS__ . '::' . __FUNCTION__);
    // If for some reason there is no fid, just leave.
    if (!isset($item['_source']['fid']) || !is_numeric($item['_source']['fid']) || empty($item['highlight']['page_contents'])) {
      if (empty($item['highlight']['page_contents'])) {
        watchdog('NYGB Search', 'No Page Contents in search results');
      }
      else {
        watchdog('NYGB Search', 'No FID data in search results');
      }

      return;
    }
    $fid = $item['_source']['fid'];

    if (!isset($file_data_processed[$fid])) {
      if ($file_data = $this->file_index->getFileData($fid)) {
        $file_data_processed[$fid] = $this->file_index->extractSearchResultValues($file_data);
      }
    }

    $entry = $file_data_processed[$fid];

    $entry['score'] = $item['_score'];
    $entry['id'] = $item['_id'];
    $entry['fid'] = $fid;
    $entry['page'] = $item['_source']['page_number'];
    $entry['match'] = $this->parseHighlights($item['highlight']);

    $this->results[] = $entry;
  }

  protected function parseHighlights($highlights) {
    $highlight = [];
    foreach($highlights['page_contents'] as $snippet) {
      $highlight[] = filter_xss(trim(preg_replace('/\s+/', ' ', $snippet)));
    }
    return truncate_utf8(implode(', ', $highlight), 100, TRUE, TRUE);
  }

  protected function renderResultsTitle() {
    $this->title = 'Full-text Search Results';
    return parent::renderResultsTitle();
  }

  public function getResultSet($fields, $params, $pagination_key = 1) {

    if (empty($fields)) {
      $fields = [];
    }

    // Removing from the index as its already defined.
    unset($fields['term_field_subtitle']);

    $fieldsPre = [
      'match' => 'Match',
      'field_collection:title' => 'Collection',
      'subtitle' => 'Subtitle',
      'term_field_location' => 'Location',
      'term_field_collection_category' => 'Category',
    ];

    ########## FIELD REMOVAL EXCEPTION ########
    // This array is a list of fields that need to be in the filters but NOT be
    // displayed in the listing. This is a bit of an anti-pattern so it will be
    // done in code.
    $exception_removal_fields = ['term_field_collection_category'];

    foreach($fieldsPre as $key => $title) {
      if (isset($fields[$key])) {
        unset($fields[$key]);
      }
    }

    $fieldsPost = array();
    if (ElasticManager::isDebug()) {
      $fieldsPre = ['score' => 'Score'] + $fieldsPre;
      $fieldsPost = [
        'name' => 'File',
        'page' => 'Image #',
        'link' => 'Image',
      ];
    }

    $fieldsPost['link'] = 'Image';

    $display_fields = array_merge($fieldsPre, $fields, $fieldsPost);

    // Field Removal Exception - As document above, some fields need to not show.
    if (count($exception_removal_fields)) {
      foreach($exception_removal_fields as $field_name) {
        unset($display_fields[$field_name]);
      }
    }

    $header = array_values($display_fields);
    $attributes = [];
    $rows = [];

    foreach ($this->getResults() as $result) {
      $path = drupal_get_path_alias('file/' . $result['fid']);
      if (is_numeric($result['page'])) {
        $path .= '/' . $result['page'];
      }
      $result['link'] = l('View', $path, array('attributes' => array('class' => array('search-results__view'))));

      $entry = [];
      foreach(array_keys($display_fields) as $field) {
        if (!isset($result[$field])) {
          $entry[$field] = '';
        }
        else if (is_array($result[$field])) {
          $entry[$field] = implode(': ', $result[$field]);
        }
        else {
          $entry[$field] = $result[$field];
        }
      }

      if (!empty($entry)) {
        $rows[] = $entry;
      }
    }

    $variables = array('header' => $header, 'rows' => $rows, 'attributes' => $attributes, 'sticky' => FALSE, 'caption' => NULL, 'colgroups' => array(), 'empty' => 'No Results Found.');
    $output = $this->renderResultsTitle();
    $output .= theme('table', $variables);
    pager_default_initialize($this->getNumberOfHitsForPager(), $this->getResultsPerPage(), $pagination_key);
    $output .= theme('pager', array('quantity' => $this->getNumberOfHitsForPager(), 'parameters' => $params, 'element' => $pagination_key));

    return $output;
  }


}

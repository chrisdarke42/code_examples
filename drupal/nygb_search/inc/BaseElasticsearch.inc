<?php

/**
 * @file
 * BaseElasticsearch class file.
 *
 * This file contains the class that all the Elasticsearch functionality is
 * based off.
 */

/**
 * Class BaseElasticsearch
 */
abstract class BaseElasticsearch {

  /**
   * Index-specific variables.
   */

  /**
   * @var object
   *   ElasticSearch Client.
   */
  protected $client;
  /**
   * @var string
   *   Name of index.
   */
  protected $indexName;

  /**
   * Search specific data.
   */

  /**
   * @var array
   *   Search response data.
   */
  protected $response = array();

  /**
   * @var bool|int
   *   Number of Hits.
   */
  protected $numHits = FALSE;

  /**
   * @var array
   *   Array of standard filters for ElasticSearch.
   */
  protected $filters = array(
    'english_stop' => array(
      'type' => 'stop',
      'stopwords' => '_english_',
    ),
  );

  /**
   * @var array
   *   Array of standard analyzers for ElasticSearch.
   */
  protected $analyzers = array(
    'plain_text' => array(
      'tokenizer' => 'whitespace',
      'filter' => array(
        'lowercase',
        'stop',
        'english_stop',
      ),
    ),
  );

  protected $page_number;

  public function __construct() {
    try {
      $this->setClient();
      $indices = $this->getIndices();
      if (isset($this->indexName) && !isset($indices[$this->indexName])) {
        throw new Exception(t('No Index defined in Elastic for %index_name', array('%index_name' => $this->indexName)));
      }
    }
    catch (Exception $e) {
      throw new Exception(t('ElasticSearch is not available at this time.'));
    }
  }

  /**
   * Setting the Connection Client in the object data.
   */
  protected function setClient() {
    $cluster_id = elasticsearch_connector_get_default_connector();
    $this->client = elasticsearch_connector_get_client_by_id($cluster_id);
    if ($this->client === NULL) {
      throw new Exception(t('Cannot run ElasticSearch Operations.'));
    }
  }

  /**
   * Return indices for cluster.
   *
   * @return array
   *   List of indices.
   */
  protected function getIndices() {
    $cluster_id = elasticsearch_connector_get_default_connector();
    return elasticsearch_connector_get_indices_options($cluster_id);
  }

  /**
   * Initiate params array.
   *
   * @return array
   *   Initial params array with index and type.
   */
  protected function getIndexParams() {
    $params = array(
      'index' => $this->indexName,
      'type' => '_doc',
    );
    return $params;
  }

  /**
   * Format parameters for ElasticSearch query.
   *
   * @param array $query
   *   Query array.
   *
   * @return array
   *   Query wrapped with Parameters.
   */
  protected function getParams(array $query) {
    $params = $this->getIndexParams();
    $params['body'] = $query;
    return $params;
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
  public function search($term, $page = 0) {
    $from = $page * $this->getResultsPerPage();
    $size = $this->getResultsPerPage();

    $bodyQuery = array(
      'query' => array(
        'query_string' => array(
          'query' => $term,
        ),
      ),
      'from' => $from,
      'size' => $size,
    );
    $params = $this->getParams($bodyQuery);
    try {
      $response = $this->client->search($params);
      $this->response = $response->getRawResponse();
      return $this->getNumberOfHits();
    }
    catch (Exception $e) {
      drupal_set_message($e->getMessage(), 'error');
    }
    return 0;
  }

  /**
   * Get lists of aggregations for all searches.
   *
   * @return array
   */
  protected function getAggregations() {
    return [
      'location|taxonomy_term|term_field_location' => 'collection_node:field_location',
      'collection|node|field_collection:nid' => 'collection_node:nid',
      'collection_category|taxonomy_term|term_field_collection_category' => 'collection_node:field_collection_category',
    ];
  }

  /**
   * Format a simple non filtered aggregation.
   *
   * @param $param
   *
   * @return array
   */
  protected function getSimpleAggregation($param) {
    return [
      'terms' => [
        'field' => $param,
      ],
    ];
  }

  /**
   * Format a filtered aggregation.
   *
   * @param $param
   * @param $key
   * @param $filters
   *
   * @return array
   */
  protected function getFilteredAggregation($param, $key, $filters) {
    list($key_name) = explode('|', $key);
    return [
      'filter' => $filters,
      'aggregations' => [
        $key_name . '_filtered' => [
          'terms' => [
            'field' => $param,
          ],
        ],
      ]
    ];
  }

  protected function filterParamNestedId($filter, &$nested = TRUE) {
    switch($filter) {
      case 'term_field_collection_category':
        $column = 'tid';
        break;
      case 'collection_node:field_collection_category':
        $column = 'tid';
        $nested = FALSE;
        break;
      case 'collection_node:nid':
      case 'field_collection:nid':
        $column = 'nid';
        $nested = FALSE;
        break;
      default:
        $column = 'tid';
    }
    return $column;
  }

  protected function filterParams($filters) {
    $filter_array = [];
    foreach ($filters as $filter => $filter_data) {
      switch($filter) {
        case 'term_field_collection_category':
          $mapping = 'collection_node:field_collection_category';
          break;
        case 'field_collection:nid':
          $mapping = 'collection_node:nid';
          break;
        default:
          $mapping = $filter;
      }
      $nested = TRUE;
      $column = $this->filterParamNestedId($mapping, $nested);
      if ($mapping) {
        $data = array_column($filter_data, $column);
        if ($nested) {
          $filter_array[$mapping][$column] = $data;
        }
        else {
          $filter_array[$mapping] = $data;
        }
      }
    }
    return $filter_array;
  }


  /**
   * Return the total number of hits for the search.
   *
   * @return bool|int
   *   Total number of hits.
   */
  public function getNumberOfHits() {
    if ($this->numHits === FALSE) {
      if (isset($this->response) && !empty($this->response)) {
        $response = $this->response;
        if (isset($response['hits']) && is_numeric($response['hits']['total'])) {
          $this->numHits = $response['hits']['total'];
        }
      }
      // Num Hits was not updated, so must be empty.
      if ($this->numHits === FALSE) {
        $this->numHits = 0;
      }
    }
    $this->debugOutput($this->numHits, $this->indexName . ' hits');
    return $this->numHits;
  }

  /**
   * Getter for {$resultsPerPage}.
   *
   * @return int
   *   Value of {$resultsPerPage}.
   */
  public function getResultsPerPage() {
    if (isset($this->results_per_page)) {
      return $this->results_per_page;
    }
//    if ($this->global) {
//      $results = variable_get('nygb_search_results_per_page_global', 10);
//    }
//    else {
//      $results = variable_get('nygb_search_results_per_page_collection', 40);
//    }
    $results = variable_get('nygb_search_results_per_page_global', 20);
    return $results;
  }

  public function setResultsPerPage($rescount) {
    $this->results_per_page = $rescount;
  }

  protected function debugOutput($output, $label) {
    if (ElasticManager::isDebug() && module_exists('devel')) {
      dpm($output, $label);
    }
  }

  /**
   * Getter for {$resultsPerPage}.
   *
   * @return int
   *   Value of {$resultsPerPage}.
   */
  public function getMaxPages() {
    return variable_get('nygb_search_max_pages', 15);
  }

  /**
   * Return the total number of hits used for the Pager utility.
   *
   * @return bool|float|int
   *   Number of hits.
   */
  public function getNumberOfHitsForPager() {
    $max_num_hits = $this->getResultsPerPage() * $this->getMaxPages();
    return ($this->getNumberOfHits() < $max_num_hits) ? $this->getNumberOfHits() : $max_num_hits;
  }

  /**
   * Get Results of search.
   *
   * @return array
   *   Return array of search results.
   */
  public function getResults() {
    // Check if we have any results...
    if ($this->numHits > 0) {
      // Parse results.
      if (method_exists($this, 'processAggregations')) {
        $this->processAggregations();
      }
      $hits = $this->response['hits']['hits'];
      if (empty($this->results)) {
        array_walk($hits, array($this, 'processResults'));
      }
    }
    return $this->results;
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
    $entry = array(
      '_score' => $item['_score'],
      'id' => $item['_id'],
    );

    foreach ($item['_source'] as $key => $value) {
      $entry[$key] = $value;
    }
    $this->results[] = $entry;
  }

  /**
   * Returns number of pages of results.
   *
   * @return float|int
   *   Number of pages.
   */
  public function getNumPages() {
    if ($this->numHits > 0 && $this->getResultsPerPage() > 0) {
      return ceil($this->numHits / $this->getResultsPerPage());
    }
    return 0;
  }

  /**
   * Set the current page number.
   *
   * @param int $page_number
   *   Page number.
   */
  public function setPageNumber($page_number) {
    $this->page_number = $page_number;
  }

  /**
   * Get facet data.
   *
   * @return array
   *   Facet data.
   */
  public function getFacets() {
    return isset($this->facets) ? $this->facets : [];
  }

  protected function renderResultsTitle() {
    if (isset($this->title)) {
      $results_per_page = $this->getResultsPerPage();
      return theme('nygb_search_result_title', array('title' => $this->title, 'results_number' => $results_per_page, 'index_name' => $this->indexName));
    }

  }
}

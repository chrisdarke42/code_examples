<?php

/**
 * @file
 * Contains the SearchLog ElasticSearch Connector implementation class.
 */

/**
 * The base class for Google event tracking.
 */
class SearchLogElasticsearchConnector extends BaseElasticsearchConnector {

  protected $fields = array(
    'search_term' => array(
      'type' => 'keyword',
    ),
    'timestamp' => array(
      'type' => 'date',
      'format' => 'epoch_second',
    ),
    'page' => array(
      'type' => 'byte',
    ),
    'locations' => array(
      'type' => 'nested',
    ),
    'activities' => array(
      'type' => 'nested',
    ),
    'tours' => array(
      'type' => 'nested',
    ),
    'rating' => array(
      'type' => 'byte',
      'null_value' => 0,
    ),
  );

  /**
   * ToursElasticsearchConnector constructor.
   *
   * @throws \Exception
   */
  public function __construct() {
    $this->indexName = 'zsearch_log';
    $this->type = 'log';
    parent::__construct();
  }

  /**
   * Index a single document.
   *
   * @param object $document
   *   A document object.
   * @param int $id
   *   The id of the document.
   *
   * @return object
   *   Response from Elastic.
   */
  protected function indexDocument($document, $id = NULL) {
    $params = $this->getIndexParams();

    if (!empty($id)) {
      // Updating entry.
      $params['id'] = $id;
      $params['body']['doc'] = $document;
      $response = $this->client->update($params);
    }
    else {
      // Creating entry.
      $params['body'] = $document;
      $response = $this->client->index($params);
    }
    watchdog('ZSearchESIndexing', print_r($response, 1));
    return $response;
  }

  /**
   * Log a given search.
   *
   * @param string $term
   *   Search term to be logged.
   * @param array $data
   *   Data to store.
   * @param string $type
   *   Mapping for JSON data.
   * @param int $id
   *   ID of data.
   * @param int $page
   *   Page number.
   *
   * @return object
   *   Indexing result.
   */
  public function logSearch($term, array $data, $type, $id, $page) {
    $document['search_term'] = $term;
    $document['page'] = $page;
    $document[$type] = $data;
    $document['timestamp'] = time();
    return $this->indexDocument($document, $id);
  }

  /**
   * Log the rating of this search.
   *
   * @param int $rating
   *   Rating given to search.
   * @param string $id
   *   ID of search.
   *
   * @return bool
   *   True if rating was logged.
   */
  public function rateSearch($rating, $id) {
    if (isset($id) && strlen($rating)) {
      $rating = ($rating === 'up') ? 1 : -1;
      $document['rating'] = $rating;
      $response = $this->indexDocument($document, $id);
      if ($response['result'] === 'updated') {
        return TRUE;
      }
    }
    return FALSE;
  }

  /**
   * Return a count of the records that should be indexed.
   *
   * @return int
   *   Record count.
   */
  public function getCountIndexable() {
    return 0;
  }

}

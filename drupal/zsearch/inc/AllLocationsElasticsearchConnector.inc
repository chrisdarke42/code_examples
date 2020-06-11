<?php

/**
 * @file
 * Contains the Locations ElasticSearch Connector implementation class.
 */

/**
 * The Location content connector.
 */
class AllLocationsElasticsearchConnector extends NodeElasticsearchConnector {

  protected $fields = array(
    'location' => array(
      'type' => 'text',
      'analyzer' => 'plain_text_loc_syn',
    ),
  );

  protected $analyzers = array(
    'plain_text_loc_syn' => array(
      'tokenizer' => 'standard',
      'filter' => array(
        'english_possessive_stemmer',
        'lowercase',
        'english_stop',
        'location_synonyms',
      ),
    ),
    'plain_text_whitespace' => array(
      'tokenizer' => 'whitespace',
      'filter' => array(
        'english_possessive_stemmer',
        'lowercase',
        'english_stop',
        'english_stemmer',
      ),
    ),
    'just_lowercase' => array(
      'tokenizer' => 'keyword',
      'filter' => array(
        'lowercase',
      ),
    ),
  );

  protected $query = array();
  protected $max_score = 0;

  /**
   * LocationsElasticsearchConnector constructor.
   *
   * @throws \Exception
   */
  public function __construct() {
    $this->indexName = 'zsearch_alllocations';
    $this->type = 'alllocations';
    parent::__construct();
  }

  /**
   * Return query array.
   *
   * @return array
   *   Query array.
   */
  public function getQuery() {
    return $this->query;
  }

  /**
   * Parse nested values into nested value set for indexing.
   *
   * @param array $values
   *   Items.
   *
   * @return array
   *   Structure for indexing.
   */
  protected function nestedValues(array $values = array()) {
    $location_array = array();
    foreach ($values as $value) {
      $location_array[] = array('value' => $value);
    }
    return $location_array;
  }

  /**
   * Parse wrapper to return an array for indexing.
   *
   * @param object $location_wrapper
   *   Entity Metadata Wrapper of node.
   *
   * @return array
   *   Array of data for indexing.
   */
  protected function parseWrapperIndexing($location_wrapper) {
    return [
      'location' => $location_wrapper->title->value(),
    ];
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
    $location = $item['_source']['location'];
    $location_id = $item['_id'];
    $this->results['matches'][] = array(
      'location' => $location,
      'score' => $item['_score'],
      'nid' => $location_id,
    );
  }

  /**
   * Format data for logging.
   *
   * @return array
   *   Data to log.
   */
  protected function getLogFormatted() {
    $results = array();
    $results['max_score'] = $this->response['hits']['max_score'];
    foreach ($this->response['hits']['hits'] as $hit) {
      $results['hits'][] = array(
        'id' => $hit['_id'],
        'score' => $hit['_score'],
        'title' => $hit['_source']['location'],
      );
    }
    return $results;
  }

  /**
   * Batch API process for indexing.
   *
   * @param object $context
   *   Batch API context.
   *
   * @return mixed|null|string
   *   Batch API return message.
   */
  public function indexData(&$context) {
    if (!isset($context['sandbox']['progress'])) {
      $context['sandbox']['progress'] = 0;
      $context['sandbox']['current_node'] = -1;
      $context['sandbox']['iterator'] = 0;
      $context['sandbox']['max'] = $this->getNodeQueryCount();
    }

    $limit = 100;

    // Retrieve the next group of nids.
    $query = $this->getNodeQuery();
    $query->orderBy('n.nid', 'ASC')
      ->where('n.nid > :nid', array(
        ':nid' => $context['sandbox']['current_node'],
      ))
      ->range(0, $limit);
    $result = $query->execute();
    $nid_array = $result->fetchCol();
    try {
      $indexed_items = $this->indexDocuments($nid_array);
      $context['sandbox']['progress'] = $context['sandbox']['progress'] + count($indexed_items);
      $last_item = end($indexed_items);
      $context['sandbox']['current_node'] = $last_item['index']['_id'];
    }
    catch (Exception $e) {
      drupal_set_message($e->getMessage(), 'error');
    }
    $context['sandbox']['iterator']++;
    $context['results'] = array_merge($context['results'], $nid_array);
    $context['finished'] = $context['sandbox']['progress'] >= $context['sandbox']['max'] ? TRUE : $context['sandbox']['progress'] / $context['sandbox']['max'];
    if ($context['sandbox']['iterator'] > ($context['sandbox']['max'] / $limit) * 2) {
      $context['finished'] = TRUE;
      if ($context['hook_install'] === TRUE) {
        $final_message = implode('', $context['sandbox']['messages']);
        return t('Locations indexed into ElasticSearch : @message', array(
          '@message' => $final_message,
        ));
      }
    }
  }

  /**
   * Generate SelectQuery to get locations.
   *
   * @return \SelectQuery
   *   Drupal SelectQuery object.
   */
  protected function getNodeQuery() {

    $query = db_select('node', 'n');
    $query->fields('n', array(
      'nid',
    ))
      ->condition('n.status', 0, '>')
      ->condition('n.type', 'location');

    return $query;
  }

  /**
   * ElasticSearch precursorMatchItem for Locations within main Search.
   *
   * @param array $location
   *   Location array data.
   * @param float $weightAdjust
   *   Boost value.
   *
   * @return array
   *   Precursor Match array format for query.
   */
  public static function precursorMatchItem(array $location, $weightAdjust) {
    return array('match' => array('locations.name' => array('query' => $location['location'], 'boost' => $weightAdjust)));
  }

  /**
   * ElasticSearch precursorMatch before Main Search.
   *
   * @param string $matches
   *   String to search on.
   *
   * @return array
   *   Search Array.
   */
  public static function precursorMatch($matches) {
    return array(
      'nested' => array(
        'path' => 'locations',
        'score_mode' => 'max',
        'query' => array('bool' => array('should' => $matches, 'boost' => 2)),
      ),
    );
  }

  /**
   * Get the data the precursor is keyed on.
   *
   * @param array $match
   *   Match data.
   *
   * @return mixed
   *   Value in key.
   */
  public static function getPrecursorKeyedData(array $match) {
    return $match['location'];
  }

}

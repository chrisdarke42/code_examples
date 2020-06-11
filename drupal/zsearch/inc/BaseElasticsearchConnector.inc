<?php

/**
 * @file
 * Contains the ZSearch ElasticSearch Connector abstract class.
 */

/**
 * The abstract base class for Zicasso ElasticSearch.
 */
abstract class BaseElasticsearchConnector {

  /**
   * Index-specific variables.
   */

  /**
   * @var object
   *   ElasticSearch Connector Client.
   */
  protected $client;
  /**
   * @var string
   *   Name of index.
   */
  protected $indexName;

  /**
   * @var string
   *   Name of document type.
   */
  protected $type;

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
   * @var int
   *   Maximum score for search.
   */
  protected $maxScore = 0;

  /**
   * @var array
   *   Array of result data from Search Response.
   */
  protected $results = array();

  /**
   * @var array
   *  Array of weights for fields.
   */
  protected $fieldWeights = array();

  /**
   * @var array
   *   Array of fields for index.
   */
  protected $fields = array();

  /**
   * @var null|string
   *   ID of log index item for search.
   */
  protected $logId = NULL;

  /**
   * @var array
   *   Array of stopwords to be filtered from search phrase.
   */
  protected $stopWords = ["i", "me", "my", "myself", "we", "our", "ours", "ourselves", "you", "your", "yours", "yourself", "yourselves", "he", "him", "his", "himself", "she", "her", "hers", "herself", "it", "its", "itself", "they", "them", "their", "theirs", "themselves", "what", "which", "who", "whom", "this", "that", "these", "those", "am", "is", "are", "was", "were", "be", "been", "being", "have", "has", "had", "having", "do", "does", "did", "doing", "a", "an", "the", "and", "but", "if", "or", "because", "as", "until", "while", "of", "at", "by", "for", "with", "about", "against", "between", "into", "through", "during", "before", "after", "above", "below", "to", "from", "up", "down", "in", "out", "on", "off", "over", "under", "again", "further", "then", "once", "here", "there", "when", "where", "why", "how", "all", "any", "both", "each", "few", "more", "most", "other", "some", "such", "no", "nor", "not", "only", "own", "same", "so", "than", "too", "very", "s", "t", "can", "will", "just", "don", "should", "now"];

  /**
   * @var array
   *   Array of standard filters for ElasticSearch.
   */
  protected $filters = array(
    'synonym' => array(
      'type' => 'synonym',
      'format' => 'wordnet',
      'synonyms_path' => 'synonyms.txt',
      'ignore_case' => TRUE,
    ),
    'location_synonyms' => array(
      'type' => 'synonym',
      'synonyms_path' => 'synonyms_locations.txt',
      'ignore_case' => TRUE,
    ),
    'zicasso_synonyms' => array(
      'type' => 'synonym',
      'synonyms_path' => 'synonyms_zicasso.txt',
      'ignore_case' => TRUE,
    ),
    'english_stop' => array(
      'type' => 'stop',
      'stopwords' => '_english_',
    ),
    'english_stemmer' => array(
      'type' => 'stemmer',
      'language' => 'english',
    ),
    'english_possessive_stemmer' => array(
      'type' => 'stemmer',
      'language' => 'possessive_english',
    ),
  );

  /**
   * @var array
   *   Array of standard analyzers for ElasticSearch.
   */
  protected $analyzers = array(
    'html_body' => array(
      'char_filter' => array('html_body_filter'),
      'tokenizer' => 'whitespace',
      'filter' => array(
        'lowercase',
        'english_stop',
        'synonym',
      ),
    ),
    'plain_text' => array(
      'tokenizer' => 'whitespace',
      'filter' => array(
        'lowercase',
        'stop',
        'synonym',
      ),
    ),
    'plain_text_no_syn' => array(
      'tokenizer' => 'whitespace',
      'filter' => array(
        'lowercase',
        'stop',
      ),
    ),
  );

  /**
   * BaseElasticsearchConnector constructor.
   *
   * @throws \Exception
   */
  public function __construct() {
    try {
      $this->setClient();
      $indices = $this->getIndices();
      if (isset($this->indexName) && !isset($indices[$this->indexName])) {
        // Build index if it doesn't exist.
        $response = $this->buildIndex();
        watchdog('ZSearchES', print_r($response, 1));
      }
      if (isset($this->type) && !$this->typeExists()) {
        $response = $this->buildMapping();
        watchdog('ZSearchES', print_r($response, 1));
      }
    }
    catch (Exception $e) {
      throw new Exception('ElasticSearch is not available at this time.');
    }
  }

  /**
   * Setting the Connection Client in the object data.
   */
  protected function setClient() {
    $cluster_id = elasticsearch_connector_get_default_connector();
    $this->client = elasticsearch_connector_get_client_by_id($cluster_id);
    if ($this->client === NULL) {
      throw new Exception('Cannot run ElasticSearch Operations.');
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
   * Getter for {$resultsPerPage}.
   *
   * @return int
   *   Value of {$resultsPerPage}.
   */
  public function getResultsPerPage() {
    return variable_get('zsearch_es_results_per_page', 10);
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
   * Get list of ElasticSearch Filters.
   *
   * @return array
   *   ElasticSearch Filters.
   */
  protected function getFilters() {
    return $this->filters;
  }

  /**
   * Get list of ElasticSearch Analyzers.
   *
   * @return array
   *   ElasticSearch Analyzers.
   */
  protected function getAnalyzers() {
    return $this->analyzers;
  }

  /**
   * Build Index.
   *
   * @return mixed
   *   Index creation response.
   */
  protected function buildIndex() {
    try {
      // Create missing index.
      $params = array(
        'index' => $this->indexName,
        'body' => array(
          'settings' => array(
            'number_of_shards' => 1,
            'number_of_replicas' => 1,
            'analysis' => array(
              'filter' => $this->getFilters(),
              'analyzer' => $this->getAnalyzers(),
              'char_filter' => array(
                'html_body_filter' => array(
                  'type' => 'html_strip',
                ),
              ),
            ),
          ),
        ),
      );
      return $this->client->indices()->create($params);
    }
    catch (Exception $e) {
      drupal_set_message($e->getMessage(), 'error building index');
    }
    return FALSE;
  }

  /**
   * Checks if a type exists in an index.
   *
   * @param string $type
   *   A type name.
   *
   * @return bool
   *   True if type exists.
   */
  protected function typeExists($type = NULL) {
    $params = array();
    $params['index'] = $this->indexName;
    $params['type'] = (isset($type)) ? $type : $this->type;
    return $this->client->indices()->existsType($params);
  }

  /**
   * Build Mapping for type in index.
   *
   * @return array
   *   Mapping creation response.
   */
  protected function buildMapping() {
    $mapping_params['index'] = $this->indexName;
    $mapping_params['type'] = $this->type;
    $mapping = array(
      'properties' => $this->fields,
    );
    $mapping_params['body'][$this->type] = $mapping;
    try {
      return $this->client->indices()->putMapping($mapping_params);
    }
    catch (Exception $e) {
      drupal_set_message($e->getMessage(), 'error building mapping');
    }
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
      'type' => $this->type,
    );
    return $params;
  }

  /**
   * Index a single document.
   *
   * @param object $document
   *   A document object.
   * @param int $id
   *   The id of the document.
   */
  abstract protected function indexDocument($document, $id = NULL);

  /**
   * Delete a specific document from the index.
   *
   * @param int $id
   *   Document node id.
   *
   * @return mixed
   *   Response from process.
   */
  protected function deleteDocument($id) {
    try {
      $params = array(
        'index' => $this->indexName,
        'type' => $this->type,
        'id' => $id,
      );
      $response = $this->client->delete($params);
      return $response;
    }
    catch (Exception $e) {
      // @todo rethrow the error to allow the end call to decide if its an error or not.
      // drupal_set_message($e->getMessage(), 'error');
    }
    return FALSE;
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
   * Get names of all fields.
   *
   * @return array
   *   Array of all the field names.
   */
  protected function getFieldNames() {
    return array_keys($this->fields);
  }

  /**
   * Get Text Search Field Names.
   *
   * To do a complete text search across all fields, these do need to be split
   * into groups for boolean nested queries.
   *
   * @return array
   *   Field names to search on.
   */
  protected function getTextSearchFieldNames() {
    $fields = array();
    foreach ($this->fields as $field_name => $field_definition) {
      switch ($field_definition['type']) {
        case 'text':
          $fields['multimatch'][] = $field_name;
          break;

        case 'integer':
          break;

        case 'nested':
          $properties = $field_definition['properties'];
          reset($properties);
          $fields['nested'][$field_name] = $field_name . '.' . key($properties);
          break;
      }
    }
    return $fields;
  }

  /**
   * Array Map function to add boost data.
   *
   * @param string $fieldname
   *   ES Field name.
   *
   * @return string
   *   ES field name with boost.
   */
  protected function boostField($fieldname) {
    if ($boost = $this->getBoostedField($fieldname)) {
      return $fieldname . '^' . $boost;
    }
    return FALSE;
  }

  /**
   * Get the value of a boosted field.
   *
   * @param string $fieldname
   *   Name of field to boost.
   *
   * @return bool|mixed|null
   *   Boost value or FALSE if no boost available.
   */
  protected function getBoostedField($fieldname) {
    $fieldname_variable = $this->indexName . '_' . $fieldname;
    $boost = variable_get($fieldname_variable, NULL);
    if (!isset($boost)) {
      if (!isset($this->fieldWeights[$fieldname])) {
        return FALSE;
      }
      $boost = $this->fieldWeights[$fieldname];;
    }
    return $boost;
  }

  /**
   * Return list of boostable fields.
   *
   * @return array|bool
   *   Boostable fields or FALSE if none defined.
   */
  public function getBoostableFields() {
    if (isset($this->fieldWeights) && count($this->fieldWeights) > 0) {
      return $this->fieldWeights;
    }
    return FALSE;
  }

  /**
   * Return list of field names with boost data.
   *
   * @return array
   *   Array of ES field names with boost.
   */
  protected function getBoostedFieldNames() {
    return array_values(array_filter(array_map(array('BaseElasticsearchConnector', 'boostField'), $this->getFieldNames())));
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
   * Return the total number of hits used for the Pager utility.
   *
   * @return bool|float|int
   *   Number of hits.
   */
  public function getNumberOfHitsForPager() {
    $max_num_hits = $this->getResultsPerPage() * variable_get('zsearch_es_max_pages', 15);
    return ($this->getNumberOfHits() < $max_num_hits) ? $this->getNumberOfHits() : $max_num_hits;
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
    return $this->numHits;
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
    $params = array(
      'index' => $this->indexName,
      'type' => $this->type,
      'body' => $query,
    );
    return $params;
  }

  /**
   * Check for Logging Data.
   *
   * @return bool
   *   Returns true if there is a log id indicating the data has been stored.
   */
  public function hasData() {
    return isset($this->logId) ? TRUE : FALSE;
  }

  /**
   * Calculate the adjusted score based on the results.
   *
   * @param array $result
   *   Result array from ElasticSearch.
   *
   * @return float
   *   Return score for query boosting.
   */
  public function getScoreAdjusted(array $result) {
    // Base Algorithm.
    $adjusted = round(($this->response['hits']['max_score'] - $result['_score']) / $this->response['hits']['max_score'], 3);
    return $adjusted;
  }

  /**
   * Return Index Object, or just the name of the Index Class.
   *
   * @param string $index_name
   *   Name of index.
   * @param bool $just_name
   *   If true returns class name, otherwise returns object.
   *
   * @return bool|string
   *   Returns object, class name, or FALSE.
   */
  public static function getIndex($index_name, $just_name = FALSE) {
    $class_name = ucfirst($index_name) . 'ElasticsearchConnector';
    if (class_exists($class_name)) {
      if ($just_name) {
        return $class_name;
      }
      return new $class_name();
    }
    return FALSE;
  }

  /**
   * Get Index Name from Entity.
   *
   * @param string $entity_type
   *   Entity Type.
   * @param object $entity
   *   Entity Object.
   *
   * @return bool|string
   *   Name of Index, or FALSE.
   */
  public static function getIndexName($entity_type, $entity) {
    if ($entity_type === 'node') {
      switch ($entity->type) {
        case 'tour':
          return 'tours';

        case 'location':
          return 'locations';
      }
    }

    return FALSE;

  }

  /**
   * Get Custom Stopwords.
   *
   * Return list of defined stopwords from variable in admin.
   *
   * @return array
   *   List of stopwords.
   */
  protected function getStopwords() {
    $custom_stopwords = explode('|', variable_get('zsearch_es_custom_stopwords', ''));
    if (count($custom_stopwords)) {
      return array_merge($this->stopWords, $custom_stopwords);
    }
    return $this->stopWords;
  }

  /**
   * Sanitize the term to be searched.
   *
   * @param string $term
   *   Search term from input.
   *
   * @return null|string|string[]
   *   Search term to be searched on.
   */
  protected function sanitizeSearchTerm($term) {
    $term = preg_replace('/\b(' . implode('|', $this->getStopwords()) . ')\b/', '', $term);
    $term = search_simplify($term);
    $term = trim(preg_replace('/\s+/', ' ', $term));
    return $term;
  }

  /**
   * Extract useful parts of ElasticSearch Explain function data.
   *
   * @param array $section
   *   Section array.
   * @param array $list
   *   List of useful parts.
   *
   * @return bool
   *   return true.
   */
  public function extractExplainDescriptions(array $section, array &$list) {
    foreach ($section as $key => $value) {
      switch ($key) {
        case 'description':
          if (
            (strpos($value, 'weight(') === 0) ||
            (strpos($value, 'duration') === 0)
          ) {
            $list[] = $value . '...';
          }
          break;

        case 'details':
          foreach ($value as $detail) {
            $this->extractExplainDescriptions($detail, $list);
          }
      }
    }
    return TRUE;
  }

  /**
   * LogId setter.
   *
   * @param string $logId
   *   Log Id.
   */
  public function setLogId($logId) {
    if (isset($logId)) {
      $this->logId = $logId;
    }
  }

  /**
   * Get the base type of an index from its ES index name.
   *
   * @param string $index_name
   *   ES index name.
   *
   * @return bool|string
   *   Base name.
   */
  public static function indexClassMapping($index_name) {
    $index_label = FALSE;
    switch ($index_name) {
      case 'zsearch_activities':
        $index_label = 'activities';
        break;

      case 'zsearch_tours':
        $index_label = 'tours';
        break;

      case 'zsearch_locations':
        $index_label = 'locations';
        break;
    }
    return $index_label;
  }

  /**
   * Return a count of the records that should be indexed.
   *
   * @return int
   *   Record count.
   */
  abstract public function getCountIndexable();

  /**
   * Return a count of the records in the index, from Count API not stats.
   *
   * Reason being that the doc count in stats includes nested content which
   * isn't so useful for comparing to local data.
   *
   * @return int
   *   Document count.
   */
  public function getCountIndexed() {
    $params = $this->getParams(array());
    try {
      $response = $this->client->count($params);
      return $response['count'];
    }
    catch (Exception $e) {
      drupal_set_message($e->getMessage(), 'error');
    }
  }

}

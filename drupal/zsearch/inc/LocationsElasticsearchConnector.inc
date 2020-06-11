<?php

/**
 * @file
 * Contains the Locations ElasticSearch Connector implementation class.
 */

/**
 * The Location content connector.
 */
class LocationsElasticsearchConnector extends NodeElasticsearchConnector {

  protected $fields = array(
    'location' => array(
      'type' => 'text',
      'analyzer' => 'plain_text_loc_syn',
    ),
    'featured' => array(
      'properties' => array(
        'value' => array('type' => 'text', 'analyzer' => 'plain_text_whitespace'),
      ),
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
    $this->indexName = 'zsearch_locations';
    $this->type = 'locations';
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
      'featured' => $this->nestedValues($location_wrapper->field_elasticsearch_priority_tag->value()),
    ];
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

    // Split term into staggered pairs.
    $term_words = explode(' ', $term);
    $term_sets = array();
    if (count($term_words) === 1) {
      $term_sets = $term_words;
    }
    else {
      for ($i = 0; $i < (count($term_words) - 1); $i++) {
        $term_sets[] = $term_words[$i] . ' ' . $term_words[$i + 1];
      }
    }

    foreach ($term_sets as $term_set) {
      $booleanSearchSet['should'][] = array(
        'match' => array(
          'location' => array(
            'query' => $term_set,
            'operator' => 'and',
            'fuzziness' => 1,
            'prefix_length' => 4,
            'boost' => 2,
          ),
        ),
      );
    }

    $booleanSearchSet['should'][] = array(
      'match' => array(
        'location' => array(
          'query' => $term,
          'fuzziness' => 1,
          'prefix_length' => 4,
        ),
      ),
    );

    // Featured match, prioritize all words.
    $featuredBooleanSubquery = array();
    foreach ($term_words as $term_word) {
      $featuredBooleanSubquery['should'][] = array(
        'match' => array(
          'featured.value' => array(
            'query' => $term_word,
            'fuzziness' => 1,
            'prefix_length' => 4,
            'boost' => 40,
          ),
        ),
      );
    }
    $featuredBooleanSubquery['boost'] = 3;

    // Add Boolean search for keywords on locations.
    $booleanSearchSet['should'][] = array(
      'bool' => $featuredBooleanSubquery,
    );

    $this->query = array(
      'query' => array(
        'bool' => $booleanSearchSet,
      ),
    );

    $wrapper = array(
      'highlight' => array(
        'pre_tags' => array(''),
        'post_tags' => array(''),
        'fields' => array(
          'location' => array(
            'fragment_size' => 0,
          ),
          'featured.value' => array(
            'fragment_size' => 0,
          ),
        ),
      ),
    );

    $bodyQuery = array_merge($this->query, $wrapper);

    $params = array(
      'index' => $this->indexName,
      'type' => $this->type,
      'body' => $bodyQuery,
    );
    try {
      $response = $this->client->search($params);
      $this->response = $response->getRawResponse();
      $this->logSearch($term);
      return $this->getNumberOfHits();
    }
    catch (Exception $e) {
      drupal_set_message($e->getMessage(), 'error');
    }
  }

  /**
   * Determine if a score is good enough to be listed.
   *
   * @param float $score
   *   Score returned from ElasticSearch.
   *
   * @return bool
   *   True if the score is good enough, false if not.
   */
  protected function isAcceptableScore($score) {
    if ($score > $this->max_score) {
      $this->max_score = $score;
      return TRUE;
    }
    $offsetMin = (($this->max_score / 3) > variable_get('zsearch_es_min_score')) ? $this->max_score / 3 : variable_get('zsearch_es_min_score');
    if ($score > $offsetMin) {
      return TRUE;
    }
    return FALSE;
  }

  /**
   * Processes result data into a form useful for the front end.
   *
   * @param array $item
   *   Document item.
   * @param string $key
   *   Array index.
   *
   * @return bool
   *   Return false if the result is not acceptable.
   */
  protected function processResults(array $item, $key) {
    if (!$this->isAcceptableScore($item['_score'])) {
      return FALSE;
    }
    $location = strtolower($item['_source']['location']);
    $this->results['matches'][] = array('location' => $location, 'score' => $item['_score']);
    if (isset($item['highlight']['location']) && count($item['highlight']['location'])) {
      // We have a highlighted location match, add to array.
      $location = array_map('strtolower', array_map('trim', $item['highlight']['location']));
      foreach ($location as $locationPart) {
        $this->results['terms'][$locationPart] = $locationPart;
      }
    }
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

    // Get Nids of excluded Locations.
    $excluded_items = db_select('node', 'exn');
    $excluded_items->join('field_data_field_exclude', 'ex', 'ex.entity_id = exn.nid');
    $excluded_items->fields('exn', array('nid'))
      ->condition('exn.type', 'location')
      ->condition('exn.status', 0, '>')
      ->condition('ex.field_exclude_value', 1);
    $result = $excluded_items->execute();
    $excluded_nids = $result->fetchCol();


    // 218 -> City/Place.
    // 219 -> Continent.
    // 220 -> Country.
    // 226 -> Sub-Continent.
    $types = '218, 219, 220, 226';

    $query = db_select('node', 'n');
    $query->join('field_data_field_location_type', 'lt', 'n.nid = lt.entity_id AND lt.field_location_type_tid IN (' . $types . ')');
    $query->fields('n', array(
      'nid',
    ))
      ->condition('n.status', 0, '>')
      ->condition('n.type', 'location');

    if (count($excluded_nids)) {
      $query->condition('n.nid', $excluded_nids, 'NOT IN');
    }

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

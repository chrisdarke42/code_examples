<?php

/**
 * @file
 * Contains the Tours ElasticSearch Connector implementation class.
 */

/**
 * The Tour content connector.
 */
class ToursElasticsearchConnector extends NodeElasticsearchConnector {

  protected $fieldWeights = array(
    'title' => 1,
    'summary' => 0.9,
    'highlights' => 0.8,
    'glance' => 0.7,
    'countries' => 0.6,
    'description' => 0.5,
    'itinerary' => 0.4,
    'places' => 0.3,
    'included' => 0.2,
    'themes' => 0.08,
    'features' => 0.06,
    'departure' => 0.04,
    'attractions' => 0.02,
  );

  protected $fields = array(
    'locations' => array(
      'type' => 'nested',
      'properties' => array(
        'name' => array('type' => 'text', 'analyzer' => 'just_lowercase'),
      ),
    ),
    'title' => array(
      'type' => 'text',
      'analyzer' => 'plain_text',
    ),
    'duration' => array(
      'type' => 'integer',
    ),
    'summary' => array(
      'type' => 'text',
      'analyzer' => 'html_body',
    ),
    'highlights' => array(
      'type' => 'text',
      'analyzer' => 'plain_text',
    ),
    'glance' => array(
      'type' => 'text',
      'analyzer' => 'plain_text',
    ),
    'countries' => array(
      'type' => 'text',
      'analyzer' => 'plain_text',
    ),
    'description' => array(
      'type' => 'text',
      'analyzer' => 'html_body',
    ),
    'itinerary' => array(
      'type' => 'text',
      'analyzer' => 'html_body',
    ),
    'places' => array(
      'type' => 'text',
      'analyzer' => 'html_body',
    ),
    'included' => array(
      'type' => 'text',
      'analyzer' => 'html_body',
    ),
    'activities' => array(
      'type' => 'nested',
      'properties' => array(
        'name' => array('type' => 'text', 'analyzer' => 'just_lowercase'),
      ),
    ),
    'themes' => array(
      'type' => 'text',
      'analyzer' => 'plain_text',
    ),
    'features' => array(
      'type' => 'text',
      'analyzer' => 'plain_text',
    ),
    'departure' => array(
      'type' => 'text',
      'analyzer' => 'plain_text',
    ),
    'attractions' => array(
      'type' => 'text',
      'analyzer' => 'plain_text',
    ),
  );

  protected $analyzers = array(
    'html_body' => array(
      'char_filter' => array('html_body_filter'),
      'tokenizer' => 'standard',
      'filter' => array(
        'english_possessive_stemmer',
        'lowercase',
        'apostrophe',
        'english_stemmer',
        'synonym',
        'location_synonyms',
        'zicasso_synonyms',
        'english_stop',
      ),
    ),
    'plain_text' => array(
      'tokenizer' => 'standard',
      'filter' => array(
        'english_possessive_stemmer',
        'lowercase',
        'apostrophe',
        'english_stemmer',
        'synonym',
        'location_synonyms',
        'zicasso_synonyms',
        'english_stop',
      ),
    ),
    'just_lowercase' => array(
      'tokenizer' => 'keyword',
      'filter' => array(
        'lowercase',
      ),
    ),
  );

  protected $tours_query = array();
  protected $refine_query = array();
  protected $locations_query = array();
  protected $locations = array();
  protected $activities = array();
  protected $search_terms = NULL;
  protected $missing_terms = array();

  /**
   * ToursElasticsearchConnector constructor.
   *
   * @throws \Exception
   */
  public function __construct() {
    $this->indexName = 'zsearch_tours';
    $this->type = 'tours';
    parent::__construct();
  }

  /**
   * Get object from cache id, or load a new one if cache id invalid.
   *
   * @param string $cid
   *   Cache Id.
   *
   * @return \ToursElasticsearchConnector
   *   ElasticSearch Connector for Tour.
   *
   * @throws \Exception
   */
  public static function loadOrNew($cid) {
    $cachedClass = cache_get($cid);
    if ($cachedClass === FALSE) {
      return new ToursElasticsearchConnector();
    }
    else {
      $class = $cachedClass->data;
      $class->setClient();
      return $class;
    }
  }

  /**
   * Parse locations into nested value set for indexing.
   *
   * @param array $values
   *   Locations for node.
   *
   * @return array
   *   Structure for indexing.
   */
  protected function nestedValues(array $values = array()) {
    $location_array = array();
    foreach ($values as $value) {
      $location_array[] = array('name' => $value);
    }
    return $location_array;
  }

  /**
   * Parse wrapper to return an array for indexing.
   *
   * @param object $tour_wrapper
   *   Entity Metadata Wrapper of node.
   *
   * @return array
   *   Array of data for indexing.
   */
  protected function parseWrapperIndexing($tour_wrapper) {
    return [
      'title' => $tour_wrapper->title->value(),
      'duration' => $this->integerValue('field_trip_duration', $tour_wrapper, 'TourDuration'),
      'summary' => $this->longTextValue('field_summary', $tour_wrapper, TRUE),
      'highlights' => $this->longTextValue('field_tour_highlights', $tour_wrapper),
      'glance' => $this->longTextValue('field_trip_glance', $tour_wrapper),
      'countries' => $this->entityReferenceValue('field_countries', $tour_wrapper, ' '),
      'description' => $this->longTextValue('body', $tour_wrapper),
      'itinerary' => $this->longTextValue('field_itinerary', $tour_wrapper),
      'places' => $this->longTextValue('field_places_visited', $tour_wrapper),
      'included' => $this->longTextValue('field_included_price', $tour_wrapper),
      'activities' => $this->nestedValues($this->entityReferenceValue('field_travel_activities', $tour_wrapper)),
      'themes' => $this->entityReferenceValue('field_travel_themes', $tour_wrapper, ' '),
      'features' => $this->entityReferenceValue('field_travel_features', $tour_wrapper, ' '),
      'departure' => $this->longTextValue('field_departures', $tour_wrapper),
      'attractions' => $this->entityReferenceValue('field_attractions', $tour_wrapper, ' '),
      'locations' => $this->nestedValues($this->entityReferenceValue('field_locations', $tour_wrapper)),
    ];
  }

  /**
   * Tour Duration conversion to number value.
   *
   * @param string $value
   *   String value from select.
   *
   * @return float|null
   *   Integer value or null if invalid.
   */
  protected function parseTourDuration($value) {
    // For some reason value is already parsed.
    if (is_numeric($value)) {
      return $value;
    }
    if (strlen(trim($value)) > 0) {
      $now = strtotime('now');
      $spanEnd = strtotime('+' . $value);
      if ($spanEnd !== FALSE) {
        $duration = ceil(($spanEnd - $now) / 86400);
        if ($duration > 0) {
          return $duration;
        }
      }
    }
    return NULL;
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
      'title' => $item['_source']['title'],
      'summary' => $item['_source']['summary'],
      'duration' => $item['_source']['duration'],
      'locations' => array_column($item['_source']['locations'], 'name'),
      '_missing' => $item['_missing'],
    );
    $this->results[] = $entry;
  }

  /**
   * Analyze and extract duration values from search query.
   *
   * @param string $term
   *   Search term.
   *
   * @return array|mixed
   *   The number of day durations found.
   */
  protected function extractDuration(&$term) {
    $regex = '/([\d,]+|a)[ ]+(day|week|month)[s]*\b/';
    $matches = array();
    $result = array();
    if (preg_match_all($regex, $term, $matches) > 0) {
      $term = preg_replace($regex, '', $term);
      // Get how many days these is if its not in days.
      if (isset($matches[2])) {
        // Check if 'a' was used.
        foreach ($matches[1] as $index => $match_a) {
          // Swap out 'a [period]' for '1 [period]' to appease strtotime.
          if ($match_a == 'a') {
            $matches[1][$index] = 1;
            $matches[0][$index] = '1 ' . $matches[2][$index];
          }
        }
        foreach ($matches[2] as $index => $match_name) {
          if ($match_name !== 'day') {
            $parsed_duration = $this->parseTourDuration($matches[0][$index]);
            if ($parsed_duration !== NULL) {
              $matches[1][$index] = $parsed_duration;
            }
          }
        }
      }
      $result = $matches[1];
    }
    return $result;
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

    if (strlen($term) === 0) {
      return 0;
    }

    // Run this before sanitization.
    $daysMatches = $this->extractDuration($term);

    // Clean up the rest of the query.
    $term = $this->sanitizeSearchTerm($term);

    if (strlen($term) === 0 && count($daysMatches) === 0) {
      return 0;
    }

    $this->search_terms = $term;

    // Initialise array of search requirements.
    $mustArray = array();
    $shouldArray = array();
    $termArray = array();


    $precursorIndices = array('locations', 'activities');

    foreach ($precursorIndices as $pIndex) {
      // Run search terms through precursor filtering.
      $results = $this->getPrecursor($term, $pIndex);
      $this->processPrecursor($term, $results, $pIndex, $mustArray, $termArray);
    }

    // Remove duplicates.
    $termArray = array_unique($termArray);

    // Set up Boolean Array of Conditions.
    $booleanSearchSet = array();

    // Locations were set with a Must, that means we need to limit those with
    // a should for the rest of the params.
    if (count($mustArray)) {
      // Internal boolean for must locations.
      $internalBoolean = array();
      if (count($mustArray) > 1) {
        $internalBoolean['must'] = array('bool' => array('should' => $mustArray));
      }
      else {
        $internalBoolean['must'] = $mustArray;
      }

      // If we are enforcing location, we should also add a lower graded search
      // for tours with location mentions but without location set.
      // Downgraded boost as this is the least accurate of the set of data.
      $booleanSearchSet['should'][] = array(
        'multi_match' => array(
          'query' => $term,
          'type' => 'cross_fields',
          'fields' => $this->getBoostedFieldNames(),
          'boost' => variable_get('zsearch_es_backup_boost', 1),
        ),
      );

      if (count($termArray)) {
        foreach ($termArray as $termItem) {
          $shouldArray[] = array(
            'multi_match' => array(
              'query' => $termItem,
              'type' => 'cross_fields',
              'fields' => $this->getBoostedFieldNames(),
              'boost' => variable_get('zsearch_es_plain_search_boost', 1),
            ),
          );
        }
      }

      if (count($shouldArray)) {
        $internalBoolean['should'] = $shouldArray;
        $internalBoolean['boost'] = 1;
      }
      else {
        $booleanSearchSet['should'][0]['multi_match']['boost'] = 4;
      }

      $booleanSearchSet['should'][] = array('bool' => $internalBoolean);
    }
    else {
      // If there are no location matches we fall back to a multimatch search.
      $mustArray[] = [
        'multi_match' => [
          'query' => $term,
          'type' => 'cross_fields',
          'fields' => $this->getBoostedFieldNames(),
          'boost' => 4,
        ],
      ];
      $booleanSearchSet['must'] = $mustArray;
    }

    // Do duration searches if they exist.
    if (count($daysMatches)) {
      foreach ($daysMatches as $numDays) {
        $booleanSearchSet['filter'][] = array(
          'term' => array('duration' => $numDays),
        );
      }
    }

    $from = $page * $this->getResultsPerPage();
    $size = $this->getResultsPerPage();

    // Separated query for use in other places.
    $this->tours_query = array(
      'query' => array(
        'bool' => $booleanSearchSet,
      ),
    );

    // Wrapper to limit what is returned.
    $wrapper = array(
      '_source' => array(
        'includes' => array(
          'title',
          'summary',
          'duration',
          'locations',
        ),
      ),
      'min_score' => variable_get('zsearch_es_min_score', 0),
      'from' => $from,
      'size' => $size,
    );

    // Query to inject into body.
    $bodyQuery = array_merge($this->tours_query, $wrapper);
    // Setting up params to pass to client.
    $params = $this->getParams($bodyQuery);
    try {
      $response = $this->client->search($params);
      $this->response = $response->getRawResponse();
      $this->refineResults();
      $this->logSearch($term, $page);
      return $this->getNumberOfHitsForPager();
    }
    catch (Exception $e) {
      drupal_set_message($e->getMessage(), 'error');
    }
    return FALSE;
  }

  /**
   * Refine results with extra data, like missing words.
   */
  protected function refineResults() {
    $hits = $this->response['hits']['hits'];
    if (count($hits)) {
      $term_parts = explode(' ', $this->search_terms);
      foreach ($term_parts as $term_part) {
        $search_fields = $this->getTextSearchFieldNames();

        // Do multimatch terms.
        $booleanMatchTerm['should'][] = array(
          'multi_match' => array(
            '_name' => $term_part,
            'query' => $term_part,
            'type' => 'cross_fields',
            'fields' => $search_fields['multimatch'],
          ),
        );

        if (isset($search_fields['nested'])) {
          foreach ($search_fields['nested'] as $path => $field) {
            $booleanMatchTerm['should'][] = array(
              'nested' => array(
                '_name' => $term_part . '_[' . $path . ']',
                'path' => $path,
                'query' => array(
                  'bool' => array(
                    'must' => array(
                      'match' => array(
                        $field => $term_part,
                      ),
                    ),
                  ),
                ),
              ),
            );
          }
        }
      }

      $ids = array_column($hits, '_id');

      $booleanMatchTerm['filter'] = array('ids' => array('values' => $ids));

      $this->refine_query = array(
        'query' => array(
          'bool' => $booleanMatchTerm,
        ),
      );

      // Wrapper to limit what is returned.
      $size = $this->getResultsPerPage();
      $wrapper = array(
        '_source' => array(
          'includes' => array(
            'title',
          ),
        ),
        'size' => $size,
      );

      // Query to inject into body.
      $query = array_merge($this->refine_query, $wrapper);
      $params = $this->getParams($query);
      try {
        $response = $this->client->search($params);
        $refine_results = $response->getRawResponse();
        // Process matched queries.
        $missing = array();
        foreach ($refine_results['hits']['hits'] as $result) {
          // Get difference between matched queries and search terms:
          if (isset($result['matched_queries'])) {
            $pattern = '/(\S*)_\[\S*\]/';
            $result['matched_queries'] = preg_replace($pattern, '$1', $result['matched_queries']);
            $missing[$result['_id']] = array_diff($term_parts, $result['matched_queries']);
          }
          else {
            $missing[$result['_id']] = $term_parts;
          }

        }
        $this->missing_terms = $missing;
        $results_hits = $this->response['hits']['hits'];
        foreach ($results_hits as $key => $search_result) {
          $search_result['_missing'] = array();
          if (isset($missing[$search_result['_id']])) {
            $results_hits[$key]['_missing'] = $missing[$search_result['_id']];
          }
        }
        $this->response['hits']['hits'] = $results_hits;
      }
      catch (Exception $e) {
        drupal_set_message($e->getMessage(), 'error');
      }
    }
  }

  /**
   * Normalize Range.
   *
   * This takes a given value within a range and places it within a new range.
   *
   * @param float $val
   *   Value to re-range.
   * @param float $max
   *   Maximum value of initial range.
   * @param float $min
   *   Minimum value of initial range.
   * @param float $minR
   *   Minimum value of new range.
   * @param float $maxR
   *   Maximum value of new range.
   *
   * @return float|int
   *   Re-ranged value.
   */
  protected function normalizeRange($val, $max, $min, $minR, $maxR) {
    if ($max === $min) {
      $divisor = 1;
    }
    else {
      $divisor = $max - $min;
    }
    return ((($maxR - $minR) * ($val - $min)) / $divisor) + $minR;
  }

  /**
   * Process a precursor for data to flesh out the main search more accurately.
   *
   * @param string $term
   *   Search term.
   * @param array $precursor
   *   Precursor response data.
   * @param string $precursor_index_name
   *   Name of index of precursor.
   * @param array $mustArray
   *   Array to store 'must' conditions.
   * @param array $termArray
   *   Array to store terms for backup search.
   */
  protected function processPrecursor($term, array $precursor, $precursor_index_name, array &$mustArray, array &$termArray) {
    $index = $this->getIndex($precursor_index_name, TRUE);
    if ($index === FALSE) {
      return;
    }
    if (isset($precursor['matches']) && count($precursor['matches'])) {
      // Normalize the scores for the different locations.
      $max = $precursor['matches'][0]['score'];
      $min = count($precursor['matches']) > 1 ? $precursor['matches'][count($precursor['matches']) - 1]['score'] : 0;

      // Set min max range for normalized weight range.
      $minR = 1;
      $maxR = 10;
      // Add an extra check for small ranges.
      if (($max - $min) < 1) {
        $minR = 9;
      }
      $matches = array();
      // Go through and remove matches from search string.
      foreach ($precursor['matches'] as $match) {
        $weightAdjust = $this->normalizeRange($match['score'], $max, $min, $minR, $maxR);
        $matches[] = $index::precursorMatchItem($match, $weightAdjust);
        if (strpos($term, $index::getPrecursorKeyedData($match)) !== FALSE) {
          // This match is found in the search string,
          // lets add this to the search.
          $termCheck = trim(str_replace($index::getPrecursorKeyedData($match), '', $term));
          if (strlen($termCheck) > 0) {
            $termArray[] = $termCheck;
          }
        }
        if (property_exists($this, $precursor_index_name)) {
          $this->{$precursor_index_name}[] = $match;
        }
      }
      $mustArray[] = $index::precursorMatch($matches);

      // Filter matches out of search terms.
      $termItems = explode(' ', $term);
      $newTermItems = array();
      foreach (array_filter($termItems) as $termItem) {
        if (!isset($precursor['terms'][$termItem])) {
          $newTermItems[] = $termItem;
        }
      }
      if (count($newTermItems)) {
        $termArray[] = trim(implode(' ', $newTermItems));
      }
    }
  }

  /**
   * Get locations from index based on search term.
   *
   * @param string $term
   *   Full search term.
   * @param string $index_name
   *   The name of the precursor index.
   *
   * @return array
   *   Results returned by search
   *
   * @throws \Exception
   *   Exception thrown if search fails.
   */
  protected function getPrecursor($term, $index_name) {
    $precursor = $this->getIndex($index_name);
    $precursor->setLogId($this->logId);
    $precursor->search($term);
    $query_storage_local = $index_name . '_query';
    if (property_exists($this, $query_storage_local)) {
      $this->{$query_storage_local} = $precursor->getQuery();
    }
    $this->logId = $precursor->logId;
    return $precursor->getResults();
  }

  /**
   * Format search results for logging.
   *
   * @return array
   *   List of results ready for logging.
   */
  protected function getLogFormatted() {
    $results = array();
    $results['max_score'] = $this->response['hits']['max_score'];
    foreach ($this->response['hits']['hits'] as $hit) {
      $results['hits'][] = array(
        'id' => $hit['_id'],
        'score' => $hit['_score'],
        'title' => $hit['_source']['title'],
      );
    }
    return $results;
  }

  /**
   * Generate Cache ID.
   *
   * @return string
   *   Cache ID.
   */
  public function getCacheId() {
    return $this->indexName . '_' . $this->logId;
  }

  /**
   * Save the search to cache.
   *
   * @param object $connector
   *   Connector object to cache.
   */
  public static function saveToCache($connector) {
    unset($connector->client);
    cache_set($connector->getCacheId(), $connector);
  }

  /**
   * Explain the current query using ElasticSearch Explain.
   *
   * @param int $nid
   *   Node id of the tour.
   *
   * @return mixed
   *   Explain data array.
   */
  public function explainQuery($nid) {
    $params = $this->getParams($this->tours_query);
    $params['id'] = $nid;
    $response = $this->client->explain($params);
    return $response;
  }

  /**
   * Get the query JSON for debugging.
   *
   * @param string $index
   *   Name of index.
   *
   * @return bool|string
   *   The query json for the given index.
   */
  public function getQueryJson($index) {
    $query_name = $index . '_query';
    if (property_exists($this, $query_name)) {
      return json_encode($this->{$query_name});
    }
    return FALSE;
  }

  /**
   * Get locations for current search.
   *
   * @param bool $formatted
   *   Flag to return in formatted form.
   *
   * @return array|string
   *   Array of locations or string of locations.
   */
  public function getLocations($formatted = FALSE) {
    $locations = array_column($this->locations, 'location');
    if ($formatted) {
      return implode('<br>', $locations);
    }
    return $locations;
  }

  /**
   * Get activites for current search.
   *
   * @param bool $formatted
   *   Flag to return in formatted form.
   *
   * @return array|string
   *   Array of activities or string of activities.
   */
  public function getActivities($formatted = FALSE) {
    $activities = array_column($this->activities, 'activity');
    if ($formatted) {
      return implode('<br>', $activities);
    }
    return $activities;
  }

  /**
   * Run through all content and parse for indexing.
   *
   * @param object $context
   *   Batch API operations context.
   *
   * @return mixed|null|string
   *   Result of batch operations.
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
        return t('Tours indexed into ElasticSearch : @message', array(
          '@message' => $final_message,
        ));
      }
    }
  }

  /**
   * Generate SelectQuery to get tours.
   *
   * @return \SelectQuery
   *   Drupal SelectQuery object.
   */
  protected function getNodeQuery() {

    $query = db_select('node', 'n');
    $query->join('field_data_field_tour_highlights', 'h', 'n.vid = h.revision_id');
    $query->fields('n', array(
      'nid',
    ))
      ->condition('n.type', 'tour')
      ->condition('n.status', 0, '>');

    return $query;
  }

}

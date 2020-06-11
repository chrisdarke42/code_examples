<?php

/**
 * @file
 * Contains the Locations ElasticSearch Connector implementation class.
 */

/**
 * The Location content connector.
 */
class ActivitiesElasticsearchConnector extends TaxonomyTermElasticsearchConnector {

  protected $fields = array(
    'activity' => array(
      'type' => 'text',
      'analyzer' => 'plain_text_en_syn',
    ),
  );

  protected $analyzers = array(
    'plain_text_en_syn' => array(
      'tokenizer' => 'standard',
      'filter' => array(
        'english_possessive_stemmer',
        'lowercase',
        'apostrophe',
        'english_stemmer',
        'synonym',
        'zicasso_synonyms',
        'english_stop',
      ),
    ),
  );

  /**
   * ActivitiesElasticsearchConnector constructor.
   *
   * @throws \Exception
   */
  public function __construct() {
    $this->indexName = 'zsearch_activities';
    $this->type = 'activities';
    parent::__construct();
  }

  /**
   * Parse wrapper to return an array for indexing.
   *
   * @param object $activity_wrapper
   *   Entity Metadata Wrapper of node.
   *
   * @return array
   *   Array of data for indexing.
   */
  protected function parseWrapperIndexing($activity_wrapper) {
    return [
      'activity' => $activity_wrapper->name->value(),
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
    $query = array(
      'query' => array(
        'match' => array(
          'activity' => array(
            'query' => $term,
            'fuzziness' => 1,
            'prefix_length' => 4,
          ),
        ),
      ),
      'highlight' => array(
        'pre_tags' => array(''),
        'post_tags' => array(''),
        'fields' => array(
          'activity' => array(
            'fragment_size' => 0,
          ),
        ),
      ),
    );

    $params = array(
      'index' => $this->indexName,
      'type' => $this->type,
      'body' => $query,
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
   * Processes result data into a form useful for the front end.
   *
   * @param array $item
   *   Document item.
   * @param string $key
   *   Array index.
   */
  protected function processResults(array $item, $key) {
    $activity = strtolower($item['_source']['activity']);
    $this->results['matches'][] = array('activity' => $activity, 'score' => $item['_score']);
    if (isset($item['highlight']['activity']) && count($item['highlight']['activity'])) {
      // We have a highlighted location match, add to array.
      $activity = array_map('strtolower', array_map('trim', $item['highlight']['activity']));
      foreach ($activity as $activityPart) {
        $this->results['terms'][$activityPart] = $activityPart;
      }
    }
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
      $context['sandbox']['current_term'] = -1;
      $context['sandbox']['iterator'] = 0;
      $context['sandbox']['max'] = $this->getTermQueryCount();
    }

    $limit = 100;

    // Retrieve the next group of tids.
    $query = $this->getTermQuery();
    $query->orderBy('a.tid', 'ASC')
      ->where('a.tid > :tid', array(
        ':tid' => $context['sandbox']['current_term'],
      ))
      ->range(0, $limit);


    $result = $query->execute();
    $tid_array = $result->fetchCol();
    try {
      $indexed_items = $this->indexDocuments($tid_array);
      $context['sandbox']['progress'] = $context['sandbox']['progress'] + count($indexed_items);
      $last_item = end($indexed_items);
      $context['sandbox']['current_term'] = $last_item['index']['_id'];
    }
    catch (Exception $e) {
      drupal_set_message($e->getMessage(), 'error');
    }
    $context['sandbox']['iterator']++;
    $context['results'] = array_merge($context['results'], $tid_array);
    $context['finished'] = $context['sandbox']['progress'] >= $context['sandbox']['max'] ? TRUE : $context['sandbox']['progress'] / $context['sandbox']['max'];
    if ($context['sandbox']['iterator'] > ($context['sandbox']['max'] / $limit) * 2) {
      $context['finished'] = TRUE;
      if ($context['hook_install'] === TRUE) {
        $final_message = implode('', $context['sandbox']['messages']);
        return t('Activities indexed into ElasticSearch : @message', array(
          '@message' => $final_message,
        ));
      }
    }
  }

  /**
   * Generate SelectQuery to get activites.
   *
   * @return \SelectQuery
   *   Drupal SelectQuery object.
   */
  protected function getTermQuery() {
    $vocab = taxonomy_vocabulary_machine_name_load('vocab_travel_activities');
    $query = db_select('taxonomy_term_data', 'a');
    $query->fields('a', array(
      'tid',
    ))
      ->condition('a.vid', $vocab->vid);
    return $query;
  }

  /**
   * Get internal query for matching precursor results.
   *
   * @param array $activities
   *   List of activities matched.
   * @param float $weightAdjust
   *   Weight adjustment for this list of activities.
   *
   * @return array
   *   Match query for Bool Query.
   */
  public static function precursorMatchItem(array $activities, $weightAdjust) {
    return array('match' => array('activities.name' => array('query' => $activities['activity'], 'boost' => $weightAdjust)));
  }

  /**
   * Get external query for bool EQL.
   *
   * @param array $matches
   *   Matched terms from precursor query.
   *
   * @return array
   *   Bool Query section.
   */
  public static function precursorMatch(array $matches) {
    return array(
      'nested' => array(
        'path' => 'activities',
        'score_mode' => 'max',
        'query' => array('bool' => array('should' => $matches, 'boost' => 1.4)),
      ),
    );
  }

  /**
   * Get the keyed items for the search from the precursor search.
   *
   * @param array $match
   *   Precursor result.
   *
   * @return mixed
   *   Term matched.
   */
  public static function getPrecursorKeyedData(array $match) {
    return $match['activity'];
  }

}

<?php

class FileElasticsearch extends BaseElasticsearch {

  protected $file_data;

  protected $collection_id;

  /**
   * NodeElasticsearch constructor.
   *
   * @throws \Exception
   */
  public function __construct($collection_id = NULL) {
    $this->indexName = 'collection_files';
    if (isset($collection_id)) {
      $this->collection_id = $collection_id;
    }
    parent::__construct();
  }

  public function get_collection_id() {
    if (isset($this->collection_id)) {
      return $this->collection_id;
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

    // Page Calculations
    $from = $page * $this->getResultsPerPage();
    $size = $this->getResultsPerPage();

    $fragment_size = count(explode(' ', $term));

    $bodyQuery = array(
      '_source' => array(
        'includes' => array(
          'name',
          'field_date_created',
          'field_collection:title',
          'term_field_subtitle.*',
          'field_subtitle_value',
          'id'
        )
      ),
      'query' => array(
        'nested' => array(
          'path' => 'attachments_content',
          'inner_hits' => array(
            '_source' => array(
              'includes' => array('attachments_content.page'),
            ),
            'size' => 100,
            'sort' => array(
              'attachments_content.page' => array(
                'order' => 'asc',
              ),
            ),
            'highlight' => array(
              'fields' => array(
                'attachments_content.data' => array(
                  'pre_tags' => '<em>',
                  'post_tags' => '</em>',
                  'fragment_size' => $fragment_size,
                ),
              ),
            ),
          ),
          'score_mode' => 'max',
          'query' => array(
            'bool' => array(
              'must' => array(
                array('match' => array(
                  'attachments_content.data' => $term
                )),
              ),
            ),
          ),
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

  protected function buildFilterQueries($filters, &$bodyQuery) {
    $filters = $this->getFieldMappings($filters);
    foreach ($filters as $filter_name => $filter) {
      $keyparts = explode(':', $filter_name);
      $param_end = end($keyparts) ;
      $properties = $filter['properties'];
      if (isset($properties)) {
        switch($properties['type']) {
          case 'nested':
            $nested_filter_condition = 'should';
            if (count($filter['data']) === 0) {
              $nested_filter_condition = 'must';
            }
            $filter_set = [];
            foreach($filter['data'] as $filter_data) {
              reset($filter_data);
              $key = key($filter_data);
              $formedFilter = [
                $filter_name . '.' . $key => $filter_data[$key]
              ];
              $access = 'term';
              if (isset($properties['properties'][$key])) {
                if ($properties['properties'][$key]['type'] === 'text' &&
                  isset($properties['properties'][$key]['fields']['keyword'])) {
                  $access = 'match';
                }
              }
              $filter_set[] = [$access => $formedFilter];
            }

            if (!isset($bodyQuery['query']['bool']['filter'])) {
              $bodyQuery['query']['bool']['filter'] = [];
            }
            $nested_filter = ['nested' => [
              'path' => $filter_name,
              'query' => [
                'bool' => [
                  $nested_filter_condition => [
                    $filter_set
                  ]
                ]
              ]
            ]];
            $bodyQuery['query']['bool']['filter'][] = $nested_filter;
            break;

          case 'integer':
            if (!isset($bodyQuery['query']['bool']['filter'])) {
              $bodyQuery['query']['bool']['filter'] = [];
            }
            if (is_array($filter['data'])) {
              foreach($filter['data'] as $val) {
                $intboolean[] = [
                  'term' => [
                    $filter_name => $val[$param_end],
                  ],
                ];
              }
            }
            else {
              $intboolean[] = [
                'term' => [
                  $filter_name => $filter['data'],
                ],
              ];
            }
            $bodyQuery['query']['bool']['filter'][]['bool']['should'] = $intboolean;
            break;

          default:
            if (!isset($bodyQuery['query']['bool']['filter'])) {
              $bodyQuery['query']['bool']['filter'] = [];
            }
            $bodyQuery['query']['bool']['filter'][] = array(
              'term' => array(
                $filter_name => $filter['data'],
              ),
            );
        }
      }
      else {
        if (!isset($bodyQuery['query']['bool']['filter'])) {
          $bodyQuery['query']['bool']['filter'] = [];
        }
        $bodyQuery['query']['bool']['filter'][] = array(
          'term' => array(
            $filter_name => $filter['data'],
          ),
        );
      }
    }
  }

  protected function buildTermQueries($terms, &$bodyQuery) {
    foreach($terms as $term) {
      $key = key($term);
      $bodyQuery['query']['bool']['must'][] = array('term' => array($key => $term[$key]));
    }
  }

  public function searchFiltered($terms = array(), $filters = array()) {

    $bodyQuery = array(
      'query' => array(
      ),
      'size' => 100,
    );

    if ($collection_id = $this->get_collection_id()) {
      $filters['field_collection:nid'] = $collection_id;
    }

    if (count($filters)) {
      $this->buildFilterQueries($filters, $bodyQuery);
    }

    if (count($terms)) {
      $this->buildTermQueries($terms, $bodyQuery);
    }

    $this->debugOutput(json_encode($bodyQuery), 'File Query');
    $params = $this->getParams($bodyQuery);
    try {
      $response = $this->client->search($params);
      $this->response = $response->getRawResponse();
      $this->updateFileDataStorage();
      return $this->getNumberOfHits();
    }
    catch (Exception $e) {
      drupal_set_message($e->getMessage(), 'error');
    }
  }

  protected function updateFileDataStorage() {
    $data = $this->parsePrecursorFileData();

    if (is_array($this->file_data)) {
      $this->file_data += $data;
    }
    else {
      $this->file_data = $data;
    }
  }

  public function parsePrecursorQueryFids() {
    if (!isset($this->response)) {
      return FALSE;
    }
    $fids = [];
    if ($this->response['hits']['total'] > 0) {
      $fids = array_map(function($hit){return $hit['_source']['fid'];}, $this->response['hits']['hits']);
    }
    return $fids;
  }

  public function parsePrecursorFileData() {
    if (!isset($this->response)) {
      return FALSE;
    }
    $data = [];
    if ($this->response['hits']['total'] > 0) {
      $data = array_column(array_column($this->response['hits']['hits'], '_source'), NULL, 'fid');
    }
    return $data;
  }

  public function getFileData($fid) {
    $data = &drupal_static(__CLASS__ . '::' . __FUNCTION__);
    if (!isset($data)) {
      $data = $this->file_data;
    }
    if (!isset($data[$fid]) && $this->getFileDataFromElastic($fid)) {
      $data[$fid] = $this->file_data[$fid];
    }
    return isset($data[$fid]) ? $data[$fid] : FALSE;
  }

  protected function getFileDataFromElastic($fid) {
    $term = array('fid' => $fid);
    if ($this->searchFiltered([$term])) {
      return TRUE;
    }
  }

  /**
   * Get Set of Facets for a given Term, with a given Collection ID.
   *
   * @param $term
   *
   * @return array|bool
   */
  public function getQueryFacets($term) {
    if (!$collection_id = $this->get_collection_id()) {
      return FALSE;
    }
    $term_data = &drupal_static(__CLASS__ . '::' . __FUNCTION__);
    if (!isset($term_data[$collection_id][$term])) {
      $bodyQuery = array(
        '_source' => array(
          'includes' => array(),
        ),
        'query' => array(
          'term' => array(
            'field_collection:nid' => $this->collection_id,
          ),
        ),
        'aggs' => array(
          $term => array(
            'nested' => array(
              'path' => $term,
            ),
            'aggs' => array(
              'values' => array(
                'terms' => array(
                  'field' => $term . '.tid',
                  'size' => 20,
                ),
              ),
            ),
          ),
        ),
      );
      $params = $this->getParams($bodyQuery);
      $this->debugOutput(json_encode($bodyQuery), 'File Facet Query');
      try {
        $response = $this->client->search($params);
        $response_data = $response->getRawResponse();
        $hits = [];

        if (isset($response_data['aggregations'][$term]['values']['buckets'])) {
          foreach ($response_data['aggregations'][$term]['values']['buckets'] as $bucket) {
            $hits[$bucket['key']] = $bucket['doc_count'];
          }
        }

        $term_data[$collection_id][$term] = $hits;
        return $hits;

      }
      catch (Exception $e) {
        drupal_set_message($e->getMessage(), 'error');
      }
    }
    else {
      return $term_data[$collection_id][$term];
    }
  }

  /**
   * For a given set of fields, get the specific index fields with properties.
   *
   * @param null $filters
   *
   * @return array
   */
  protected function getFieldMappings($filters = NULL) {
    $params = array('index' => $this->indexName);
    $mappings = $this->client->indices()->getMapping($params);

    $propertiesWrapper = array_pop($mappings[$this->indexName]['mappings']);
    $properties = $propertiesWrapper['properties'];

    $mapping = [];
    if (isset($filters)) {
      foreach ($filters as $filtername => $data) {
        $mapping[$filtername]['data'] = $data;
        if (array_key_exists($filtername, $properties)) {
          $mapping[$filtername]['properties'] = $properties[$filtername];
          if (!isset($properties[$filtername]['type'])) {
            $mapping[$filtername]['properties']['type'] = 'nested';
          }
        }
      }
    }
    return $mapping;
  }

  protected function getIndexManagedFields() {
    $params = array('index' => $this->indexName);
    $mappings = $this->client->indices()->getMapping($params);
    $propertiesWrapper = array_pop($mappings[$this->indexName]['mappings']);
    $properties = $propertiesWrapper['properties'];
    // Remove field added via attachment processor.
    unset($properties['attachments_content']);
    // Remove reference id.
    unset($properties['id']);
    // Remove search api property
    unset($properties['search_api_language']);
    return $properties;
  }

  public function getIndexFieldMap($fields, $reference_field = NULL) {
    $mapped_fields = [];
    $index_fields = $this->getIndexManagedFields();
    foreach ($fields as $fieldname => $data) {
      // First check if we have term types, as these will take precedence over the standard type.
      $field_info = field_info_field($fieldname);
      if ($field_info['type'] === 'taxonomy_term_reference') {
        $term_filtername = 'term_' . $fieldname;
        if (array_key_exists($term_filtername, $index_fields)) {
          $mapped_fields[$fieldname] = $data;
          $mapped_fields[$fieldname]['index_field_name'] = $term_filtername;
          continue;
        }
      }
      // If we have a literal then we can move forwards with it.
      if (array_key_exists($fieldname, $index_fields)) {
        $mapped_fields[$fieldname] = $data;
        $mapped_fields[$fieldname]['index_field_name'] = $fieldname;
        continue;
      }
      if (isset($reference_field)) {
        // If we have a literal from an entity reference.
        $fieldnameFull = $reference_field . ':' . $fieldname;
        if (array_key_exists($fieldnameFull, $index_fields)) {
          $mapped_fields[$fieldname] = $data;
          $mapped_fields[$fieldname]['index_field_name'] = $fieldnameFull;
          continue;
        }
      }
    }
    return $mapped_fields;
  }

  /**
   * Clear cached data from object. Currently unused.
   */
  static function clearIndexCache() {
    $cacheList = [
      'getFacets',
    ];
    foreach ($cacheList as $function) {
      drupal_static_reset(__CLASS__ . '::' . $function);
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
    $entry = array(
      '_score' => $item['_score'],
      'id' => $item['_id'],
      'subtitle' => [],
    );

    foreach($item['_source'] as $field_name => $data) {
      if ($field_name === 'term_field_subtitle') {
        if (isset($data['label']) && isset($data['name'])) {
          $entry['subtitle'] = [
            'label' => $item['_source']['term_field_subtitle']['label'],
            'value' => $item['_source']['term_field_subtitle']['name'],
          ];
          continue;
        }
      }
      if (is_array($data)) {
        if (isset($data[0])) {
          // Nested set of data, merge
          $entry[$field_name] = implode(', ', array_column($data, 'name'));
        }
        else {
          $entry[$field_name] = $data['name'];
        }
      }
      else {
        $entry[$field_name] = $data;
      }
    }


//    if (isset($item['inner_hits']['attachments_content']['hits'])) {
//      foreach ($item['inner_hits']['attachments_content']['hits']['hits'] as $index => $inner_hit) {
//        if (isset($inner_hit['highlight']['attachments_content.data'])) {
//          foreach ($inner_hit['highlight']['attachments_content.data'] as $inner_hit_index => $inner_hit_item) {
//            $entry['page'] = $inner_hit['_source']['page'];
//            $entry['match'] = $inner_hit_item;
//            $this->results[] = $entry;
//          }
//        }
//      }
//    }
  }

  public static function extractSearchResultValues(array $item) {
    $entry = ['subtitle' => []];
    foreach($item as $field_name => $data) {
      if (empty($data)) {
        continue;
      }
      if ($field_name === 'term_field_subtitle') {
        if (isset($data['label']) && isset($data['name'])) {
          $entry['subtitle'] = [
            'label' => $data['label'],
            'value' => $data['name'],
          ];
          continue;
        }
      }
      if (is_array($data)) {
        if (isset($data[0])) {
          // Nested set of data, merge
          $entry[$field_name] = implode(', ', array_column($data, 'name'));
        }
        else {
          $entry[$field_name] = $data['name'];
        }
      }
      else {
        $entry[$field_name] = $data;
      }
    }
    return $entry;
  }
}

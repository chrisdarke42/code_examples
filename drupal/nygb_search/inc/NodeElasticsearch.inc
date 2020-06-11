<?php

class NodeElasticsearch extends BaseElasticsearch {

  protected $collection_data;

  /**
   * NodeElasticsearch constructor.
   *
   * @throws \Exception
   */
  public function __construct() {
    $this->indexName = 'collections';
    parent::__construct();
  }

  public function searchFiltered($terms = array(), $filters = array()) {

    $bodyQuery = array(
      'query' => array(
      ),
      'size' => 100,
    );

    if (count($filters)) {
      $this->buildFilterQueries($filters, $bodyQuery);
    }

    if (count($terms)) {
      $this->buildTermQueries($terms, $bodyQuery);
    }

    $this->debugOutput(json_encode($bodyQuery), 'Node Query');
    $params = $this->getParams($bodyQuery);
    try {
      $response = $this->client->search($params);
      $this->response = $response->getRawResponse();
      $this->updateCollectionDataStorage();
      return $this->getNumberOfHits();
    }
    catch (Exception $e) {
      drupal_set_message($e->getMessage(), 'error');
    }
  }

  protected function updateCollectionDataStorage() {
    $data = $this->parsePrecursorCollectionData();
    if (is_array($this->collection_data)) {
      $this->collection_data += $data;
    }
    else {
      $this->collection_data = $data;
    }
  }

  public function parsePrecursorCollectionData() {
    if (!isset($this->response)) {
      return FALSE;
    }
    $data = [];
    if ($this->response['hits']['total'] > 0) {
      $data = array_column(array_column($this->response['hits']['hits'], '_source'), NULL, 'id');
    }
    return $data;
  }

  /**
   * Build out search query based on filters passed in.
   *
   * @param $filters
   * @param $bodyQuery
   */
  protected function buildFilterQueries($filters, &$bodyQuery) {
    $filters = $this->getFieldMappings($filters);
    foreach ($filters as $filter_name => $filter) {
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
                    $filter_name => $val[$filter_name],
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
        $keyparts = explode(':', $filtername);
        if (count($keyparts) > 1 && $keyparts[0] === 'field_collection') {
          $filtername = $keyparts[1];
        }
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

  protected function getCollectionDataFromElastic($collection_id) {
    $term = array('nid' => $collection_id);
    if ($this->searchFiltered([$term])) {
      return TRUE;
    }
  }

  public function getCollectionData($collection_id) {
    $data = &drupal_static(__CLASS__ . '::' . __FUNCTION__);
    if (!isset($data)) {
      $data = $this->collection_data;
    }
    if (!isset($data[$collection_id]) && $this->getCollectionDataFromElastic($collection_id)) {
      $data[$collection_id] = $this->collection_data[$collection_id];
    }
    return isset($data[$collection_id]) ? $data[$collection_id] : FALSE;
  }

  /**
   * Build out search query based on terms passed in.
   *
   * @param $terms
   * @param $bodyQuery
   */
  protected function buildTermQueries($terms, &$bodyQuery) {
    foreach($terms as $term) {
      $key = key($term);
      $bodyQuery['query']['bool']['must'][] = array('term' => array($key => $term[$key]));
    }
  }

  public function parsePrecursorQueryNids() {
    if (!isset($this->response)) {
      return FALSE;
    }
    $nids = [];
    if ($this->response['hits']['total'] > 0) {
      $nids = array_map(function($hit){return $hit['_source']['id'];}, $this->response['hits']['hits']);
    }
    return $nids;
  }


  public static function extractSearchResultValues(array $item) {
    $entry = [];
    foreach($item as $field_name => $data) {
      if (empty($data)) {
        continue;
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

<?php
/**
 * @file
 * Contains the Locations ElasticSearch Connector implementation class.
 */

/**
 * The base class for Node content.
 */
abstract class NodeElasticsearchConnector extends BaseElasticsearchConnector {

  /**
   * Parse wrapper to return an array for indexing.
   *
   * @param object $node_wrapper
   *   Entity Metadata Wrapper of node.
   *
   * @return array
   *   Array of data for indexing.
   */
  abstract protected function parseWrapperIndexing($node_wrapper);

  /**
   * Batch callback for indexing all data.
   *
   * @param object $context
   *   Batch API context.
   *
   * @return mixed
   *   Results of process.
   */
  abstract public function indexData(&$context);

  /**
   * Abstract function definition for getNodeQuery.
   *
   * @return \SelectQuery
   *   Drupal SelectQuery object.
   */
  abstract protected function getNodeQuery();

  /**
   * Index a set of documents based on Nid.
   *
   * @param array $nids
   *   Array of node ids.
   *
   * @return array
   *   Response data.
   */
  public function indexDocuments(array $nids = array()) {
    if (count($nids)) {
      $documents = [];
      $nodes = node_load_multiple($nids);
      foreach ($nodes as $node) {
        $node_wrapper = entity_metadata_wrapper('node', $node);
        $documents[$node_wrapper->getIdentifier()] = $this->parseWrapperIndexing($node_wrapper);
      }
      try {
        if (count($documents) > 0) {
          if (count($documents) === 1) {
            $this->indexDocument(reset($documents), key($documents));
          }
          else {
            return $this->indexDocumentBulk($documents);
          }
        }
      }
      catch (Exception $e) {
        drupal_set_message($e->getMessage(), 'error');
      }
    }
  }

  /**
   * Index a single document.
   *
   * @param object $document
   *   A document object.
   * @param int $nid
   *   The id of the document.
   */
  protected function indexDocument($document, $nid = NULL) {
    $id = $nid;
    $params = $this->getIndexParams();
    $params['id'] = $id;
    $params['body'] = $document;
    $response = $this->client->index($params);
    watchdog('ZSearchESIndexing', print_r($response, 1));
  }

  /**
   * Bulk Index a group of documents.
   *
   * @param array $documents
   *   Array of documents.
   *
   * @return array
   *   Items parsed.
   */
  protected function indexDocumentBulk(array $documents) {
    $params = $this->getIndexParams();
    foreach ($documents as $nid => $document) {
      $params['body'][] = array('index' => array('_id' => $nid));
      $params['body'][] = $document;
    }

    try {
      $response = $this->client->bulk($params);
      if (!empty($response['errors']) && ($response['errors'] !== FALSE)) {
        foreach ($response['items'] as $item) {
          if (!empty($item['index']['status']) && $item['index']['status'] == '400') {
            watchdog('ZsearchESIndexing', $item['index']['error'], array(), WATCHDOG_ERROR);
          }
        }
        drupal_set_message('Error During Bulk Indexing', 'error');
      }
      return $response['items'];
    }
    catch (Exception $e) {
      drupal_set_message($e->getMessage(), 'error');
    }
  }

  /**
   * Delete a list of documents from the index.
   *
   * @param array $nids
   *   List of node ids.
   *
   * @return array|bool
   *   Array of items deleted, or false if none.
   */
  public function deleteDocuments(array $nids = array()) {
    if (count($nids)) {
      $response = array();
      foreach ($nids as $id) {
        $response[] = $this->deleteDocument($id);
      }
      return $response;
    }
    return FALSE;
  }

  /**
   * Return what ElasticSearch would be sent when indexing a node.
   *
   * @param object $node
   *   The node to be sampled.
   *
   * @return array
   *   The data to be sent to ElasticSearch.
   */
  public function indexDataForNode($node) {
    if ($this->isValidForIndex($node->nid)) {
      $node_wrapper = entity_metadata_wrapper('node', $node);
      $data = $this->parseWrapperIndexing($node_wrapper);
      return $data;
    }
    return 'Content is not valid for ElasticSearch Index';
  }

  /**
   * Check if node is valid for indexing.
   *
   * @param int $nid
   *   Node id.
   *
   * @return bool
   *   Return true if the node is valid to be indexed.
   */
  public function isValidForIndex($nid) {
    $query = $this->getNodeQuery();
    $query->condition('n.nid', $nid);
    $result = $query->execute();
    return ($result->rowCount() > 0);
  }

  /**
   * Return number of nodes that can be indexed.
   *
   * @return mixed
   *   Number of rows.
   */
  protected function getNodeQueryCount() {
    $query = $this->getNodeQuery();
    $result = $query->execute();
    return $result->rowCount();
  }

  /**
   * Return a count of the records that should be indexed.
   *
   * @return mixed
   *   Record count.
   */
  public function getCountIndexable() {
    return $this->getNodeQueryCount();
  }

  /**
   * Return document by id.
   *
   * @param int $id
   *   Unique id.
   *
   * @return array|null
   *   Document or null.
   */
  public function getDocument($id) {
    $params = array(
      'index' => $this->indexName,
      'type' => $this->type,
      'body' => array(
        'query' => array(
          'ids' => array('values' => array($id)),
        ),
      ),
    );
    try {
      $response = $this->client->search($params);
      return $response->getRawResponse();
    }
    catch (Exception $e) {
      drupal_set_message($e->getMessage(), 'error');
    }
    return NULL;
  }

  /**
   * Get long text value from field.
   *
   * @param string $field_name
   *   Field name.
   * @param object $wrapper
   *   EMW wrapper.
   * @param bool $strip_tags
   *   Flag to enable tag stripping.
   *
   * @return string
   *   Field value.
   */
  protected function longTextValue($field_name, $wrapper, $strip_tags = FALSE) {
    $long_text_value = $wrapper->{$field_name}->value();
    $value = isset($long_text_value) ? $long_text_value['value'] : '';
    return $strip_tags ? strip_tags($value) : $value;
  }

  /**
   * Return integer value for field.
   *
   * @param string $field_name
   *   Name of field to be parsed.
   * @param object $wrapper
   *   Wrapper object.
   * @param string|null $parser
   *   Optional parser to process integer value.
   *
   * @return int
   *   Integer value.
   */
  protected function integerValue($field_name, $wrapper, $parser = NULL) {
    $value = $wrapper->{$field_name}->value();
    if (isset($parser)) {
      $function_name = 'parse' . $parser;
      if (method_exists($this, $function_name)) {
        $value = $this->$function_name($value);
      }
    }
    return $value;
  }

  /**
   * Get value from entity reference.
   *
   * @param string $field_name
   *   Field name.
   * @param object $wrapper
   *   EMW wrapper.
   * @param string|null $implode
   *   String to implode array on.
   *
   * @return array|string
   *   Entity Reference values.
   */
  protected function entityReferenceValue($field_name, $wrapper, $implode = NULL) {
    $values = [];
    foreach ($wrapper->{$field_name}->getIterator() as $value_wrapper) {
      $values[] = $value_wrapper->label();
    }
    if (isset($implode)) {
      return implode($implode, $values);
    }
    return $values;
  }

  /**
   * Log a search.
   *
   * @param string $term
   *   Search Term.
   * @param int $page
   *   Page number.
   *
   * @throws \Exception
   */
  protected function logSearch($term, $page = 0) {
    $logger = new SearchLogElasticsearchConnector();
    try {
      $results = $logger->logSearch($term, $this->getLogFormatted(), $this->type, $this->logId, $page);
      $this->logId = $results['_id'];
    }
    catch (Exception $e) {
      drupal_set_message($e->getMessage(), 'error');
    }
  }

  /**
   * Log a Rating.
   *
   * @param int $rating
   *   The rating to log.
   *
   * @return bool
   *   Return true if the rating was successful.
   *
   * @throws \Exception
   */
  public function logRating($rating) {
    if (isset($this->logId)) {
      // Log Rating.
      $logger = new SearchLogElasticsearchConnector();
      return $logger->rateSearch($rating, $this->logId);
    }
  }

  /**
   * Return formatted content for log.
   *
   * @return array
   *   The content to be logged.
   */
  protected function getLogFormatted() {
    return $this->response;
  }

  /**
   * Get the classname of a node if available.
   *
   * @param object $node
   *   Node object.
   *
   * @return null|string
   *   ClassName or null.
   */
  public static function getIndexedClass($node) {
    $connectorName = NULL;
    switch ($node->type) {
      case 'tour':
        $connectorName = 'ToursElasticsearchConnector';
        break;

      case 'location':
        $connectorName = 'LocationsElasticsearchConnector';
        break;
    }
    return $connectorName;
  }

  /**
   * Load the ElasticSearch connector for the given node.
   *
   * @param object $node
   *   Node to be indexed.
   *
   * @return \LocationsElasticsearchConnector|null|\ToursElasticsearchConnector
   *   The ElasticSearch connector or null.
   *
   * @throws \Exception
   */
  public static function loadElasticSearchConnector($node) {
    try {
      $connector = NULL;
      $connectorName = NodeElasticsearchConnector::getIndexedClass($node);
      if (isset($connectorName)) {
        $connector = new $connectorName();
      }
      return $connector;
    }
    catch (Exception $e) {
      drupal_set_message($e->getMessage(), 'error');
      throw new Exception('Cannot run ElasticSearch Operations.');
    }
  }

}

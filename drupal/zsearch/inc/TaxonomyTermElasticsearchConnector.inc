<?php
/**
 * @file
 * Contains the Taxonomy Terms ElasticSearch Connector implementation class.
 */

abstract class TaxonomyTermElasticsearchConnector extends BaseElasticsearchConnector {

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
   * Index a set of documents based on Tid.
   *
   * @param array $tids
   *   Array of term ids.
   *
   * @return array
   *   Response data.
   */
  public function indexDocuments(array $tids = array()) {
    if (count($tids)) {
      $documents = [];
      $terms = taxonomy_term_load_multiple($tids);
      foreach ($terms as $term) {
        $term_wrapper = entity_metadata_wrapper('taxonomy_term', $term);
        $documents[$term_wrapper->getIdentifier()] = $this->parseWrapperIndexing($term_wrapper);
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
   * @param int $tid
   *   The id of the document.
   */
  protected function indexDocument($document, $tid = NULL) {
    $id = $tid;
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
    foreach ($documents as $tid => $document) {
      $params['body'][] = array('index' => array('_id' => $tid));
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
   * @param array $tids
   *   List of term ids.
   */
  public function deleteDocuments(array $tids = array()) {
    if (count($tids)) {
      foreach ($tids as $id) {
        $this->deleteDocument($id);
      }
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
   * Get DB query for terms.
   */
  protected function getTermQuery() {}

  /**
   * Return number of terms that can be indexed.
   *
   * @return mixed
   *   Number of rows.
   */
  protected function getTermQueryCount() {
    $query = $this->getTermQuery();
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
    return $this->getTermQueryCount();
  }

}

<?php

/**
 * @file
 * ZSearch Settings Administration.
 */

/**
 * ZSearch Admin settings form.
 *
 * @param array $form
 *   Form Array.
 * @param array $form_state
 *   Form State Array.
 *
 * @return mixed
 *   The admin form.
 *
 * @throws \Exception
 */
function zsearch_admin_settings(array $form, array &$form_state) {

  $displayed_indices = array('tours', 'locations', 'activities', 'alllocations');

  $boost_fields = [];

  $cluster_id = elasticsearch_connector_get_default_connector();

  try {
    $cluster = elasticsearch_connector_cluster_load($cluster_id);
    if ($cluster_id === '') {
      drupal_set_message(t('Please configure default ElasticSearch cluster !link', array('!link' => l(t('>> here <<'), 'admin/config/elasticsearch-connector/clusters'))));
      return;
    }
    $client = elasticsearch_connector_get_client_by_id($cluster_id);
    $indices = $client->getIndicesStats();
  }
  catch (Exception $e) {
    drupal_set_message($e->getMessage());
    return;
  }

  // Index Data Header.
  $headers = array(
    array('data' => t('Index name')),
    array('data' => t('Docs (ES/DB)')),
    array('data' => t('Size')),
    array('data' => t('Operations')),
  );
  $rows = array();
  foreach ($displayed_indices as $index) {
    $row = array();

    // Get stats for records on db.
    $es_client = BaseElasticsearchConnector::getIndex($index);
    $record_count = 'UNDEFINED';
    $document_count = 'UNDEFINED';
    if ($es_client) {
      $record_count = $es_client->getCountIndexable();
      $document_count = $es_client->getCountIndexed();
    }

    $index_name = 'zsearch_' . $index;

    if ($boostable_fields = $es_client->getBoostableFields()) {
      $boost_fields[$index] = $boostable_fields;
    }

    $count = $document_count . ' / ' . $record_count;
    $size = 'EMPTY';
    $operations = theme('links__ctools_dropbutton', array(
      'links' => array(
        array('title' => t('Re-index'), 'href' => 'admin/config/zicasso/zsearch/reindex/' . $index),
      ),
      'attributes' => array(
        'class' => array('links', 'inline'),
      ),
    ));
    if (isset($indices['indices'][$index_name])) {
      $index_info = $indices['indices'][$index_name];
      $operations = theme('links__ctools_dropbutton', array(
        'links' => array(
          array('title' => t('Re-index'), 'href' => 'admin/config/zicasso/zsearch/reindex/' . $index),
          array('title' => t('Edit'), 'href' => elasticsearch_connector_main_settings_path() . '/clusters/' . $cluster_id . '/indices/' . $index_name . '/edit'),
          array('title' => t('Delete'), 'href' => elasticsearch_connector_main_settings_path() . '/clusters/' . $cluster_id . '/indices/' . $index_name . '/delete'),
        ),
        'attributes' => array(
          'class' => array('links', 'inline'),
        ),
      ));

      $count = $document_count . ' / ' . $record_count;
      $size = format_size($index_info['total']['store']['size_in_bytes']);
    }
    $row[] = $index_name;
    $row[] = $count;
    $row[] = $size;
    $row[] = $operations;
    $rows[$index] = $row;
  }


  $form['zsearch_index_management'] = array(
    '#type' => 'fieldset',
    '#title' => t('Cluster %name active', array('%name' => $cluster->name)),
  );

  $form['zsearch_index_management']['zsearch_es_index_listing'] = array(
    '#type' => 'tableselect',
    '#header' => $headers,
    '#options' => $rows,
    '#weight' => 0,
    '#empty' => t('No content available.'),
  );

  $form['zsearch_index_management']['reindex'] = array(
    '#type' => 'submit',
    '#value' => t('Re-index Selected'),
    '#weight' => 1,
    '#submit' => array('_zsearch_admin_es_reindex_submit'),
  );

  foreach ($boost_fields as $index => $fields) {
    $form['zsearch_boost_index_' . $index] = array(
      '#type' => 'fieldset',
      '#title' => t('%indexname boosted field settings', array('%indexname' => ucwords($index))),
    );
    foreach ($fields as $fieldname => $weight) {
      $index_name = 'zsearch_' . $index;
      $var_name = $index_name . '_' . $fieldname;
      $weight = variable_get($var_name, $weight);
      $field_weight = ($weight > 0) ? 1 / $weight : 0;
      $form['zsearch_boost_index_' . $index][$var_name] = array(
        '#type' => 'textfield',
        '#title' => ucfirst($fieldname),
        '#default_value' => $weight,
        '#size' => 5,
        '#maxlength' => 5,
        '#weight' => $field_weight,
        '#required' => TRUE,
      );
    }
  }

  $form['zsearch_general_settings'] = array(
    '#type' => 'fieldset',
    '#title' => t('General Settings'),
  );

  $form['zsearch_general_settings']['zsearch_es_min_score'] = array(
    '#type' => 'textfield',
    '#title' => t('ElasticSearch Minimum score.'),
    '#default_value' => variable_get('zsearch_es_min_score', 0),
    '#size' => 4,
    '#maxlength' => 4,
    '#weight' => 2,
    '#description' => t("The Minimum Score to include in search results."),
    '#required' => TRUE,
  );

  $form['zsearch_general_settings']['zsearch_es_plain_search_boost'] = array(
    '#type' => 'textfield',
    '#title' => t('ElasticSearch Query Plain Query Boost.'),
    '#default_value' => variable_get('zsearch_es_plain_search_boost', 1),
    '#size' => 4,
    '#maxlength' => 4,
    '#weight' => 3,
    '#description' => t("How much boost should be applied to the plain text query that is mixed with the precursor queries."),
    '#required' => TRUE,
  );

  $form['zsearch_general_settings']['zsearch_es_backup_boost'] = array(
    '#type' => 'textfield',
    '#title' => t('ElasticSearch Query Backup Boost.'),
    '#default_value' => variable_get('zsearch_es_backup_boost', 1),
    '#size' => 4,
    '#maxlength' => 4,
    '#weight' => 3,
    '#description' => t("How much boost should be applied to the backup query to cover all bases when precursor searches return results."),
    '#required' => TRUE,
  );

  $form['zsearch_general_settings']['zsearch_es_results_per_page'] = array(
    '#type' => 'textfield',
    '#title' => t('Number of Results per page.'),
    '#default_value' => variable_get('zsearch_es_results_per_page', 10),
    '#size' => 4,
    '#maxlength' => 4,
    '#weight' => 4,
    '#required' => TRUE,
  );

  $form['zsearch_general_settings']['zsearch_es_max_pages'] = array(
    '#type' => 'textfield',
    '#title' => t('Maximum number of pages of results.'),
    '#default_value' => variable_get('zsearch_es_max_pages', 15),
    '#size' => 4,
    '#maxlength' => 4,
    '#weight' => 5,
    '#required' => TRUE,
  );



  $form['zsearch_general_settings']['zsearch_es_custom_stopwords'] = array(
    '#type' => 'textarea',
    '#title' => t('Words to be stripped from searches.'),
    '#default_value' => variable_get('zsearch_es_custom_stopwords', ''),
    '#weight' => 6,
    '#description' => t("Enter single words, pipe (|) separated, eg. one|two|three"),
    '#required' => FALSE,
  );

  $form['zsearch_general_settings']['zsearch_es_debug'] = array(
    '#type' => 'checkbox',
    '#title' => t('Show Debug Information.'),
    '#weight' => 7,
    '#default_value' => variable_get('zsearch_es_debug', 0),
    '#description' => t("Show debug info on search results, this will also show on anonymous."),
  );

  $form['#attached']['css'][] = drupal_get_path('module', 'zsearch') . '/css/zsearch_tours.css';

  return system_settings_form($form);
}

/**
 * Initiator for Batch API process to re-index.
 *
 * @param string $index
 *   Name of ElasticSearch index to re-index.
 */
function _zsearch_admin_es_batch_process($index) {
  _zsearch_admin_es_batch($index);
  batch_process('admin/config/zicasso/zsearch');
}

/**
 * Reindex Tours Submit button function.
 *
 * @param array $form
 *   Form Array.
 * @param array $form_state
 *   Form State Array.
 */
function _zsearch_admin_es_reindex_submit(array $form, array $form_state) {
  if (isset($form_state['values']['zsearch_es_index_listing']) && count($form_state['values']['zsearch_es_index_listing'])) {
    $types = array();
    foreach ($form_state['values']['zsearch_es_index_listing'] as $value) {
      if ($value !== 0) {
        $types[] = $value;
      }
    }
    _zsearch_admin_es_batch($types);
  }

}

/**
 * Reindex Tours batch config.
 */
function _zsearch_admin_es_batch($types) {
  $operations = array();
  if (!is_array($types)) {
    $types = array($types);
  }

  if (count($types) === 1) {
    $type = reset($types);
    $progress = t('Processing %type content for ElasticSearch.', array('%type' => $type));
    $title = t('Processing ElasticSearch %type batch.', array('%type' => $type));
  }
  else {
    $progress = t('Processing @current of @total ElasticSearch indices.');
    $title = t('Processing ElasticSearch indexes batch operations.');
  }

  foreach ($types as $type) {
    $operations[] = array('zsearch_admin_es_batch_process', array($type));
  }

  $batch = array(
    'operations' => $operations,
    'finished' => 'zsearch_admin_es_batch_finished',
    'title' => $title,
    'init_message' => t('ElasticSearch batch is starting.'),
    'progress_message' => $progress,
    'error_message' => t('ElasticSearch batch has encountered an error.'),
    'file' => drupal_get_path('module', 'zsearch') . '/zsearch.admin.inc',
  );
  batch_set($batch);
}

/**
 * Batch API process.
 *
 * Where the index name loads up the correct index for re-indexing of data.
 *
 * @param string $type
 *   Name of index type.
 * @param array $context
 *   Batch API context.
 */
function zsearch_admin_es_batch_process($type, array &$context) {
  try {
    switch ($type) {
      case 'tours':
        $class = 'ToursElasticsearchConnector';
        break;

      case 'locations':
        $class = 'LocationsElasticsearchConnector';
        break;

      case 'activities':
        $class = 'ActivitiesElasticsearchConnector';
        break;

      case 'alllocations':
        $class = 'AllLocationsElasticsearchConnector';
        break;

      default:
        $class = FALSE;
    }

    if ($class === FALSE) {
      $context['finished'] = 1;
      drupal_set_message(t('No Valid Index for %index', array('%index' => $type)));
      return;
    }

    $indexObject = new $class();
    $indexObject->indexData($context);
  }
  catch (Exception $e) {
    drupal_set_message($e->getMessage(), 'error');
  }
}

/**
 * Reindex Tours batch process.
 */

/**
 * Reindex Tours batch finished callback.
 */
function zsearch_admin_es_batch_finished($success, $results, $operations) {
  if ($success) {
    $message = t('@count items successfully processed.', array('@count' => count($results)));
    drupal_set_message($message);
  }
  else {
    // An error occurred.
    // $operations contains the operations that remained unprocessed.
    $error_operation = reset($operations);
    $message = t(
      'An error occurred while processing %error_operation with arguments: @arguments',
      array(
        '%error_operation' => $error_operation[0],
        '@arguments' => print_r($error_operation[1],
          TRUE
        ),
      )
    );
    drupal_set_message($message, 'error');
  }

}

/**
 * Get ElasticSearch data for node.
 *
 * @param object $node
 *   Node object to get data for.
 *
 * @return string
 *   ElasticSearch information.
 */
function _zsearch_es_node_info($node) {
  $form = drupal_get_form('_zsearch_admin_es_node_info_form', $node);
  return drupal_render($form);
}

/**
 * Form for ElasticSearch information and re-indexing for a node.
 *
 * @param array $form
 *   Form API array.
 * @param array $form_state
 *   Form API form state.
 * @param object $node
 *   Node object.
 *
 * @return mixed
 *   Form Array.
 */
function _zsearch_admin_es_node_info_form(array $form, array &$form_state, $node) {
  $es_stored_data = NULL;
  try {
    if ($index_name = BaseElasticsearchConnector::getIndexName('node', $node)) {
      $es_connector = BaseElasticsearchConnector::getIndex($index_name);
      $es_stored_data = $es_connector->getDocument($node->nid);
    }
  }
  catch (Exception $e) {
    drupal_set_message($e->getMessage(), 'error');
  }
  if (isset($es_stored_data)) {
    $action_value = t('Re-index this @type', array('@type' => $node->type));
    $status = t('This @type is indexed on ElasticSearch', array('@type' => $node->type));
    if ($es_stored_data['hits']['total'] === 0) {
      $action_value = t('Index this @type', array('@type' => $node->type));
      $status = t('This @type is not yet indexed on ElasticSearch', array('@type' => $node->type));
    }
    $form['elastic-info'] = array(
      '#markup' => '<p>' . $status . '</p>',
    );
    // Add button to re-index.
    $form['reindex-nid'] = array(
      '#type' => 'hidden',
      '#value' => $node->nid,
    );
    $form['actions']['reindex_node'] = array(
      '#type' => 'submit',
      '#value' => $action_value,
      '#weight' => 15,
      '#submit' => array('_zsearch_admin_es_reindex_node_submit'),
    );
    $form['elastic-data'] = array(
      '#markup' => '<p>Data format to be passed to ElasticSearch</p>' . '<pre>' . print_r($es_connector->indexDataForNode($node), TRUE) . '</pre>',
    );
  }

  return $form;
}

/**
 * Submit for Node Reindex form, runs indexing of content.
 *
 * @param array $form
 *   Form API array.
 * @param array $form_state
 *   Form API form state.
 */
function _zsearch_admin_es_reindex_node_submit(array $form, array $form_state) {
  $nid = $form_state['input']['reindex-nid'];
  try {
    if ($index_name = BaseElasticsearchConnector::getIndexName('node', node_load($nid))) {
      $es_connector = BaseElasticsearchConnector::getIndex($index_name);
      $es_connector->indexDocuments(array($nid));
      drupal_set_message('Indexed');
    }
  }
  catch (Exception $e) {
    drupal_set_message($e->getMessage(), 'error');
  }
}

/**
 * Test Search form.
 *
 * @param array $form
 *   Form array.
 * @param array $form_state
 *   Form state array.
 *
 * @return mixed
 *   Form array.
 */
function zsearch_es_test_form(array $form, array &$form_state) {
  $params = drupal_get_query_parameters();
  $search_term = '';
  $search = TRUE;
  if (isset($params['search'])) {
    $search_term = filter_xss(trim($params['search']));
  }
  if (isset($form_state['input']) && isset($form_state['input']['keywords'])) {
    $search = FALSE;
  }

  $form['#attached']['js'][] = drupal_get_path('module', 'zsearch') . '/js/zsearch_defer_load.js';
  $form['#attached']['js'][] = drupal_get_path('module', 'zsearch') . '/js/zsearch_rate_results.js';
  $form['#attached']['css'][] = drupal_get_path('module', 'zsearch') . '/css/zsearch_tours.css';

  $devel_access = FALSE;
  // Devel tools for listing.
  if ((module_exists('devel') && user_access('access devel information')) || (variable_get('zsearch_es_debug') === 1)) {
    $devel_access = TRUE;
    $form['#attached']['js'][] = drupal_get_path('module', 'zsearch') . '/js/zsearch_result_explain.js';
  }

  $form['keywords'] = array(
    '#type' => 'textfield',
    '#title' => 'Test',
    '#size' => 60,
    '#maxlength' => 60,
    '#default_value' => $search_term,
    '#required' => FALSE,
  );

  $form['submit_button'] = array(
    '#type' => 'submit',
    '#value' => t('Search Elastic'),
  );

  $form['results'] = array(
    '#type' => 'item',
    '#prefix' => '<div id="search_results">',
    '#suffix' => '</div>',
  );

  if (strlen($search_term) && $search) {
    $page = 0;
    if (isset($params['page']) && is_numeric($params['page'])) {
      $page = $params['page'];
    }

    try {
      $search = new ToursElasticsearchConnector();
      $response = $search->search($search_term, $page);
      $output = '';
      $image = theme_image(array(
        'path' => drupal_get_path('module', 'zsearch') . '/images/placeholder.png',
        'width' => 220,
        'height' => 200,
        'attributes' => array('class' => 'image-placeholder'),
      ));
      if ($response > 0) {
        ToursElasticsearchConnector::saveToCache($search);
        $cacheID = $search->getCacheId();
        $thumbs_up = theme_image(array(
          'path' => drupal_get_path('module', 'zsearch') . '/images/thumbs-up.png',
          'width' => 15,
          'height' => 15,
          'attributes' => array('class' => 'thumbs up'),
        ));
        $thumbs_down = theme_image(array(
          'path' => drupal_get_path('module', 'zsearch') . '/images/thumbs-down.png',
          'width' => 15,
          'height' => 15,
          'attributes' => array('class' => 'thumbs down'),
        ));
        $output .= '<div class="search-rating" data-es-cache-id="' . $cacheID . '">Was this useful?<a href="">' . $thumbs_up . '</a><a href="">' . $thumbs_down . '</a></div>';
        if ($devel_access) {
          $output .= '<div class="eql-area es-devel-area" data-es-cache-id="' . $cacheID . '"><a class="es-devel-button" href="">Location Query</a><pre class="wrapped hidden">' . $search->getQueryJson('locations') . '</pre></div>';
          $locations = $search->getLocations();
          if (count($locations)) {
            $output .= '<div class="eql-area es-devel-area" data-es-cache-id="' . $cacheID . '"><a class="es-devel-button" href="">Locations</a><pre class="wrapped hidden">' . implode('<br>', $locations) . '</pre></div>';
          }
          $output .= '<div class="eql-area es-devel-area" data-es-cache-id="' . $cacheID . '"><a class="es-devel-button" href="">Main Query</a><pre class="wrapped hidden">' . $search->getQueryJson('tours') . '</pre></div>';

          $activities = $search->getActivities();
          if (count($activities)) {
            $output .= '<div class="eql-area es-devel-area" data-es-cache-id="' . $cacheID . '"><a class="es-devel-button" href="">Activities</a><pre class="wrapped hidden">' . implode('<br>', $activities) . '</pre></div>';
          }
        }
        foreach ($search->getResults() as $result) {
          $score = '';
          $explain = '';
          $scoreAdjusted = $search->getScoreAdjusted($result);
          if ($devel_access) {
            $score = ' - ' . $result['_score'] . ' - ' . $scoreAdjusted;
            $explain = '<div class="explain-area es-devel-area not-processed" data-es-cache-id="' . $cacheID . '"><a class="es-devel-button" href="">Explain</a></div>';
          }
          $missing = '';
          if (isset($result['_missing']) && count($result['_missing'])) {
            $missing = '<div class="result__missing">Missing: <span class="result__missing-terms">' . implode(', ', $result['_missing']) . '</span></div>';
          }

          $output .= '<div class="result" data-result-id="' . $result['id'] . '" id="result-' . $result['id'] . '">
                        <div class="deferred thumbnail" data-defer-id="' . $result['id'] . '" data-defer-field="field_photos">' . $image . '</div>
                        <div class="content">
                          <h2>' . l($result['title'], 'node/' . $result['id'], array('attributes' => array('target' => '_blank'))) . $score . '</h2>'
            . $result['summary'] . $missing . $explain .
            '</div>
                      </div>';
        }

        // No point in storing in session if its just one page.
        if ($search->getNumPages() > 1) {
          $_SESSION['search_params'] = $search_term;
        }
        pager_default_initialize($search->getNumberOfHitsForPager(), $search->getResultsPerPage());
        $output .= theme('pager', array('quantity' => $search->getNumberOfHitsForPager()));

      }
      else {
        $output = t('No results');
      }
      $form['results']['#markup'] = $output;
    }
    catch (Exception $e) {
      $form['results']['#markup'] = $e->getMessage();
    }
  }

  return $form;
}

/**
 * Search Tours submit function.
 *
 * @param array $form
 *   Form array.
 * @param array $form_state
 *   Form state array.
 */
function zsearch_es_test_form_submit(array $form, array &$form_state) {
  if (isset($form_state['values']['keywords'])) {
    $params = array();
    $params['search'] = $form_state['values']['keywords'];
    drupal_goto('admin/config/zicasso/zsearch/test', array('query' => $params));
  }
}

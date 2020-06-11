<?php

/**
 * @file
 * NYGB Search Settings Administration.
 */

/**
 * NYGB Search Admin settings form.
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
function nygb_search_admin_settings(array $form, array &$form_state) {


  $cluster_id = elasticsearch_connector_get_default_connector();
  $cluster = elasticsearch_connector_cluster_load($cluster_id);

  if ($cluster_id === '') {
    drupal_set_message(t('Please configure default ElasticSearch cluster !link', array('!link' => l(t('>> here <<'), 'admin/config/elasticsearch-connector/clusters'))));
    return;
  }

  // Collection Index field management:
  try {
    $index = new FileElasticsearch();

    $fields = field_info_instances('node', 'collection');
    $fields['nid'] = ['type' => 'select', 'label' => 'nid'];
    $fields['title'] = ['type' => 'text', 'label' => 'title'];
    $fields = $index->getIndexFieldMap($fields, 'field_collection');
    $selection_options_node = _nygb_search_parameter_options($fields, 'collection');
  }
  catch(Exception $e) {
    drupal_set_message($e->getMessage());
  }


  $form['header'] = array(
    '#markup' => t('<h2>Cluster %name active</h2>', array('%name' => $cluster->name)),
    '#weight' => -1,
  );

  $form['nygb_search_results_uri'] = array(
    '#type' => 'textfield',
    '#title' => t('Search Results uri.'),
    '#default_value' => variable_get('nygb_search_results_uri', 'online-records/search-results'),
    '#size' => 100,
    '#maxlength' => 100,
    '#weight' => 1,
    '#required' => TRUE,
  );

  $form['nygb_search_collection_fields_searchable'] = array(
    '#type' => 'checkboxes',
    '#options' => $selection_options_node,
    '#default_value' => variable_get('nygb_search_collection_fields_searchable', []),
    '#title' => t('Select the fields for search form and search results'),
  );

  $form['nygb_search_min_score'] = array(
    '#type' => 'textfield',
    '#title' => t('ElasticSearch Minimum score.'),
    '#default_value' => variable_get('nygb_search_min_score', 0),
    '#size' => 4,
    '#maxlength' => 4,
    '#weight' => 2,
    '#description' => t("The Minimum Score to include in search results."),
    '#required' => TRUE,
  );

  $form['nygb_search_results_per_page_global'] = array(
    '#type' => 'textfield',
    '#title' => t('Number of Results per page - Global results.'),
    '#default_value' => variable_get('nygb_search_results_per_page_global', 20),
    '#size' => 4,
    '#maxlength' => 4,
    '#weight' => 4,
    '#required' => TRUE,
  );

  $form['nygb_search_results_per_page_collection'] = array(
    '#type' => 'textfield',
    '#title' => t('Number of Results per page - Collection Specific results.'),
    '#default_value' => variable_get('nygb_search_results_per_page_collection', 10),
    '#size' => 4,
    '#maxlength' => 4,
    '#weight' => 4,
    '#required' => TRUE,
  );

  $form['nygb_search_max_pages'] = array(
    '#type' => 'textfield',
    '#title' => t('Maximum number of pages of results.'),
    '#default_value' => variable_get('nygb_search_max_pages', 15),
    '#size' => 4,
    '#maxlength' => 4,
    '#weight' => 5,
    '#required' => TRUE,
  );

  $form['nygb_search_help_text_indexed'] = array(
    '#type' => 'textarea',
    '#title' => t('Help Text for What\'s This tool tip for Indexed Search Results.'),
    '#default_value' => variable_get('nygb_search_help_text_indexed', ''),
    '#weight' => 6,
    '#required' => TRUE,
  );

  $form['nygb_search_help_text_fulltext'] = array(
    '#type' => 'textarea',
    '#title' => t('Help Text for What\'s This tool tip for Fulltext Search Results.'),
    '#default_value' => variable_get('nygb_search_help_text_fulltext', ''),
    '#weight' => 6,
    '#required' => TRUE,
  );

  $form['nygb_search_structured_disabled'] = array(
    '#type' => 'checkbox',
    '#title' => t('Disable Structured Search.'),
    '#weight' => 9,
    '#default_value' => variable_get('nygb_search_structured_disabled', FALSE),
    '#description' => t("Turn off Structured Data search across the side. USE WITH CAUTION."),
  );

  $form['nygb_search_pdf_page_disabled'] = array(
    '#type' => 'checkbox',
    '#title' => t('Disable PDF Page Search.'),
    '#weight' => 9,
    '#default_value' => variable_get('nygb_search_pdf_page_disabled', FALSE),
    '#description' => t("Turn off PDF Page Data search across the side. USE WITH CAUTION."),
  );

  $form['nygb_search_show_all_facets'] = array(
    '#type' => 'checkbox',
    '#title' => t('Show ALL POSSIBLE FACETS.'),
    '#weight' => 8,
    '#default_value' => variable_get('nygb_search_show_all_facets', FALSE),
    '#description' => t("Show all possible facets regardless of search matches. USE WITH CAUTION."),
  );

  $form['nygb_search_debug'] = array(
    '#type' => 'checkbox',
    '#title' => t('Show Debug Information.'),
    '#weight' => 9,
    '#default_value' => variable_get('nygb_search_debug', 0),
    '#description' => t("Show debug info on search results."),
  );

  return system_settings_form($form);
}

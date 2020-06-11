<?php
/**
 * @file
 * nygb_pdf.ds.inc
 */

/**
 * Implements hook_ds_layout_settings_info().
 */
function nygb_pdf_ds_layout_settings_info() {
  $export = array();

  $ds_layout = new stdClass();
  $ds_layout->api_version = 1;
  $ds_layout->id = 'file|collection_file|default';
  $ds_layout->entity_type = 'file';
  $ds_layout->bundle = 'collection_file';
  $ds_layout->view_mode = 'default';
  $ds_layout->layout = 'ds_1col';
  $ds_layout->settings = array(
    'regions' => array(
      'ds_content' => array(
        0 => 'file',
        1 => 'field_collection',
        2 => 'group_admin',
        3 => 'field_id',
        4 => 'field_file_open_access',
        5 => 'field_subtitle_label',
        6 => 'field_subtitle_value',
        7 => 'field_location',
        8 => 'field_creator',
        9 => 'field_date_created',
        10 => 'field_copyright_owner',
        11 => 'field_volume',
        12 => 'field_issue',
        13 => 'field_citation',
        14 => 'field_num_pages',
      ),
    ),
    'fields' => array(
      'file' => 'ds_content',
      'field_collection' => 'ds_content',
      'group_admin' => 'ds_content',
      'field_id' => 'ds_content',
      'field_file_open_access' => 'ds_content',
      'field_subtitle_label' => 'ds_content',
      'field_subtitle_value' => 'ds_content',
      'field_location' => 'ds_content',
      'field_creator' => 'ds_content',
      'field_date_created' => 'ds_content',
      'field_copyright_owner' => 'ds_content',
      'field_volume' => 'ds_content',
      'field_issue' => 'ds_content',
      'field_citation' => 'ds_content',
      'field_num_pages' => 'ds_content',
    ),
    'classes' => array(
      'layout_class' => array(
        '' => '',
      ),
    ),
    'wrappers' => array(
      'ds_content' => 'div',
    ),
    'layout_wrapper' => 'div',
    'layout_attributes' => '',
    'layout_attributes_merge' => 1,
    'layout_link_attribute' => '',
    'layout_link_custom' => '',
    'layout_disable_css' => FALSE,
  );
  $export['file|collection_file|default'] = $ds_layout;

  $ds_layout = new stdClass();
  $ds_layout->api_version = 1;
  $ds_layout->id = 'file|collection_file|single_page';
  $ds_layout->entity_type = 'file';
  $ds_layout->bundle = 'collection_file';
  $ds_layout->view_mode = 'single_page';
  $ds_layout->layout = 'ds_1col';
  $ds_layout->settings = array(
    'regions' => array(
      'ds_content' => array(
        0 => 'file',
      ),
    ),
    'fields' => array(
      'file' => 'ds_content',
    ),
    'classes' => array(),
    'wrappers' => array(),
    'layout_wrapper' => 'div',
    'layout_attributes' => '',
    'layout_attributes_merge' => TRUE,
    'layout_link_attribute' => FALSE,
    'layout_link_custom' => '',
    'layout_disable_css' => FALSE,
  );
  $export['file|collection_file|single_page'] = $ds_layout;

  $ds_layout = new stdClass();
  $ds_layout->api_version = 1;
  $ds_layout->id = 'file|collection_file|single_page';
  $ds_layout->entity_type = 'file';
  $ds_layout->bundle = 'collection_file';
  $ds_layout->view_mode = 'single_page';
  $ds_layout->layout = 'ds_1col';
  $ds_layout->settings = array(
    'regions' => array(
      'ds_content' => array(
        0 => 'file',
      ),
    ),
    'fields' => array(
      'file' => 'ds_content',
    ),
    'classes' => array(),
    'wrappers' => array(),
    'layout_wrapper' => 'div',
    'layout_attributes' => '',
    'layout_attributes_merge' => TRUE,
    'layout_link_attribute' => FALSE,
    'layout_link_custom' => '',
    'layout_disable_css' => FALSE,
  );
  $export['file|collection_file|single_page'] = $ds_layout;

  return $export;
}

/**
 * Implements hook_ds_view_modes_info().
 */
function nygb_pdf_ds_view_modes_info() {
  $export = array();

  $ds_view_mode = new stdClass();
  $ds_view_mode->api_version = 1;
  $ds_view_mode->view_mode = 'single_page';
  $ds_view_mode->label = 'Single Page';
  $ds_view_mode->entities = array(
    'file' => 'file',
  );
  $export['single_page'] = $ds_view_mode;

  return $export;
}

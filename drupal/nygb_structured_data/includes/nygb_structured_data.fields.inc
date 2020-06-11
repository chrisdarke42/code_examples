<?php

function _nygb_structured_data_field_data() {
  $data_fields = array(
    'collection_node' => array(
      'field' => array(
        'field_name' => 'collection_node',
        'cardinality' => 1,
        'type' => 'entityreference',
        'settings' => array(
          'target_type' => 'node',
          'handler_settings' => array('target_bundles' => array('collection')),
        )
      ),
      'instance' => array(
        'field_name' => 'collection_node',
        'label' => 'Collection Node',
        'entity_type' => 'structured_data',
        'bundle' => 'structured_data',
        'required' => TRUE,
        'description' => t('The Collection Node that this structured data is for.'),
      ),
    ),
  );
  return $data_fields;
}

/**
 * Create fields and field instances for structured data entity type.
 */
function _nygb_structured_data_create_extra_fields() {
  // Define field and field instances.
  $data_fields = _nygb_structured_data_field_data();
  foreach ($data_fields as $field_data) {
    $field = $field_data['field'];
    // Create field if not exists.
    if (!field_read_field($field['field_name'], array('include_inactive' => TRUE))) {
      field_create_field($field, 'create field');
    }
    $instance = $field_data['instance'];
    // Create field instance if not exists.
    if (!field_read_instance($instance['entity_type'], $instance['field_name'], $instance['bundle'], array('include_inactive' => TRUE))) {
      field_create_instance($instance);
    }
  }
}

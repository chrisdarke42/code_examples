<?php

/**
 * @file
 * Support for Structured Data destination.
 */


/**
 * Destination class implementing migration into terms.
 */
class MigrateDestinationStructuredData extends MigrateDestinationEntity {

  static public function getKeySchema() {
    return array(
      'sid' => array(
        'type' => 'int',
        'unsigned' => TRUE,
        'description' => 'SID of destination Data',
      ),
    );
  }

  /**
   * Basic initialization
   *
   * @param array $options
   *  Options applied to terms.
   */
  public function __construct(array $options = array()) {
    parent::__construct('structured_data', 'structured_data', $options);
  }

  /**
   * Returns a list of fields available to be mapped for this data.
   *
   * @param Migration $migration
   *  Optionally, the migration containing this destination.
   *
   * @return array
   *  Keys: machine names of the fields (to be passed to addFieldMapping)
   *  Values: Human-friendly descriptions of the fields.
   */
  public function fields($migration = NULL) {
    $fields = array();
    $fields['sid'] = t('Structured Data ID');
    $fields['collection_id'] = t('Collection Node ID');
    $fields['fid'] = t('File ID');
    $fields['pid'] = t('Page ID');
    $fields['data'] = t('JSON Data');
    return $fields;
  }

  /**
   * Delete a migrated structured data entity
   *
   * @param $ids
   *  Array of fields representing the key (in this case, just sid).
   */
  public function rollback(array $key) {
    $sid = reset($key);

    migrate_instrument_start('entity_load');
    if (entity_load('structured_data', array($sid))) {
      migrate_instrument_stop('entity_load');
      migrate_instrument_start('entity_delete');
      $this->prepareRollback($sid);
      $result = (bool) entity_delete('structured_data', $sid);
      $this->completeRollback($sid);
      migrate_instrument_stop('entity_delete');
    }
    else {
      migrate_instrument_stop('entity_load');
      // If it didn't exist, consider this a success
      $result = TRUE;
    }
    return $result;
  }

  /**
   * Delete a batch of items at once.
   *
   * @param $nids
   *  Array of node IDs to be deleted.
   */
  public function bulkRollback(array $sids) {
    migrate_instrument_start('entity_delete_multiple');
    $this->prepareRollback($sids);
    entity_delete_multiple('structured_data', $sids);
    $this->completeRollback($sids);
    migrate_instrument_stop('entity_delete_multiple');
  }

  /**
   * Import a single Structured Data item.
   *
   * @param $item
   *  Item object to build. Prefilled with any fields mapped in the Migration.
   * @param $row
   *  Raw source data object - passed through to prepare/complete handlers.
   *
   * @return array
   *  Array of key fields (nid only in this case) of the node that was saved if
   *  successful. FALSE on failure.
   */
  public function import(stdClass $item, stdClass $row) {

    // Invoke migration prepare handlers
    $this->prepare($item, $row);

    if (isset($item->sid)) {
      $updating = TRUE;
    }
    else {
      $updating = FALSE;
    }

    // Validate field data prior to saving.
    MigrateDestinationEntity::fieldAttachValidate('structured_data', $item);

    migrate_instrument_start('entity_save');
    entity_save('structured_data', $item);
    migrate_instrument_stop('entity_save');

    if (isset($item->sid)) {
      if ($updating) {
        $this->numUpdated++;
      }
      else {
        $this->numCreated++;
      }

      $return = array($item->sid);
    }
    else {
      $return = FALSE;
    }

    $this->complete($item, $row);
    return $return;
  }
}
